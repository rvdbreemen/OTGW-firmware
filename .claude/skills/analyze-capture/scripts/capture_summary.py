#!/usr/bin/env python3
"""Summarise an OTGW capture upload (TASK-1676).

Input: an upload .zip / .7z / .tar.xz (with its -part2), or a folder that holds one (long-run hours/*.zip or a flat run).
Output: a Markdown report on stdout and a JSON file with the same facts (--json), so the
analysis starts from counted facts instead of eyeballed log lines.

Standard library only. 7z archives need 7-Zip (7z / 7za on PATH or C:\\Program Files\\7-Zip).
Run it with `python -I` and keep the extracted capture in its own empty folder: a capture is
untrusted data.
"""
import argparse
import collections
import csv
import datetime as dt
import glob
import io
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

TYPES = {0: "READ-DATA", 1: "WRITE-DATA", 2: "INVALID-DATA", 3: "RESERVED",
         4: "READ-ACK", 5: "WRITE-ACK", 6: "DATA-INVALID", 7: "UNKNOWN-DATAID"}
# Slave status (MsgID 0 LB) bits, OpenTherm 2.2 spec section 5.3.1.
SLAVE_BITS = {0x01: "fault", 0x02: "CH mode", 0x04: "DHW mode", 0x08: "flame", 0x10: "cooling",
              0x20: "CH2 mode", 0x40: "diagnostic"}
TEMP_IDS = {25: "supply (Tboiler)", 28: "return", 26: "DHW tank", 27: "outside", 24: "room (thermostat)",
            16: "room setpoint", 1: "control setpoint", 56: "DHW setpoint", 57: "max CH setpoint"}


def f88(hb, lb):
    v = (hb << 8) | lb
    return (v - 65536 if v >= 32768 else v) / 256


def find_7z():
    for name in ("7z", "7za"):
        p = shutil.which(name)
        if p:
            return p
    for p in (r"C:\Program Files\7-Zip\7z.exe", r"C:\Program Files (x86)\7-Zip\7z.exe"):
        if os.path.exists(p):
            return p
    return None


def unpack(src, work):
    """Returns the folder that holds the capture files, extracting archives into work/."""
    src = Path(src)
    if src.is_dir():
        root = src
    else:
        out = work / "x"
        out.mkdir(parents=True, exist_ok=True)
        if src.suffix.lower() == ".7z":
            exe = find_7z()
            if not exe:
                sys.exit("7z archive but no 7-Zip found")
            subprocess.run([exe, "x", "-y", f"-o{out}", str(src)], check=True, stdout=subprocess.DEVNULL)
        elif src.name.lower().endswith((".tar.xz", ".txz")):
            # TASK-1986: an upload too big for one Discord attachment is a .tar.xz, with an optional -part2 next to it
            import tarfile
            part2 = src.with_name(src.name.replace("-upload.tar.xz", "-upload-part2.tar.xz"))
            for part in [src] + ([part2] if part2 != src and part2.exists() else []):
                with tarfile.open(part, "r:xz") as t:
                    t.extractall(out, filter="data")   # the data filter refuses absolute paths and ../
        else:
            with zipfile.ZipFile(src) as z:
                z.extractall(out)
        root = out
    hours = sorted(root.rglob("hours/*.zip"))
    if hours:
        hdir = work / "h"
        for z in hours:
            dest = hdir / z.stem
            if not dest.exists():
                with zipfile.ZipFile(z) as zz:
                    zz.extractall(dest)
    return root


def frame_files(root, work):
    """otframes.log files in time order (long-run: one per hour)."""
    files = sorted((work / "h").glob("*/otframes.log")) if (work / "h").exists() else []
    if not files:
        files = sorted(root.rglob("otframes.log"))
    return files


