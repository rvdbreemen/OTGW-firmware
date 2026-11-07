#!/usr/bin/env python3
"""TASK-1412.5 AC3: measure the SAT transport delay chain from an OTGW capture.

Reads the frame stream (otframes.log from scripts/otgw_capture.py, or the processOT lines of the telnet log) and the
telnet debug log (a merged capture transcript or plain telnet.log segments), all on the DEVICE clock, and reports:

  1. MsgID 25 (flow temperature) supply: interval between valid boiler Read-Acks (p50/p95/p99/max), excluding
     intervals that cross a capture gap marker, plus Data-Invalid / Unknown-DataID counts.
  2. Age of the flow value at every SAT control tick (SATTICK, or the older "SAT loop:" line):
       age_bus  = tick - true bus arrival of the last valid B/25 frame
       age_esp  = tick - the moment the firmware processed it (what the feed stamp sees)
     and, when the SATTICK line carries fage (ms), the firmware's own figure for comparison.
  3. CS command path on a PIC: SAT insert (addCommandToQueue) -> "Sending to Serial" -> PIC echo "CS: x" ->
     first frame to the boiler with MsgID 1 = new value (R, or T when the thermostat already writes it) ->
     boiler Write-Ack (B/01) with that value. Wire stages only for CS value changes (the 5 s repeats carry the
     same value). Echo latency for every CS send.
  4. OT-Direct: the same chain from 'OTD: cmd "CS=x"' to the first T/01 frame with the value, where present.

Timing model (OTGW-Core.ino delay buffer, ~5856-5867): the firmware processes frame k when frame k+1 arrives, and
the log line of frame k carries the timestamp of that moment. So
  true arrival(k)   = device time on the line of frame k-1
  processed(k)      = device time on the line of frame k  (= when the SAT feed stamp is taken, OTGW-Core.ino ~6089)
An arrival is only trusted when the previous frame line is less than 1.2 s earlier and no gap marker lies between.

Not observable from these logs: when the boiler sampled its sensor, the OT wire time of the frame (~34 ms) and the
PIC-to-ESP serial transfer (~11 ms at 9600 baud). The report says so.

Output is aggregates only (no raw frames), so it can be quoted from private field captures.

Usage:
  python scripts/sat_transport_delay_1412_5.py --label F1104 --otframes <otframes.log> --telnet <transcript.txt>
  python scripts/sat_transport_delay_1412_5.py --label F1030 --telnet <transcript.txt>        (frames from telnet)
  --telnet and --otframes may be given more than once (hourly segments, in time order).
  --json writes the summary as JSON as well.
"""
import argparse
import bisect
import json
import math
import re
import sys

TS_RE = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d{6})\s')
FRAME_TELNET_RE = re.compile(r'processOT\s*\(\s*\d+\):\s+[A-Za-z]+(?: [A-Za-z]+)?\s+([TBRAE])([0-9A-F]{8})\s')
TICK_RE = re.compile(r'satControlLo\(\s*\d+\):\s+(SATTICK\s|SAT loop:)')
INSERT_CS_RE = re.compile(r'CmdQueue: Insert queue in slot\[\d+\]:cmd\[CS=(-?[\d.]+)\]')
SEND_CS_RE = re.compile(r'Sending to Serial \[CS=(-?[\d.]+)\]')
ECHO_CS_RE = re.compile(r'CmdQueue: Checking if command is in queue \[CS: (-?[\d.]+)\]')
DUE_RE = re.compile(r'CmdQueue: Queue slot \[\d+\] due')
OTD_CS_RE = re.compile(r'OTD: cmd "CS=(-?[\d.]+)"')
NOECHO_RE = re.compile(r'CmdQueue: no echo for \[CS=')
KV_RE = re.compile(r'(\w+)=(\S+)')

ARRIVAL_MAX_GAP_S = 1.2      # OT: reply within 0.8 s, next request >= 0.1 s later; a longer gap means a lost line
MATCH_WINDOW_S = 60.0        # look for the wire stages of a CS change at most this long


class Clock:
    """HH:MM:SS.uuuuuu -> monotonic seconds; adds a day when the clock wraps past midnight."""

    def __init__(self):
        self.day = 0
        self.last = None

    def __call__(self, h, m, s, us):
        t = int(h) * 3600 + int(m) * 60 + int(s) + int(us) / 1e6
        if self.last is not None and t + 43200 < self.last:
            self.day += 86400
        self.last = t
        return t + self.day


def f88(value):
    v = value & 0xFFFF
    if v >= 0x8000:
        v -= 0x10000
    return v / 256.0


