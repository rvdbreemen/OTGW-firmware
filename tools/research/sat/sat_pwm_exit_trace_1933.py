"""TASK-1933 post-hoc trace (MODEL RESULT): one worst-case run, per-tick log of the PWM ON phases.

Wraps Sat1933.tick of tools/research/sat/sat_pwm_exit_sim_1933.py (not edited) and records, per tick, the request (sp),
the flow, the flame, the PWM phase and the e342 counter. Then, per CH ignition inside a PWM ON phase, the time until
flow >= request: the same measure as the field analysis (rise_after_ignition.py on gruijter's captures).

Usage: python -I tools/research/sat/sat_pwm_exit_trace_1933.py <repo> <out.json> <variant> <class> <era> <emitter> <flow> <pmin> <mm 0|1> <seed>
"""
import json
import os
import sys

repo, out, variant, cls, era, em, fl, pmin, mm, seed = sys.argv[1:11]
sys.path.insert(0, os.path.join(repo, "tools", "research", "sat"))
import sat_pwm_exit_sim_1933 as m  # noqa: E402

rows = []
orig = m.Sat1933.tick


def tick(self, now_s, room, tout, flow, ret, flame):
    was_pwm, was_on = self.pwm, getattr(self, "ph_on", False)
    n_ev = len(self.events)
    sp = orig(self, now_s, room, tout, flow, ret, flame)
    rows.append({"t": now_s, "sp": round(sp, 2), "flow": round(flow, 2), "ret": round(ret, 2), "flame": int(bool(flame)),
                 "pwm": int(was_pwm), "on": int(bool(was_on)), "ex": round(self.ex_s, 1), "cs": round(self.cs, 2),
                 "ev": self.events[n_ev][1] if len(self.events) > n_ev else ""})
    return sp


m.Sat1933.tick = tick
r = m.run_one((variant, 30.0, (cls, era, em, fl, float(pmin), mm == "1", int(seed))))

# per CH ignition inside a PWM ON phase: time until flow >= request (tick resolution 30 s)
rises, fires = [], 0
for i in range(1, len(rows)):
    a, b = rows[i - 1], rows[i]
    if b["pwm"] and b["on"] and b["flame"] and not a["flame"]:
        rise = None
        j = i
        while j < len(rows) and rows[j]["pwm"] and rows[j]["on"] and rows[j]["flame"]:
            if rise is None and rows[j]["flow"] >= rows[j]["sp"]:
                rise = rows[j]["t"] - b["t"]
            j += 1
        rises.append(rise)
fires = sum(1 for x in rows if x["ev"] == "exit_e342")
known = sorted(x for x in rises if x is not None)
summary = {"variant": variant, "run": [cls, era, em, fl, pmin, mm, seed], "toggles_per_day": r["toggles_per_day"],
           "exit_under_per_day": r["exit_under_per_day"], "ignitions_in_pwm_on": len(rises),
           "reached": len(known), "never": sum(1 for x in rises if x is None),
           "within_30s": sum(1 for x in known if x <= 30.0),
           "median_s": known[len(known) // 2] if known else None, "max_s": known[-1] if known else None,
           "e342_exits": fires}
json.dump({"summary": summary, "rows": rows}, open(out, "w"))
print(json.dumps(summary))
