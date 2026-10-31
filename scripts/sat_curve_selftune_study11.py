"""TASK-1519 round 11: the firmware's overshoot margin (2.0 C, fOvershootMargin, TASK-1268) against thermo-nova's 3.0 C
on the calibrated model, with and without the decided tuner (3-day window, entry 175, PID kept on a step per ADR-204).

Round 5 compared the margins on the uncalibrated model (its reference coefficient was wrong, round 5b). This round
repeats it on the round 5b houses, three winters (2010-11 cold, 2024-25 mild, 2025-26 calibration), starts 1.0 / 1.4 /
1.8 / 2.4, 2 seeds, gas / hp30 / hp60. Scored: minutes per day outside +-0.3 C, burner / compressor starts per day,
short burns (< 5 min) per day, PWM fraction, fuel.
Run: python scripts/sat_curve_selftune_study11.py --weatherdir <weather20 dir> --weather2526 <json> --krs <round5c_runs.json> --outdir <dir>
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study8 as s8

def _init(w, krs, margin):
    s8._init(w, krs); s4.STEP_KEEPS_PID = True; s4.OVERSHOOT_MARGIN = margin

def jobs():
    return [("rule", spec, e, m, st, sd, src) for spec in (("none",), ("A", 3, 1.0, 1.0, 0.1)) for e, m in s5b.HOUSES
            for st in (1.0, 1.4, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; wd = arg("--weatherdir"); res = {}
    winters = [("2010_11", os.path.join(wd, "weather_2010_11.json")), ("2024_25", os.path.join(wd, "weather_2024_25.json")), ("2025_26", arg("--weather2526"))]
    for label, w in winters:
        for margin in (2.0, 3.0):
            with Pool(8, initializer=_init, initargs=(w, krs, margin)) as p:
                res[f"{label}|{margin}"] = p.map(s8.job, jobs(), chunksize=2)
            print(label, margin, "done", flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round11_runs.json"), "w"))
