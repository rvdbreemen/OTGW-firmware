"""TASK-1519 round 7 (Sergeant D entry 170): tuner signal I + D against the curve integral alone.

Model = round 5b (calibrated radiators, EXCLUDE_PWM and EXCLUDE_ATMIN), plus PID_D_HOLD: the PID derivative is held
inside the 0.1 C deadband as in thermo-nova pid.py:205-207. The tuner's own derivative (kd x room slope, kd = 588 kp,
pid.py:92) is computed every step and never held. Signals (study4.SIGNAL): CI = setpoint - curve (entry 163, control);
CI+D = CI + tuner derivative (entry 170); I+D = PID integral + tuner derivative; I = PID integral (deadband-reset,
pid.py:187-189); CI+guard = CI, an hour whose room moved > 0.1 C is not clean. Rule: daily, 2 days, threshold 1 C, step 0.1.
Run: python scripts/sat_curve_selftune_study7.py --weather <json> --krs <round5b_calib_sweep.json> --outdir <dir>
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b

def _init(w, krs):
    s5b._init(w, krs); s5b.KRS = krs; s4.EXCLUDE_ATMIN = True; s4.PID_D_HOLD = True

def job(a):
    spec, sig, era, mass, start, seed, src = a; s4.SIGNAL = sig; kr = s5b.KRS[f"{era}|{mass}"]
    r = s4.run((spec, era, mass, kr, round(start - s5b._cb(era, mass, kr), 3), 30, seed, False, src)); r.update(start=start, signal=sig); return r

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]; out = sys.argv[sys.argv.index("--outdir") + 1]
    krs = json.load(open(sys.argv[sys.argv.index("--krs") + 1]))["krs"]
    t = ("B", 2, 1.0, 0.1, 0.0)
    specs = [(("none",), "CI")] + [(t, s) for s in ("CI", "CI+D", "I+D", "I", "CI+guard")]
    jobs = [(sp, sg, e, m, st, sd, src) for sp, sg in specs for e, m in s5b.HOUSES for st in (1.0, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    with Pool(6, initializer=_init, initargs=(w, krs)) as p: res = p.map(job, jobs, chunksize=2)
    json.dump(res, open(os.path.join(out, "round7_runs.json"), "w")); print("done", len(res))
