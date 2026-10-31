"""TASK-1519 round 10: the decided rule with the PID left alone on a coefficient step (ADR-204).

ADR-204 (Must Not: "Reset on a coefficient change") forbids zeroing the PID integral and derivative when the
coefficient changes. Rounds 1-9 zeroed both on every tuner step. This round reruns the decided rule (3-day window,
entry 175) with STEP_KEEPS_PID = True on three winters (2010-11 coldest, 2024-25, 2025-26 calibration) and compares
it pair by pair with the round 9 runs of the same winters (2025-26: rerun here with both settings, because round 9
does not hold that winter).
Run: python scripts/sat_curve_selftune_study10.py --weatherdir <weather20 dir> --weather2526 <json> --krs <round5c_runs.json> --outdir <dir>
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study8 as s8

def _init(w, krs, keep):
    s8._init(w, krs); s4.STEP_KEEPS_PID = keep

def jobs():
    out = []
    for spec in (("none",), ("A", 3, 1.0, 1.0, 0.1)):
        out += [("rule", spec, e, m, st, sd, src) for e, m in s5b.HOUSES for st in (1.0, 1.2, 1.4, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; wd = arg("--weatherdir"); res = {}
    runs = [("2010_11", os.path.join(wd, "weather_2010_11.json"), True), ("2024_25", os.path.join(wd, "weather_2024_25.json"), True),
            ("2025_26", arg("--weather2526"), True), ("2025_26_reset", arg("--weather2526"), False)]
    for label, w, keep in runs:
        with Pool(8, initializer=_init, initargs=(w, krs, keep)) as p: res[label] = p.map(s8.job, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round10_runs.json"), "w"))