def pct(xs, p):
    if not xs:
        return None
    s = sorted(xs)
    k = (len(s) - 1) * p / 100.0
    lo, hi = math.floor(k), math.ceil(k)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def dist(xs, scale=1.0, nd=2):
    if not xs:
        return {"n": 0}
    return {"n": len(xs), "min": round(min(xs) * scale, nd), "p50": round(pct(xs, 50) * scale, nd),
            "p95": round(pct(xs, 95) * scale, nd), "p99": round(pct(xs, 99) * scale, nd),
            "max": round(max(xs) * scale, nd)}


def open_text(path):
    return open(path, 'r', encoding='utf-8', errors='replace')


def read_otframes(paths):
    """Frames from otframes.log: list of dicts with t (device s), src, val; gap markers start a new segment."""
    frames = []
    clock = Clock()
    seg = 0
    for p in paths:
        seg += 1                                     # a new file is a new segment as well
        with open_text(p) as fh:
            for line in fh:
                if line.startswith('#'):
                    if 'connected' in line or 'GAP' in line:
                        seg += 1
                    continue
                parts = line.rstrip('\n').split('\t')
                if len(parts) < 4:
                    continue
                m = re.match(r'(\d\d):(\d\d):(\d\d)\.(\d{6})', parts[1])
                if not m or not re.fullmatch(r'[0-9A-Fa-f]{8}', parts[3]):
                    continue
                frames.append({"t": clock(*m.groups()), "src": parts[2], "val": int(parts[3], 16), "seg": seg})
    return frames


def telnet_lines(paths):
    """Yield (t, line) for the telnet debug part of a transcript or plain telnet.log files."""
    clock = Clock()
    for p in paths:
        in_section = None                            # None = no section headers seen yet (plain telnet.log)
        with open_text(p) as fh:
            for line in fh:
                if line.startswith('=== '):
                    in_section = 'TELNET DEBUG' in line
                    continue
                if in_section is False:
                    continue
                m = TS_RE.match(line)
                if not m:
                    continue
                yield clock(*m.groups()), line


def annotate_arrivals(frames):
    """Set arr (true arrival, or None when not trustworthy) and proc for every frame."""
    prev = None
    for f in frames:
        f["proc"] = f["t"]
        f["arr"] = None
        if prev is not None and prev["seg"] == f["seg"] and 0 <= f["t"] - prev["t"] <= ARRIVAL_MAX_GAP_S:
            f["arr"] = prev["t"]
        prev = f


def msgid(f):
    return (f["val"] >> 16) & 0xFF


def mtype(f):
    return (f["val"] >> 28) & 0x7


