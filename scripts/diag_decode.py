#!/usr/bin/env python3
"""Decode an OTGW diagnostic recording (ADR-213, ids per ADR-214; TASK-1667).

A recording is what GET /api/v2/diag/recording[?since=<block_seq>] downloads: a file header, then
4096-byte blocks. Each block decodes on its own. The encoder is src/OTGW-firmware/diagRecorderStuff.h;
this decoder follows it byte for byte and is checked against that header by tests/test_diag_decode.py.

    python scripts/diag_decode.py rec.bin                       # summary
    python scripts/diag_decode.py rec.bin --lines               # one line per record
    python scripts/diag_decode.py rec.bin --json rec.json       # every record as JSON
    python scripts/diag_decode.py rec.bin --otframes rec.log    # OT frames for POST /api/v2/simulate (ADR-209)
    python scripts/diag_decode.py rec.bin --names topics.txt    # resolve topic/setting ids from a name list
    python scripts/diag_decode.py diff old.bin new.bin          # PIC commands and MQTT publishes compared

Several downloads of one recording (a host that pulls with since=) can be given together; blocks
are merged by block_seq and a block seen twice is kept once.

Standard library only.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

MAGIC = b"OTGWREC\0"
FORMAT_VERSION = 1
BLOCK_HDR = 16
LOST_SEQ = 0xFFFFFFFF
TEXT_CUT = 255
SOURCES = "TBRAE"   # DIAG_T_OT_FRAME + src: 1..5, DIAG_T_OT_REPEAT + src: 6..10
INT16_MIN = -32768

T_END, T_PIC_CMD, T_OTD_CMD, T_RESPONSE = 0, 11, 12, 13
T_MQTT_PUB, T_MQTT_REPEAT, T_MQTT_IN, T_REST_IN = 14, 15, 16, 17
T_WEATHER, T_BLE, T_SETTING, T_REBOOT, T_GAP, T_TIME_SYNC = 18, 19, 20, 21, 22, 23


class FormatError(Exception):
    pass


def name_id(name):
    """diagNameId(): FNV-1a over the ASCII-lowercased name, folded to 16 bits; 0xFFFF is reserved."""
    h = 2166136261
    for c in name.encode("ascii", "replace"):
        if 65 <= c <= 90:
            c += 32
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    i = ((h >> 16) ^ (h & 0xFFFF)) & 0xFFFF
    return 0xFFFE if i == 0xFFFF else i


class Reader:
    def __init__(self, data, pos=0, end=None):
        self.d, self.p, self.end = data, pos, len(data) if end is None else end

    def need(self, n):
        if self.p + n > self.end:
            raise FormatError(f"record runs past its block at offset {self.p}")

    def u8(self):
        self.need(1)
        v = self.d[self.p]
        self.p += 1
        return v

    def u16(self):
        self.need(2)
        v = struct.unpack_from("<H", self.d, self.p)[0]
        self.p += 2
        return v

    def i16(self):
        self.need(2)
        v = struct.unpack_from("<h", self.d, self.p)[0]
        self.p += 2
        return v

    def u32(self):
        self.need(4)
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def varint(self):
        v, shift = 0, 0
        for _ in range(5):
            b = self.u8()
            v |= (b & 0x7F) << shift
            if b < 0x80:
                return v
            shift += 7
        raise FormatError(f"varint longer than 5 bytes at offset {self.p}")

    def text(self):
        n = self.u8()
        cut = n == TEXT_CUT
        n = 254 if cut else n
        self.need(n)
        s = self.d[self.p:self.p + n].decode("utf-8", "replace")
        self.p += n
        return s, cut

    def hdr_str(self):
        n = self.u8()
        self.need(n)
        s = self.d[self.p:self.p + n].decode("utf-8", "replace")
        self.p += n
        return s


def parse_header(data):
    if data[:8] != MAGIC:
        raise FormatError("not a diagnostic recording (magic OTGWREC missing)")
    r = Reader(data, 8)
    h = {"version": r.u16(), "header_len": r.u16(), "block_size": r.u32(), "first": r.u32(), "count": r.u32(),
         "ring_blocks": r.u32(), "boot_count": r.u32()}
    flags = r.u8()
    r.p += 3
    h["psram"] = bool(flags & 1)
    if h["version"] != FORMAT_VERSION:
        raise FormatError(f"format version {h['version']} (this decoder reads {FORMAT_VERSION})")
    h["fw_version"] = r.hdr_str()
    h["githash"] = r.hdr_str()
    h["routes"] = [r.hdr_str() for _ in range(r.u8())] if r.p < h["header_len"] else []
    if r.p != h["header_len"]:
        raise FormatError(f"header length {h['header_len']} but the fields end at {r.p}")
    return h


def decode_block(blk, routes=()):
    """One block: (block info, records). Repeat records are expanded with the value they repeat."""
    if len(blk) < BLOCK_HDR:
        raise FormatError("block shorter than its header")
    seq, up, epoch, first_rec = struct.unpack_from("<IIII", blk, 0)
    info = {"block_seq": seq, "uptime_ms": up, "epoch": epoch, "first_record_seq": first_rec,
            "lost": first_rec == LOST_SEQ}
    recs = []
    if info["lost"]:
        return info, recs
    r = Reader(blk, BLOCK_HDR)
    ms = up
    last_ot = {}   # (src, msgid) -> frame: the encoder's repeat cache is cleared at every block start
    last_mq = {}   # topic id -> value
    rec_seq = first_rec
    while r.p < len(blk):
        t = r.u8()
        if t == T_END:
            break
        ms = (ms + r.varint()) & 0xFFFFFFFF
        e = {"seq": rec_seq, "ms": ms, "block": seq}
        if 1 <= t <= 5:
            frame = struct.unpack(">I", bytes(r.u8() for _ in range(4)))[0]
            src = SOURCES[t - 1]
            last_ot[(src, (frame >> 16) & 0xFF)] = frame
            e.update(kind="ot", src=src, frame=frame)
        elif 6 <= t <= 10:
            src, mid = SOURCES[t - 6], r.u8()
            if (src, mid) not in last_ot:
                raise FormatError(f"OT repeat of {src} MsgID {mid} without an earlier frame in block {seq}")
            e.update(kind="ot", src=src, frame=last_ot[(src, mid)], repeat=True)
        elif t in (T_PIC_CMD, T_OTD_CMD, T_RESPONSE):
            s, cut = r.text()
            e.update(kind={T_PIC_CMD: "pic_cmd", T_OTD_CMD: "otd_cmd", T_RESPONSE: "response"}[t], text=s, cut=cut)
        elif t == T_MQTT_PUB:
            tid = r.u16()
            s, cut = r.text()
            last_mq[tid] = s
            e.update(kind="mqtt_pub", topic_id=tid, value=s, cut=cut)
        elif t == T_MQTT_REPEAT:
            tid = r.u16()
            if tid not in last_mq:
                raise FormatError(f"MQTT repeat of topic {tid:04x} without an earlier publish in block {seq}")
            e.update(kind="mqtt_pub", topic_id=tid, value=last_mq[tid], repeat=True, cut=False)
        elif t == T_MQTT_IN:
            topic, tcut = r.text()
            value, vcut = r.text()
            e.update(kind="mqtt_in", topic=topic, value=value, cut=tcut or vcut)
        elif t == T_REST_IN:
            route, method = r.u8(), r.u8()
            body, cut = r.text()
            e.update(kind="rest_in", route=route, route_name=routes[route] if route < len(routes) else None,
                     method=method, body=body, cut=cut)
        elif t == T_WEATHER:
            e.update(kind="weather", temp=x100(r.i16()), humidity=x100(r.i16()), wind=x100(r.i16()))
        elif t == T_BLE:
            e.update(kind="ble", slot=r.u8(), temp=x100(r.i16()), humidity=x100(r.i16()), battery=r.u8(),
                     rssi=struct.unpack("b", bytes([r.u8()]))[0])
        elif t == T_SETTING:
            sid = r.u16()
            s, cut = r.text()
            e.update(kind="setting", setting_id=sid, value=s, cut=cut)
        elif t == T_REBOOT:
            boot = r.u32()
            reason, _ = r.text()
            crash, _ = r.text()
            e.update(kind="reboot", boot_count=boot, reset_reason=reason, crash=crash)
        elif t == T_GAP:
            e.update(kind="gap", dropped=r.varint())
        elif t == T_TIME_SYNC:
            e.update(kind="time_sync", epoch=r.u32())
        else:
            raise FormatError(f"unknown record type {t} in block {seq} at offset {r.p - 1}")
        recs.append(e)
        rec_seq += 1
    return info, recs


def x100(v):
    return None if v == INT16_MIN else v / 100.0


def decode(paths):
    """Decode one or more downloads of a recording: (header of the first file, blocks, records)."""
    header, blocks = None, {}
    for p in paths:
        data = Path(p).read_bytes()
        h = parse_header(data)
        header = header or h
        off, bs = h["header_len"], h["block_size"]
        if len(data) < off + h["count"] * bs:
            raise FormatError(f"{p}: {h['count']} blocks announced, {(len(data) - off) // bs} present")
        for i in range(h["count"]):
            blk = data[off + i * bs: off + (i + 1) * bs]
            info, recs = decode_block(blk, h["routes"])
            if info["block_seq"] not in blocks or blocks[info["block_seq"]][0]["lost"]:
                blocks[info["block_seq"]] = (info, recs)
    order = [blocks[k] for k in sorted(blocks)]
    records = [r for _, recs in order for r in recs]
    return header, [i for i, _ in order], records


def load_names(path):
    """Topic suffixes / setting fields, one per line, to label ids (ADR-214: the header carries no table)."""
    names = {}
    if path:
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            n = line.strip()
            if n and not n.startswith("#"):
                names.setdefault(name_id(n), n)
    return names


def fmt(rec, names=None):
    """Canonical one-line form; the same strings the C++ fixture generator writes."""
    k = rec["kind"]
    if k == "ot":
        return f"OT{SOURCES.index(rec['src'])} {rec['frame']:08x}"
    if k in ("pic_cmd", "otd_cmd", "response"):
        return {"pic_cmd": "PIC", "otd_cmd": "OTD", "response": "RSP"}[k] + " " + rec["text"] + (" <cut>" if rec["cut"] else "")
    if k == "mqtt_pub":
        label = (names or {}).get(rec["topic_id"])
        return f"MQ {rec['topic_id']:04x}" + (f"({label})" if label else "") + " " + rec["value"] + (" <cut>" if rec["cut"] else "")
    if k == "mqtt_in":
        return f"MQIN {rec['topic']} {rec['value']}" + (" <cut>" if rec["cut"] else "")
    if k == "rest_in":
        return f"REST {rec['route']} {rec['method']} {rec['body']}" + (" <cut>" if rec["cut"] else "")
    if k == "weather":
        return "WX " + " ".join(_x(rec[f]) for f in ("temp", "humidity", "wind"))
    if k == "ble":
        return f"BLE {rec['slot']} {_x(rec['temp'])} {_x(rec['humidity'])} {rec['battery']} {rec['rssi']}"
    if k == "setting":
        label = (names or {}).get(rec["setting_id"])
        return f"SET {rec['setting_id']:04x}" + (f"({label})" if label else "") + " " + rec["value"]
    if k == "reboot":
        return f"REBOOT {rec['boot_count']} {rec['reset_reason']} | {rec['crash']}"
    if k == "gap":
        return f"GAP {rec['dropped']}"
    if k == "time_sync":
        return f"TIME {rec['epoch']}"
    return k


def _x(v):
    return "na" if v is None else str(int(round(v * 100)))


def device_time(ms):
    """ADR-209 device_time HH:MM:SS.uuuuuu from the uptime; the replay only uses the differences."""
    s, frac = divmod(ms % 86400000, 1000)
    return f"{s // 3600:02d}:{s % 3600 // 60:02d}:{s % 60:02d}.{frac * 1000:06d}"


def write_otframes(records, path, header):
    """OT frames as otframes.log-style lines (device_time TAB source TAB frame), which the replay
    behind POST /api/v2/simulate/start plays at the recorded pace (ADR-209). Gaps over 60 s are
    clamped by the replay itself."""
    n = 0
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write(f"# OTGW diagnostic recording {header['fw_version']} {header['githash']}, "
                f"boot {header['boot_count']}: OT frames for /api/v2/simulate (ADR-209)\n")
        for r in records:
            if r["kind"] == "ot":
                f.write(f"{device_time(r['ms'])}\t{r['src']}\t{r['frame']:08X}\n")
                n += 1
    return n


def summary(header, blocks, records):
    kinds = {}
    for r in records:
        kinds[r["kind"]] = kinds.get(r["kind"], 0) + 1
    seqs = [b["block_seq"] for b in blocks]
    missing = sorted(set(range(seqs[0], seqs[-1] + 1)) - set(seqs)) if seqs else []
    lost = [b["block_seq"] for b in blocks if b["lost"]]
    dropped = sum(r["dropped"] for r in records if r["kind"] == "gap")
    span = (records[-1]["ms"] - records[0]["ms"]) / 1000 if records else 0
    lines = [f"recording: firmware {header['fw_version']} ({header['githash']}), boot {header['boot_count']}, "
             f"ring {header['ring_blocks']} blocks{' in PSRAM' if header['psram'] else ''}",
             f"blocks: {len(blocks)} (seq {seqs[0] if seqs else '-'}..{seqs[-1] if seqs else '-'}), "
             f"missing {len(missing)}, lost while downloading {len(lost)}",
             f"records: {len(records)} over {span:.1f} s, dropped by the writer: {dropped}",
             "by kind: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))]
    return "\n".join(lines)


def diff(a_recs, b_recs, names):
    """Compare the PIC/OT-Direct command and MQTT publish sequences (ADR-213 decision 7), in order."""
    import difflib
    pick = lambda recs: [fmt(r, names) for r in recs if r["kind"] in ("pic_cmd", "otd_cmd", "mqtt_pub")]
    a, b = pick(a_recs), pick(b_recs)
    out = list(difflib.unified_diff(a, b, "recorded", "replayed", lineterm="", n=1))
    return out, len(a), len(b)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "diff":
        ap = argparse.ArgumentParser(prog="diag_decode.py diff")
        ap.add_argument("a")
        ap.add_argument("b")
        ap.add_argument("--names")
        a = ap.parse_args(argv[1:])
        names = load_names(a.names)
        out, na, nb = diff(decode([a.a])[2], decode([a.b])[2], names)
        print("\n".join(out) if out else f"same {na} command/publish records")
        return 1 if out else 0
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("files", nargs="+", help="one or more downloads of the same recording")
    ap.add_argument("--lines", action="store_true", help="print every record")
    ap.add_argument("--json", help="write the records as JSON")
    ap.add_argument("--otframes", help="write the OT frames for the replay (ADR-209)")
    ap.add_argument("--names", help="file with topic suffixes / setting fields to label the ids")
    a = ap.parse_args(argv)
    try:
        header, blocks, records = decode(a.files)
    except FormatError as e:
        print(f"diag_decode: {e}", file=sys.stderr)
        return 2
    names = load_names(a.names)
    print(summary(header, blocks, records))
    if a.lines:
        for r in records:
            print(f"{r['ms']:>10} {fmt(r, names)}")
    if a.json:
        Path(a.json).write_text(json.dumps({"header": header, "blocks": blocks, "records": records}, indent=1),
                                encoding="utf-8")
    if a.otframes:
        print(f"otframes: {write_otframes(records, a.otframes, header)} frames -> {a.otframes}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