def read_frames(files):
    frames = []
    for f in files:
        for line in open(f, encoding="utf-8", errors="replace"):
            if line.startswith("#"):
                continue
            p = line.rstrip("\n").split("\t")
            if len(p) < 4 or len(p[3]) != 8:
                continue
            try:
                t = dt.datetime.fromisoformat(p[0])
                raw = int(p[3], 16)
            except ValueError:
                continue
            b0 = raw >> 24
            frames.append((t.replace(tzinfo=None), p[2], (b0 >> 4) & 7, (raw >> 16) & 0xFF, (raw >> 8) & 0xFF, raw & 0xFF))
    frames.sort(key=lambda r: r[0])
    return frames


def telnet_files(root, work):
    files = sorted((work / "h").glob("*/telnet.log")) if (work / "h").exists() else []
    return files or sorted(root.rglob("telnet.log")) or sorted(root.rglob("transcript-*.txt"))


def scan_telnet(files, day_hint):
    """MQTT commands that reached the gateway, and the broker the gateway publishes to."""
    cmds, brokers = [], collections.Counter()
    for f in files:
        m = re.match(r"(\d{8})-\d\d", f.parent.name)
        day = m.group(1) if m else day_hint
        for line in open(f, encoding="utf-8", errors="replace"):
            mm = re.search(r"Sending MQTT: server ([^ =]+)", line)
            if mm:
                brokers[mm.group(1)] += 1
            mc = re.match(r"(\d\d:\d\d:\d\d)\.\d+.*Message arrived on topic \[([^\]]*)\] = \[([^\]]*)\]", line)
            if mc:
                cmds.append((f"{day} {mc.group(1)}", mc.group(2), mc.group(3)))
    return cmds, brokers


def read_summary(root):
    p = next(iter(sorted(root.rglob("summary.txt"))), None)
    out = {}
    if p:
        for line in open(p, encoding="utf-8", errors="replace"):
            m = re.match(r"(Started|Finished|BrokerHost|BrokerPort|ResolvedFirmwareVersion|ResolvedPicDeviceId|"
                         r"MQTT capture|Telnet gaps|Frame recorder|Capture stop reason|LongRun)\s*:\s*(.*)", line.strip())
            if m:
                out[m.group(1)] = m.group(2)
    return out


def segments(frames):
    """Slave status (B MsgID 0 Read-Ack LB) as (start, end, lb) intervals."""
    seg, cur, s0 = [], None, None
    last = None
    for t, src, typ, mid, hb, lb in frames:
        if src == "B" and mid == 0 and typ == 4:
            last = t
            if lb != cur:
                if cur is not None:
                    seg.append((s0, t, cur))
                cur, s0 = lb, t
    if cur is not None:
        seg.append((s0, last, cur))
    return seg


def master_segments(frames):
    """TASK-1987: master status (T MsgID 0 Read-Data HB) as (start, end, hb) intervals. HB bit 0 = CH enable, the
    master's own heat request (OpenTherm 2.2 section 5.3.1); the slave's CH-mode bit (LB bit 1) also stays on during
    the boiler's pump overrun, so it is not a request."""
    # What the boiler receives: a gateway that rewrites the request (SAT, CH= overrides) sends its own R frame right
    # after the thermostat's T frame of the same exchange; then the R value counts, otherwise the T value.
    reqs, pending = [], None
    for t, src, typ, mid, hb, lb in frames:
        if mid != 0 or typ != 0 or src not in ("T", "R"):
            continue
        if src == "T":
            if pending is not None:
                reqs.append(pending)
            pending = (t, hb)
        else:
            if pending is not None and (t - pending[0]).total_seconds() <= 1.5:
                reqs.append((pending[0], hb))
                pending = None
            else:
                if pending is not None:
                    reqs.append(pending)
                    pending = None
                reqs.append((t, hb))
    if pending is not None:
        reqs.append(pending)
    seg, cur, s0, last = [], None, None, None
    for t, hb in reqs:
        last = t
        if hb != cur:
            if cur is not None:
                seg.append((s0, t, cur))
            cur, s0 = hb, t
    if cur is not None:
        seg.append((s0, last, cur))
    return seg