def analyse(label, frame_paths, telnet_paths):
    out = {"label": label}
    # ---- telnet pass: ticks, command events, and (if no otframes) frames ----
    ticks, inserts, sends, echoes, dues, otd_cmds, noecho = [], [], [], [], [], [], 0
    tel_frames = []
    tel_seg = 1
    last_t = None
    for t, line in telnet_lines(telnet_paths):
        if last_t is not None and t - last_t > 30:   # a telnet silence of >30 s (reconnect) splits frame segments
            tel_seg += 1
        last_t = t
        m = FRAME_TELNET_RE.search(line)
        if m:
            tel_frames.append({"t": t, "src": m.group(1), "val": int(m.group(2), 16), "seg": tel_seg})
            continue
        if 'Telnet' in line and 'connect' in line.lower():
            tel_seg += 1
        if TICK_RE.search(line):
            kv = dict(KV_RE.findall(line))
            fage = kv.get('fage')
            ticks.append({"t": t, "fage": int(fage) if fage and fage.isdigit() else None, "fc": kv.get('fc'),
                          "dhw": kv.get('dhw')})
            continue
        m = INSERT_CS_RE.search(line)
        if m:
            inserts.append((t, float(m.group(1))))
            continue
        m = SEND_CS_RE.search(line)
        if m:
            sends.append((t, float(m.group(1))))
            continue
        m = ECHO_CS_RE.search(line)
        if m:
            echoes.append((t, float(m.group(1))))
            continue
        if DUE_RE.search(line):
            dues.append(t)
            continue
        m = OTD_CS_RE.search(line)
        if m:
            otd_cmds.append((t, float(m.group(1))))
            continue
        if NOECHO_RE.search(line):
            noecho += 1

    if frame_paths:
        frames = read_otframes(frame_paths)
        out["frame_source"] = "otframes.log (device time)"
    else:
        frames = tel_frames
        out["frame_source"] = "telnet processOT lines (device time)"
    annotate_arrivals(frames)
    out["frames"] = len(frames)
    out["frames_cross_check"] = {"otframes_B25": (sum(1 for f in frames if f["src"] == 'B' and msgid(f) == 25)
                                                  if frame_paths else "n/a"),
                                 "telnet_B25": sum(1 for f in tel_frames if f["src"] == 'B' and msgid(f) == 25)}
    if frames:
        out["span_h"] = round((frames[-1]["t"] - frames[0]["t"]) / 3600.0, 2)

    # ---- 1. MsgID 25 supply ----
    b25 = [f for f in frames if f["src"] == 'B' and msgid(f) == 25]
    valid = [f for f in b25 if mtype(f) == 4]
    out["b25"] = {"read_ack": len(valid), "data_invalid": sum(1 for f in b25 if mtype(f) == 6),
                  "unknown_dataid": sum(1 for f in b25 if mtype(f) == 7)}
    iv, iv_gap = [], 0
    for a, b in zip(valid, valid[1:]):
        if a["seg"] != b["seg"]:
            iv_gap += 1
            continue
        iv.append(b["proc"] - a["proc"])
    out["b25_interval_s"] = dist(iv)
    out["b25_interval_s"]["skipped_across_capture_gap"] = iv_gap
    out["b25_interval_s"]["count_gt_30s"] = sum(1 for x in iv if x > 30)
    out["b25_interval_s"]["count_gt_60s"] = sum(1 for x in iv if x > 60)
    lag = [f["proc"] - f["arr"] for f in valid if f["arr"] is not None]
    out["b25_decode_lag_s"] = dist(lag, nd=3)    # true arrival -> processed (delay buffer + loop scheduling)
    out["b25_arrival_untrusted"] = sum(1 for f in valid if f["arr"] is None)
    t01 = [f for f in frames if f["src"] == 'T' and msgid(f) == 1]
    out["t01_interval_s"] = dist([b["t"] - a["t"] for a, b in zip(t01, t01[1:]) if a["seg"] == b["seg"]])

    # ---- 2. age at tick ----
    out["ticks"] = len(ticks)
    out["tick_interval_s"] = dist([b["t"] - a["t"] for a, b in zip(ticks, ticks[1:])])
    age_bus, age_esp, fage_diff, fage_fw = [], [], [], []
    no_sample = 0
    j = -1
    vt = [f["proc"] for f in valid]
    for tk in ticks:
        while j + 1 < len(vt) and vt[j + 1] <= tk["t"]:
            j += 1
        if tk["fage"] is not None and tk["fage"] != 4294967295:
            fage_fw.append(tk["fage"] / 1000.0)
        if j < 0:
            no_sample += 1
            continue
        f = valid[j]
        # a tick far beyond the frame data (capture ended or frame stream gap) is not judged
        if tk["t"] - f["proc"] > 600:
            no_sample += 1
            continue
        age_esp.append(tk["t"] - f["proc"])
        if f["arr"] is not None:
            age_bus.append(tk["t"] - f["arr"])
        if tk["fage"] is not None and tk["fage"] != 4294967295:
            fage_diff.append(tk["fage"] / 1000.0 - (tk["t"] - f["proc"]))
    out["age_at_tick_bus_s"] = dist(age_bus)
    out["age_at_tick_esp_s"] = dist(age_esp)
    for k in ("age_at_tick_bus_s", "age_at_tick_esp_s"):
        xs = age_bus if k.endswith("bus_s") else age_esp
        out[k]["count_ge_30s"] = sum(1 for x in xs if x >= 30)
        out[k]["count_ge_60s"] = sum(1 for x in xs if x >= 60)
    out["fage_firmware_s"] = dist(fage_fw)
    out["fage_minus_esp_age_s"] = dist(fage_diff, nd=3)
    out["ticks_without_flow_sample"] = no_sample
    out["tick_flow_class"] = {}
    for tk in ticks:
        if tk["fc"]:
            out["tick_flow_class"][tk["fc"]] = out["tick_flow_class"].get(tk["fc"], 0) + 1

    # ---- 3. CS command path (PIC) ----
    def first_after(events, t0, pred=lambda e: True, window=MATCH_WINDOW_S):
        for e in events:
            et = e[0] if isinstance(e, tuple) else e
            if et < t0:
                continue
            if et - t0 > window:
                return None
            if pred(e):
                return e
        return None

    def same(a, b):
        return abs(a - b) < 0.01

    # queue wait and echo for every send
    send_wait, echo_lat = [], []
    for (ts, v) in sends:
        e = first_after(echoes, ts, lambda e: same(e[1], v), window=2.0)
        if e:
            echo_lat.append(e[0] - ts)
    for (ti, v) in inserts:
        s = first_after(sends, ti, lambda e: same(e[1], v), window=5.0)
        if s:
            send_wait.append(s[0] - ti)
    out["cs_inserts"] = len(inserts)
    out["cs_sends"] = len(sends)
    out["cs_insert_to_send_s"] = dist(send_wait, nd=3)
    out["cs_send_to_echo_s"] = dist(echo_lat, nd=3)
    out["cs_no_echo_lines"] = noecho
    if not sends and inserts:
        dl = []
        for (ti, v) in inserts:
            d = first_after(dues, ti, window=2.0)
            if d is not None:
                dl.append(d - ti)
        out["cs_insert_to_queue_pick_s"] = dist(dl, nd=3)

    to_boiler = [f for f in frames if msgid(f) == 1 and f["src"] in ('R', 'T') and mtype(f) == 1]
    acks = [f for f in frames if msgid(f) == 1 and f["src"] == 'B' and mtype(f) == 5]
    changes = []
    prev_v = None
    for (ti, v) in inserts:
        if prev_v is None or not same(v, prev_v):
            if prev_v is not None and v > 0:
                changes.append((ti, v, prev_v))
        prev_v = v
    stage = {"insert_to_send": [], "send_to_echo": [], "echo_to_boiler_req": [], "insert_to_boiler_req": [],
             "insert_to_boiler_ack": [], "superseded_before_bus": 0, "superseded_by_revert_to_old_value": 0,
             "not_delivered": 0, "not_delivered_then_revert": 0}
    tb_t = [(f["arr"] if f["arr"] is not None else f["proc"]) for f in to_boiler]
    for (ti, v, old_v) in changes:
        s = first_after(sends, ti, lambda e: same(e[1], v), window=5.0)
        e = first_after(echoes, s[0], lambda x: same(x[1], v), window=2.0) if s else None
        # The value is only expected on the bus until the PIC receives a different CS (on OT-Direct there is no
        # send line: the window is the next command with another value).
        t_from = s[0] if s else ti
        nxt = first_after(sends if sends else inserts, t_from + 1e-6, lambda x: not same(x[1], v))
        # +0.1 s: a frame that completes just after the next send was already on the wire with this value
        t_end = min(nxt[0] + 0.1 if nxt else t_from + MATCH_WINDOW_S, t_from + MATCH_WINDOW_S)
        req = None
        k = bisect.bisect_left(tb_t, ti)
        while k < len(to_boiler) and tb_t[k] <= t_end:
            if abs(f88(to_boiler[k]["val"]) - v) < 0.01:
                req = (tb_t[k], to_boiler[k])
                break
            k += 1
        if req is None:
            # superseded only when the newer CS reached the PIC before the first boiler-bound MsgID 1 frame after
            # this one; a value that missed a MsgID 1 slot and was replaced later was not delivered
            k = bisect.bisect_right(tb_t, e[0] if e else t_from)   # the PIC holds the new CS from its echo on
            first_slot = tb_t[k] if k < len(tb_t) else None
            revert = bool(nxt) and same(nxt[1], old_v)
            if nxt and (first_slot is None or nxt[0] < first_slot):
                stage["superseded_before_bus"] += 1
                if revert:
                    stage["superseded_by_revert_to_old_value"] += 1
            else:
                stage["not_delivered"] += 1
                if revert:
                    stage["not_delivered_then_revert"] += 1
            continue
        ack = None
        for f in acks:
            ft = f["arr"] if f["arr"] is not None else f["proc"]
            if ft < req[0]:
                continue
            if ft - req[0] > 2.0:
                break
            if abs(f88(f["val"]) - v) < 0.01:
                ack = ft
                break
        if s:
            stage["insert_to_send"].append(s[0] - ti)
        if s and e:
            stage["send_to_echo"].append(e[0] - s[0])
            stage["echo_to_boiler_req"].append(req[0] - e[0])
        stage["insert_to_boiler_req"].append(req[0] - ti)
        if ack is not None:
            stage["insert_to_boiler_ack"].append(ack - ti)
    out["cs_changes"] = len(changes)
    out["cs_change_stages_s"] = {k: (dist(v, nd=3) if isinstance(v, list) else v) for k, v in stage.items()}

    # ---- 4. OT-Direct command path ----
    out["otd_cs_cmds"] = len(otd_cmds)
    if otd_cmds:
        lat, miss = [], 0
        prev_v = None
        for (ti, v) in otd_cmds:
            if prev_v is not None and not same(v, prev_v) and v > 0:
                hit = None
                for f in to_boiler:
                    if f["src"] != 'T' or f["proc"] < ti:
                        continue
                    if f["proc"] - ti > MATCH_WINDOW_S:
                        break
                    if abs(f88(f["val"]) - v) < 0.01:
                        hit = f
                        break
                if hit:
                    lat.append((hit["arr"] if hit["arr"] is not None else hit["proc"]) - ti)
                else:
                    miss += 1
            prev_v = v
        out["otd_cs_change_to_bus_s"] = dist(lat, nd=3)
        out["otd_cs_change_not_on_bus"] = miss
    return out


