#!/usr/bin/env python3
"""Count SAT control-mode flips (PWM <-> continuous) per flame from a telnet log.

Input: a telnet log with SAT debug on (sat flag), i.e. SATTICK lines "... mode=<0|1|2> ... flame=<0|1> ..."
(mode 1 = continuous, 2 = PWM as seen in the sat status control_mode; verify against a short sample first)
plus the switch lines "SAT: ... switching to continuous mode" / "... switching to PWM mode".

Output per flame (flame=1 edge to flame=0 edge): ticks, flips between mode 1 and 2, the switch lines seen,
and the first/last mode. A summary line gives the total flips and the worst flame.
Use:  python -I count_flips.py telnet.log [--min-ticks 3] [--json out.json]
Written for the 25 Nov flip test (request below the flow floor, room + 21.5 C, request 33-35 C); not run on a device yet.
"""
import argparse, json, re, sys

TICK = re.compile(r"SATTICK\b.*?\bmode=(\d+)\b.*?\bflame=(\d)\b")
SW = re.compile(r"switching to (continuous|PWM) mode")
TS = re.compile(r"^(\d\d:\d\d:\d\d(?:\.\d+)?)")


def parse(lines, min_ticks):
    flames, cur, last_flame = [], None, 0
    for ln in lines:
        m = TICK.search(ln)
        if m:
            mode, flame = int(m.group(1)), int(m.group(2))
            t = TS.match(ln)
            t = t.group(1) if t else None
            if flame and not last_flame:
                cur = {"start": t, "ticks": 0, "modes": [], "flips": 0, "switch_lines": []}
            if flame and cur is not None:
                cur["ticks"] += 1
                if cur["modes"] and cur["modes"][-1] != mode and {cur["modes"][-1], mode} == {1, 2}:
                    cur["flips"] += 1
                cur["modes"].append(mode)
            if not flame and last_flame and cur is not None:
                cur["end"] = t
                flames.append(cur)
                cur = None
            last_flame = flame
            continue
        s = SW.search(ln)
        if s and cur is not None:
            t = TS.match(ln)
            cur["switch_lines"].append((t.group(1) if t else None, s.group(1)))
    if cur is not None:
        cur["end"] = None
        flames.append(cur)
    out = []
    for f in flames:
        if f["ticks"] < min_ticks:
            continue
        out.append({"start": f["start"], "end": f["end"], "ticks": f["ticks"], "flips": f["flips"],
                    "first_mode": f["modes"][0], "last_mode": f["modes"][-1], "switch_lines": f["switch_lines"]})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--min-ticks", type=int, default=3)
    ap.add_argument("--json")
    a = ap.parse_args()
    with open(a.log, encoding="utf-8", errors="replace") as fh:
        res = parse(fh, a.min_ticks)
    for r in res:
        print(f"flame {r['start']}..{r['end']} ticks {r['ticks']} flips {r['flips']} modes {r['first_mode']}->{r['last_mode']} switch lines {len(r['switch_lines'])}")
    tot = sum(r["flips"] for r in res)
    worst = max(res, key=lambda r: r["flips"], default=None)
    print(f"flames {len(res)} total flips {tot} worst {worst['flips'] if worst else 0}")
    if a.json:
        with open(a.json, "w") as fh:
            json.dump(res, fh, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
