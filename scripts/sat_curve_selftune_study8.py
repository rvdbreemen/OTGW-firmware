"""TASK-1519 round 8: robustness of the chosen rule on a second winter (Robert 2026-10-31: another year, 2-day window).

Rule (entries 163, 165, 167, 171, 172 and rounds 5b-5g): curve integral (setpoint - curve) on clean hours (SAT heating,
no hot water, sun < 50 W/m2, SAT not in PWM, heat source not off / at minimum output, both ways); decision once a day
on the last 2 days; step 0.1 when the mean is beyond +-1.0 C of flow. Same houses as round 5b with the radiator sizes
calibrated on the 2025-26 winter (a house does not change with the weather), so the 2024-25 run is out of sample.
Per winter: a fixed-coefficient sweep 0.8-2.6 (no tuner, seed 1) for the empirical optimum, and the rule vs no tuner
for starts 1.0 / 1.2 / 1.4 / 1.8 / 2.4, 2 seeds, gas / hp30 / hp60, paired.
Run: python scripts/sat_curve_selftune_study8.py --weather <2025-26 json> --weather2 <2024-25 json>
         --krs <round5c_runs.json> --outdir <dir>
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study5c as s5c

def _init(w, krs):
    s5c._init(w, krs); s4.EXCLUDE_PWM = True; s4.EXCLUDE_ATMIN = True

def job(a):
    kind, spec, era, mass, start, seed, src = a; kr = s5b.KRS[f"{era}|{mass}|1.8"]
    r = s4.run((spec, era, mass, kr, round(start - s5b._cb(era, mass, kr), 3), 30, seed, False, src))
    r.update(kind=kind, start=start); return r

def jobs():
    out = [("sweep", ("none",), e, m, round(0.8 + 0.1 * i, 1), 1, src) for i in range(19) for e, m in s5b.HOUSES for src in ("gas", "hp30", "hp60")]
    for spec in (("none",), ("A", 2, 1.0, 1.0, 0.1)):
        out += [("rule", spec, e, m, st, sd, src) for e, m in s5b.HOUSES for st in (1.0, 1.2, 1.4, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; out = arg("--outdir"); res = {}
    for label, w in (("2025-26", arg("--weather")), ("2024-25", arg("--weather2"))):
        with Pool(6, initializer=_init, initargs=(w, krs)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(out, "round8_runs.json"), "w"))