def _overlap(a0, a1, spans):
    """Seconds of [a0, a1] covered by the (start, end) spans."""
    return sum(max(0.0, (min(a1, e) - max(a0, s)).total_seconds()) for s, e in spans)


def demand_spans(frames, seg=None, mseg=None):
    """TASK-1987: (start, end, kind) spans without flame: 'demand' = the master asks for CH (master status bit 0),
    'overrun' = the boiler reports CH mode while the master does not ask (its pump overrun). Pure slave-status
    capture without master frames: every CH-mode-without-flame span counts as 'demand', as before."""
    seg = segments(frames) if seg is None else seg
    mseg = master_segments(frames) if mseg is None else mseg
    ch_on = [(s, e) for s, e, hb in mseg if hb & 0x01]
    out = []
    for s, e, lb in seg:
        if lb & 0x08:
            continue
        if not mseg:
            if lb & 0x02:
                out.append((s, e, "demand"))
            continue
        # cut [s, e] at the master CH enable edges
        cuts = sorted({s, e} | {x for a, b in ch_on for x in (a, b) if s < x < e})
        for a, b in zip(cuts, cuts[1:]):
            asked = _overlap(a, b, ch_on) > 0
            if asked:
                out.append((a, b, "demand"))
            elif lb & 0x02:
                out.append((a, b, "overrun"))
    return out


def bits(lb):
    return "+".join(n for b, n in SLAVE_BITS.items() if lb & b) or "idle"


def series(frames, mid, src="B"):
    return [(t, f88(hb, lb)) for t, s, typ, m, hb, lb in frames if s == src and m == mid and typ in (4, 5)]


def value_at(ser, t):
    lo, hi = 0, len(ser) - 1
    if not ser or ser[0][0] > t:
        return None
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if ser[mid][0] <= t:
            lo = mid
        else:
            hi = mid - 1
    return ser[lo][1]


