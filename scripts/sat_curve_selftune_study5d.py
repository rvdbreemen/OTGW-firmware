"""TASK-1519 round 5d: one-sided PWM exclusion.

Round 5c found the tuner blind in the cliff of low-load houses: in 2015 and BENG houses a curve of 1.0 drives SAT
into PWM (minimum output above the low setpoint), PWM hours are excluded, and the tuner gets 0-20 clean hours per
winter while the room is mostly too cold. Test: exclude PWM (and minimum-output) hours only while the room is above
target. Variants: PWM both + minimum both (round 5b rule); PWM above + minimum both; PWM above + minimum above.
Symmetric threshold 1.0, 2 days, step 0.1, houses balanced at 1.8, starts 1.0 / 1.2 / 1.4 / 1.8 / 2.4.
Run: python scripts/sat_curve_selftune_study5d.py --weather <json> --krs <round5c_runs.json> --outdir <dir>
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study5c as s5c

SPECS = [(True, True), ("above", True), ("above", "above")]

def _init(w, krs): s5c._init(w, krs)

def job(a):
    spec, pwm, atmin, era, mass, start, seed, src = a; s4.EXCLUDE_PWM = pwm; s4.EXCLUDE_ATMIN = atmin; kr = s5b.KRS[f"{era}|{mass}|1.8"]
    r = s4.run((spec, era, mass, kr, round(start - s5b._cb(era, mass, kr), 3), 30, seed, False, src))
    r.update(start=start, pwm_rule=str(pwm), atmin=str(atmin)); return r

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]; out = sys.argv[sys.argv.index("--outdir") + 1]
    krs = json.load(open(sys.argv[sys.argv.index("--krs") + 1]))["krs"]
    if "--cold" in sys.argv: SPECS[:] = [(False, "cold")]   # round 5e: PWM handled inside the "cold" hour rule
    t = ("A", 2, 1.0, 1.0, 0.1)
    specs = [(("none",), True, False)] + [(t, pw, am) for pw, am in SPECS]
    jobs = [(sp, pw, am, e, m, st, sd, src) for sp, pw, am in specs for e, m in s5b.HOUSES for st in (1.0, 1.2, 1.4, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    with Pool(6, initializer=_init, initargs=(w, krs)) as p: res = p.map(job, jobs, chunksize=2)
    name = sys.argv[sys.argv.index("--name") + 1] if "--name" in sys.argv else "round5d_runs.json"
    json.dump(res, open(os.path.join(out, name), "w")); print("done", len(res))