def fmt(d):
    if not d or d.get("n", 0) == 0:
        return "none"
    s = "n=%d p50=%s p95=%s p99=%s max=%s" % (d["n"], d["p50"], d["p95"], d["p99"], d["max"])
    extra = [f"{k}={v}" for k, v in d.items() if k not in ("n", "min", "p50", "p95", "p99", "max")]
    return s + ((" " + " ".join(extra)) if extra else "")


def report(o):
    print(f"== {o['label']} ==")
    print(f"frames: {o['frames']} from {o['frame_source']}, span {o.get('span_h', 0)} h; "
          f"B/25 cross-check otframes={o['frames_cross_check']['otframes_B25']} telnet={o['frames_cross_check']['telnet_B25']}")
    b = o["b25"]
    print(f"MsgID 25 boiler replies: Read-Ack {b['read_ack']}, Data-Invalid {b['data_invalid']}, Unknown-DataID {b['unknown_dataid']}")
    if b["read_ack"] == 0:
        print("  no MsgID 25 Read-Ack on the bus in this capture: no flow supply data")
    print(f"  interval between valid samples [s]: {fmt(o['b25_interval_s'])}")
    print(f"  bus arrival -> firmware processed [s]: {fmt(o['b25_decode_lag_s'])} (untrusted arrivals {o['b25_arrival_untrusted']})")
    print(f"  T/01 (thermostat MsgID 1) interval [s]: {fmt(o['t01_interval_s'])}")
    print(f"SAT ticks: {o['ticks']}, interval [s]: {fmt(o['tick_interval_s'])}; flow class at tick {o['tick_flow_class'] or 'n/a'}")
    print(f"  flow age at tick, from bus arrival [s]: {fmt(o['age_at_tick_bus_s'])}")
    print(f"  flow age at tick, from firmware processing [s]: {fmt(o['age_at_tick_esp_s'])}")
    print(f"  firmware fage at tick [s]: {fmt(o['fage_firmware_s'])}")
    print(f"  fage minus recomputed firmware age [s]: {fmt(o['fage_minus_esp_age_s'])}; ticks without a flow sample {o['ticks_without_flow_sample']}")
    print(f"CS (PIC queue): inserts {o['cs_inserts']}, sends {o['cs_sends']}, no-echo lines {o['cs_no_echo_lines']}")
    print(f"  insert -> Sending to Serial [s]: {fmt(o['cs_insert_to_send_s'])}")
    print(f"  Sending to Serial -> PIC echo [s]: {fmt(o['cs_send_to_echo_s'])}")
    if "cs_insert_to_queue_pick_s" in o:
        print(f"  no serial sends (simulation keeps CS off the bus); insert -> queue pick [s]: {fmt(o['cs_insert_to_queue_pick_s'])}")
    print(f"  CS value changes: {o['cs_changes']}")
    for k, v in o["cs_change_stages_s"].items():
        print(f"    {k}: {fmt(v) if isinstance(v, dict) else v}")
    if o["otd_cs_cmds"]:
        print(f"OT-Direct CS commands: {o['otd_cs_cmds']}; change -> T/01 on the bus [s]: {fmt(o.get('otd_cs_change_to_bus_s'))}; "
              f"changes not seen on the bus {o.get('otd_cs_change_not_on_bus')}")
    else:
        print("OT-Direct CS commands: none in this capture")
    print()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--label', required=True, help='name for this capture in the report (no private identifiers)')
    ap.add_argument('--otframes', action='append', default=[], help='otframes.log (repeatable, in time order)')
    ap.add_argument('--telnet', action='append', required=True, help='transcript or telnet.log (repeatable, in time order)')
    ap.add_argument('--json', help='also write the summary as JSON to this path')
    a = ap.parse_args(argv)
    o = analyse(a.label, a.otframes, a.telnet)
    report(o)
    if a.json:
        with open(a.json, 'w', encoding='utf-8') as fh:
            json.dump(o, fh, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