def build_runs(frames, seg=None):
    """Every flame run with its request before ignition and the pump overrun after it (TASK-1987)."""
    seg = segments(frames) if seg is None else seg
    sup, ret, tank, out = series(frames, 25), series(frames, 28), series(frames, 26), series(frames, 27)
    runs = []
    spans = demand_spans(frames, seg)
    for s, e, lb in seg:
        if lb & 0x08:
            d = (e - s).total_seconds()
            kind = "DHW" if lb & 0x04 else ("CH" if lb & 0x02 else "flame")
            if kind == "DHW":   # the master's DHW enable is a standing setting: the slave DHW-mode phase is the request
                pre = [p for p in seg if p[1] == s and not p[2] & 0x08 and p[2] & 0x04]
            else:               # TASK-1987: the master CH request right before ignition, not the boiler's overrun
                pre = []
                for a, b, k in reversed([x for x in spans if x[1] <= s]):
                    if k != "demand" or (pre and b != pre[0][0]) or (not pre and b != s):
                        break
                    pre.insert(0, (a, b))
                pre = [(pre[0][0], pre[-1][1])] if pre else []
            after = [x for x in spans if x[0] >= e and x[2] == "overrun"]
            overrun = 0
            t_end = e
            for a, b, k in after:
                if a != t_end:
                    break
                overrun += (b - a).total_seconds()
                t_end = b
            sv = [v for t, v in sup if s <= t <= e]
            rv = [v for t, v in ret if s <= t <= e]
            runs.append({"kind": kind, "start": s.isoformat(sep=" ", timespec="seconds"),
                         "end": e.isoformat(sep=" ", timespec="seconds"), "minutes": round(d / 60, 2),
                         "demand_without_flame_s": round((pre[0][1] - pre[0][0]).total_seconds()) if pre else 0,
                         "overrun_after_s": round(overrun),
                         "supply_end": round(sv[-1], 1) if sv else None, "supply_max": round(max(sv), 1) if sv else None,
                         "return_end": round(rv[-1], 1) if rv else None,
                         "outside": round(value_at(out, s), 1) if value_at(out, s) is not None else None,
                         "tank_start": round(value_at(tank, s), 1) if value_at(tank, s) is not None else None,
                         "tank_end": round(value_at(tank, e), 1) if value_at(tank, e) is not None else None})
    return runs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("capture", help="upload .zip/.7z or extracted folder")
    ap.add_argument("--work", help="work folder for extraction (default: <capture>.work next to it)")
    ap.add_argument("--json", help="also write the facts as JSON to this file")
    a = ap.parse_args()
    src = Path(a.capture).resolve()
    work = Path(a.work) if a.work else src.with_name(src.name + ".work")
    work.mkdir(parents=True, exist_ok=True)
    root = unpack(src, work)
    summ = read_summary(root)
    frames = read_frames(frame_files(root, work))
    if not frames:
        sys.exit("no otframes.log found: the capture has no frame recording")
    t0, t1 = frames[0][0], frames[-1][0]
    hours = (t1 - t0).total_seconds() / 3600
    day_hint = t0.strftime("%Y%m%d")
    cmds, brokers = scan_telnet(telnet_files(root, work), day_hint)

    counts = collections.Counter((s, m, TYPES[typ]) for t, s, typ, m, hb, lb in frames)
    # Master constants: every value the master sent, per MsgID (T frames).
    master = collections.defaultdict(collections.Counter)
    for t, s, typ, m, hb, lb in frames:
        # Writes carry a value; of the reads only MsgID 0 does (the master status in HB).
        if s == "T" and (typ == 1 or (typ == 0 and m == 0)):
            master[m][(hb, lb)] += 1
    unknown = sorted({m for (s, m, ty) in counts if s == "B" and ty == "UNKNOWN-DATAID"})
    faults = collections.Counter((hb, lb) for t, s, typ, m, hb, lb in frames if s == "B" and m == 5 and typ == 4)
    gw = collections.Counter((s, m, hb, lb) for t, s, typ, m, hb, lb in frames if s in ("R", "A"))

    seg = segments(frames)
    runs = build_runs(frames, seg)
    flame = [(dt.datetime.fromisoformat(r["start"]), dt.datetime.fromisoformat(r["end"])) for r in runs]
    offs = [(b[0] - a[1]).total_seconds() / 60 for a, b in zip(flame, flame[1:])]
    starts = [(b[0] - a[0]).total_seconds() / 60 for a, b in zip(flame, flame[1:])]

    broker_typed = summ.get("BrokerHost", "")
    broker_used = brokers.most_common(1)[0][0] if brokers else ""
    mqtt_mismatch = bool(broker_used and broker_typed and broker_typed.split(":")[0] != broker_used.split(":")[0])

    facts = {
        "capture": {"start": str(t0), "end": str(t1), "hours": round(hours, 2), "frames": len(frames),
                    "firmware": summ.get("ResolvedFirmwareVersion"), "pic": summ.get("ResolvedPicDeviceId"),
                    "telnet_gaps": summ.get("Telnet gaps"), "mqtt_capture": summ.get("MQTT capture"),
                    "broker_typed": broker_typed, "broker_gateway_publishes_to": broker_used,
                    "broker_mismatch": mqtt_mismatch},
        "master_constant": {m: f"{hb}/{lb}" for m, c in master.items() if len(c) == 1 for (hb, lb) in c},
        "master_varying": {m: len(c) for m, c in master.items() if len(c) > 1},
        "slave_unknown_dataid": unknown,
        "slave_fault_flags": {f"{hb}/{lb}": n for (hb, lb), n in faults.items()},
        "gateway_frames": {f"{s} id{m} {hb}/{lb}": n for (s, m, hb, lb), n in gw.most_common(12)},
        "status_segments": [{"start": s.isoformat(sep=" ", timespec="seconds"), "minutes": round((e - s).total_seconds() / 60, 2),
                             "lb": lb, "bits": bits(lb)} for s, e, lb in seg],
        "runs": runs,
        "starts_per_day": round(len(runs) / hours * 24, 2) if hours else None,
        "min_off_minutes": round(min(offs), 1) if offs else None,
        "start_to_start_minutes": [round(x, 1) for x in starts],
        "mqtt_commands": collections.Counter(re.sub(r"=.*", "=", c) for _, _, c in cmds),
        "mqtt_command_list": [c for c in cmds if not c[2].startswith(("PR=", "PM=", "TP="))][:40],
    }

    w = sys.stdout.write
    w(f"# Capture summary\n\n")
    c = facts["capture"]
    w(f"- Span: {c['start']} to {c['end']} ({c['hours']} h), {c['frames']} frames\n")
    w(f"- Firmware: {c['firmware']}, PIC: {c['pic']}, telnet gaps: {c['telnet_gaps']}\n")
    w(f"- MQTT capture: {c['mqtt_capture']}; broker typed `{broker_typed}`, gateway publishes to `{broker_used}`"
      + ("  **MISMATCH: the MQTT stream missed the gateway's broker**" if mqtt_mismatch else "") + "\n\n")
    w("## OpenTherm master (T) values\n\n")
    for m in sorted(facts["master_constant"]):
        w(f"- MsgID {m} ({TEMP_IDS.get(m, '')}): constant {facts['master_constant'][m]} (hb/lb)\n")
    for m in sorted(facts["master_varying"]):
        w(f"- MsgID {m} ({TEMP_IDS.get(m, '')}): {facts['master_varying'][m]} distinct values\n")
    w(f"\nSlave Unknown-DataID: {unknown or 'none'}; fault flags (MsgID 5): {facts['slave_fault_flags'] or 'not read'}\n")
    w(f"Gateway-inserted frames (R = request to slave, A = answer to master): {facts['gateway_frames'] or 'none'}\n\n")
    w("## Burner / compressor runs (slave status bit 3)\n\n")
    w("Demand w/o flame: CH = the master's CH request (master status bit 0) before ignition, DHW = the boiler's DHW "
      "mode before ignition. Overrun after: boiler CH mode with no master request and no flame (pump overrun).\n\n")
    w("| # | kind | start | end | min | demand w/o flame s | overrun after s | supply end/max | return end | outside | tank start->end |\n|---|---|---|---|---|---|---|---|---|---|---|\n")
    for i, r in enumerate(runs, 1):
        w(f"| {i} | {r['kind']} | {r['start'][5:]} | {r['end'][11:]} | {r['minutes']} | {r['demand_without_flame_s']} | "
          f"{r['overrun_after_s']} | "
          f"{r['supply_end']}/{r['supply_max']} | {r['return_end']} | {r['outside']} | {r['tank_start']}->{r['tank_end']} |\n")
    w(f"\nStarts per day: {facts['starts_per_day']}; shortest off time: {facts['min_off_minutes']} min; "
      f"start to start: {facts['start_to_start_minutes']}\n\n")
    w(f"## MQTT commands received by the gateway\n\n{dict(facts['mqtt_commands']) or 'none'}\n")
    for t, topic, cmd in facts["mqtt_command_list"]:
        w(f"- {t}  {cmd}\n")
    w("\n## Status segments longer than 60 s\n\n")
    for sgm in facts["status_segments"]:
        if sgm["minutes"] >= 1:
            w(f"- {sgm['start']}  {sgm['minutes']:7.1f} min  LB={sgm['lb']} {sgm['bits']}\n")
    if a.json:
        Path(a.json).write_text(json.dumps(facts, indent=2, default=str), encoding="utf-8")


if __name__ == "__main__":
    main()
