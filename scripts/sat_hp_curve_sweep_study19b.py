"""TASK-1527 round 19b (2026-11-01): fair comparison for round 19. Weather compensation and a NIBE-style room factor
only work with a curve tuned to the house, as an installer does. Sweep the curve coefficient per house and floor for
those cases (and for the SAT PID as a check), score on the 2025-26 season, then report the best coefficient per case.
Run: python scripts/sat_hp_curve_sweep_study19b.py --season <season json> --outdir <dir> [--workers N]
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_onoff_study13 as h13
import sat_hp_season_study15 as s15

CONTROLS = [("fw", (180, 10, 10)), ("own", 0), ("dm", 120)]
KPS = ("sat", "2.0", "wc")
COEFFS = (0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8)
ERAS = ("4_1992-2012", "5_2015", "6_BENG2021")
SCREEDS = (8.0e6, 3.0e6)

def _init(w): s15._init(w, {})

def job(a):
    v, p, kp, c, era, scr, sd = a
    h13.KP_MODE = kp; h13.SETBACK = None; h13.COEFF = c
    r = h13.run((v, p, era, "heavy", scr, sd)); r.update(par=list(p) if isinstance(p, tuple) else p, kp_mode=kp, coeff=c); return r

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    J = [(v, p, kp, c, e, scr, 1) for v, p in CONTROLS for kp in KPS for c in COEFFS for e in ERAS for scr in SCREEDS]
    with Pool(int(arg("--workers")) if "--workers" in sys.argv else 4, initializer=_init, initargs=(arg("--season"),)) as p:
        res = p.map(job, J, chunksize=2)
    json.dump(res, open(os.path.join(arg("--outdir"), "round19b_curve_sweep_runs.json"), "w"))
    print("done", len(res))
