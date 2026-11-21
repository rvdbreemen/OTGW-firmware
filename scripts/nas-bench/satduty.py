#!/usr/bin/env python3
"""satduty.py <telnet.raw> [base] [window_s]: check the SAT PWM duty in SATTICK lines (TASK-2993/2992 bench).

Per tick (dhw=0 only), mode=2 ticks are checked against duty = clamp((pid - base) / (max(T, base + 1) - base)):
- ON start phase (pwm of the previous tick = 1, or the anchor tick itself), inside the start-up window (<= window_s after the anchor):
  T = BT, the EMA seeded with the flow at the anchor tick and updated after each window tick (0.3 flow + 0.7 BT);
- ON start phase after the window: T = the live flow of the tick;
- OFF start phase (previous pwm = 0): T = held = the flow of the last tick with an ON start phase in this PWM session
  (no ON tick yet since the PWM entry: the live flow).
Anchor = flame 0 -> 1, or pwm 0 -> 1 while flame = 1. Also: MM=T must appear only when raw > 1.05 (1.01-1.05
is not judged), and every 'PWM saturation' exit needs 300 s of poff=0 mode=2 ticks before it.
Tolerance per tick 0.005 + 0.05*(pid-base)/(T-base)^2 (the logged flow has one decimal). Prints counts and every miss.
"""
import re
import sys
from datetime import datetime

LOG = sys.argv[1]
BASE = float(sys.argv[2]) if len(sys.argv) > 2 else 27.2
WIN = float(sys.argv[3]) if len(sys.argv) > 3 else 31.0
TS = re.compile(r"^(\d\d:\d\d:\d\d\.\d+)")


def secs(ts):
    t = datetime.strptime(ts[:15], "%H:%M:%S.%f")
    return t.hour * 3600 + t.minute * 60 + t.second + t.microsecond / 1e6


ticks, sat_exits = [], []
for line in open(LOG, encoding="utf-8", errors="replace"):
    line = line.replace("\r", "")
    m = TS.match(line)
    if not m:
        continue
    if "PWM saturation (off_time=0 sustained)" in line:
        sat_exits.append(secs(m.group(1)))
    if "SATTICK" not in line or " dhw=0" not in line:
        continue
    f = dict(kv.split("=", 1) for kv in line.split() if "=" in kv and kv.count("=") == 1)
    try:
        ticks.append({"t": secs(m.group(1)), "ts": m.group(1)[:12], "pid": float(f["pid"]), "flow": float(f["flow"]),
                      "duty": float(f["duty"]), "mode": int(f["mode"]), "pwm": int(f["pwm"]), "flame": int(f["flame"]),
                      "mm": f.get("mm", ""), "poff": int(f.get("poff", "0"))})
    except (KeyError, ValueError):
        continue


def clamp(x):
    return 0.0 if x < 0 else 1.0 if x > 1 else x


stats = {"win": 0, "live": 0, "off": 0, "entry": 0, "bad": 0, "mm_bad": 0, "mm_T": 0}
prev, anchor_t, bt, held = None, None, None, None
for k in ticks:
    if prev is not None:
        if k["flame"] == 1 and (prev["flame"] != 1 or (prev["pwm"] != 1 and k["pwm"] == 1)):
            anchor_t, bt = k["t"], k["flow"]
        if k["flame"] != 1:
            anchor_t = None
    if prev is not None and k["mode"] == 2:
        on_phase = prev["pwm"] == 1 or k["t"] == anchor_t   # the anchor tick itself uses the seed (= its own flow)
        if prev["mode"] != 2:   # the PWM entry tick (from continuous): no held flow yet, the live flow
            T, kind = k["flow"], "entry"
            held = None         # a new PWM session: the held flow of an earlier session does not carry over
        elif on_phase:
            if anchor_t is not None and k["t"] - anchor_t <= WIN:
                T, kind = bt, "win"
            else:
                T, kind = k["flow"], "live"
        else:
            T, kind = (held if held is not None else k["flow"]), "off"
        raw = (k["pid"] - BASE) / (max(T, BASE + 1) - BASE)
        d = clamp(raw)
        stats[kind] += 1
        tol = 0.005 + 0.05 * max(k["pid"] - BASE, 0) / (max(T, BASE + 1) - BASE) ** 2   # flow printed with 1 decimal (otgw-dev)
        if abs(d - k["duty"]) > tol:
            stats["bad"] += 1
            print("DUTY %s %s pid=%.2f flow=%.1f T=%.2f duty=%.2f calc=%.3f dt=%s" % (
                k["ts"], kind, k["pid"], k["flow"], T, k["duty"], d,
                "%.3f" % (k["t"] - anchor_t) if anchor_t is not None else "-"))
        if k["mm"] == "T":
            stats["mm_T"] += 1
        if raw > 1.05 and k["mm"] != "T":
            stats["mm_bad"] += 1
            print("MM-MISSING %s raw=%.3f" % (k["ts"], raw))
        if raw < 1.01 and k["mm"] == "T":
            stats["mm_bad"] += 1
            print("MM-EXTRA %s raw=%.3f" % (k["ts"], raw))
    # EMA update after a window tick; held flow from ON-phase ticks
    if anchor_t is not None and bt is not None and k["t"] - anchor_t <= WIN and k["t"] != anchor_t:
        bt = 0.3 * k["flow"] + 0.7 * bt
    if prev is not None and prev["pwm"] == 1:
        held = k["flow"]
    prev = k

sat_bad = 0
for t in sat_exits:
    window = [k for k in ticks if t - 300 <= k["t"] < t]
    if not window or any(k["mode"] != 2 or k["poff"] != 0 for k in window):
        sat_bad += 1
        print("SATURATION-EXIT without 300 s of poff=0 at t=%.0f" % t)
print("ticks %d mode2 checked win=%d live=%d off=%d entry=%d duty-bad=%d mm_T=%d mm-bad=%d saturation-exits=%d sat-bad=%d" % (
    len(ticks), stats["win"], stats["live"], stats["off"], stats["entry"], stats["bad"], stats["mm_T"], stats["mm_bad"], len(sat_exits), sat_bad))
