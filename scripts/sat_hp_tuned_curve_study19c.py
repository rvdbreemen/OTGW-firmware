"""TASK-1527 round 19c (2026-11-01): out-of-sample check of round 19b. The best curve per control mode was picked on
2025-26, seed 1. Here the coefficients are fixed (SAT PWM 0.8 / 1.0, degree minutes 1.4 / 1.6, own control 1.6 / 1.8,
plus 1.8 everywhere as the old baseline) and run on all three seasons and both seeds. Also re-checked at a tuned curve:
the 60 min minimum ON (round 18b/c finding) and the night setback (round 19 finding 4).
Run: python scripts/sat_hp_tuned_curve_study19c.py --seasondir docs/sat/sim-hp-lowload/weather_season --outdir <dir> [--workers N]
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_onoff_study13 as h13
import sat_hp_season_study15 as s15

# (variant, par, coeff, setback)
CASES = ([("fw", (180, 10, 10), c, None) for c in (0.8, 1.0, 1.8)]
         + [("fw", (30, 10, 10), c, None) for c in (0.8, 1.8)]
         + [("fw", (180, 60, 10), c, None) for c in (0.8, 1.8)]
         + [("dm", 120, c, None) for c in (1.4, 1.6, 1.8)]
         + [("own", 0, c, None) for c in (1.6, 1.8)]
         + [("fw", (180, 10, 10), 0.8, (1.0, 23, 6)), ("fw", (180, 10, 10), 0.8, (3.0, 23, 6)),
            ("dm", 120, 1.6, (3.0, 23, 6)), ("own", 0, 1.8, (3.0, 23, 6))])
ERAS = ("4_1992-2012", "5_2015", "6_BENG2021")
SCREEDS = (8.0e6, 3.0e6)

def _init(w): s15._init(w, {})

def job(a):
    v, p, c, sb, era, scr, sd = a
    h13.KP_MODE = "sat"; h13.SETBACK = sb; h13.COEFF = c
    r = h13.run((v, p, era, "heavy", scr, sd))
    r.update(par=list(p) if isinstance(p, tuple) else p, coeff=c, setback=list(sb) if sb else None); return r

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    J = [(v, p, c, sb, e, scr, sd) for v, p, c, sb in CASES for e in ERAS for scr in SCREEDS for sd in (1, 2)]
    res = {}
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]
        with Pool(int(arg("--workers")) if "--workers" in sys.argv else 4, initializer=_init, initargs=(w,)) as p:
            res[label] = p.map(job, J, chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round19c_tuned_curve_runs.json"), "w"))
