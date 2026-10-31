"""TASK-1519 round 5b: calibrated reference, empirical optimum, and the round-5 tuners on it.

Why: round 5 scored tuners against best_coeff2() (study3), which assumes flow = mean radiator temperature + 5 C and no
internal gains. The round-4 hydraulics put the flow at mean + Q/(2 MCP) (about +2.4 C at 3 kW) and the house gets
about 135 W of internal gains plus sun, so "1.8" was 2.5-3 C of flow too high (signal ablation, docs/sat/sim-1519/round5b-round7.md).
Fix: size the radiators per house (kr) so the round-4 hydraulics with mean gains balance at 1.8 at the season mean
(entry 166 anchor), verify by a fixed-coefficient sweep (no tuner), then rerun the KISS tuners with and without
EXCLUDE_ATMIN (hours at minimum output not clean), paired against no tuner.
Run: python scripts/sat_curve_selftune_study5b.py --weather <weather_2025_26.json> --outdir <dir>
"""
import json, os, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5 as s5
from sat_curve_selftune_study import curve, TARGET

HOUSES = [("3_1983-92", "heavy"), ("4_1992-2012", "heavy"), ("5_2015", "heavy"), ("6_BENG2021", "heavy"), ("4_1992-2012", "light")]
GAINS_W = (1200 * 60 + 2 * 600 * 75 + 1500 * 300 / 14) / 1440.0   # mean of the gains schedule in study4.run

def balance_c(era, mass, kr, to):
    Ca, Cw, Gao, Gwo, Heff, K = s3.house(era, mass, kr)
    Q = max(Heff * (TARGET - to) - GAINS_W, 50.0)
    flow = TARGET + (Q / K) ** (1 / 1.3) + Q / (2 * s4.MCP)
    return min((abs(curve(c / 100, to) - flow), c / 100) for c in range(10, 501))[1]

def kr_for(era, mass, to, target=1.8):
    lo, hi = 0.05, 5.0
    for _ in range(40):
        mid = (lo + hi) / 2
        if balance_c(era, mass, mid, to) > target: lo = mid   # bigger radiators -> lower coefficient
        else: hi = mid
    return round((lo + hi) / 2, 3)

def _init(w, krs):
    global KRS
    base._init(w); KRS = krs
    s4.OVERSHOOT_MARGIN = 2.0; s4.MAX_SP_GAS = 62.0; s4.MAX_SP_HP = 40.0; s4.EXCLUDE_PWM = True; s4.make = s5.make5

def _cb(era, mass, kr):
    Ca, Cw, Gao, Gwo, Heff, K = s3.house(era, mass, kr)
    return s3.best_coeff2(Heff, K, statistics.fmean(t for t in base.WEATHER["w"][0] if t < 15))

def sweep_job(a):
    c, era, mass, src = a; kr = KRS[f"{era}|{mass}"]; s4.EXCLUDE_ATMIN = False
    r = s4.run((("none",), era, mass, kr, round(c - _cb(era, mass, kr), 3), 30, 1, False, src)); r["c"] = c; r["kr"] = kr; return r

def tuner_job(a):
    spec, atmin, era, mass, start, seed, src = a; kr = KRS[f"{era}|{mass}"]; s4.EXCLUDE_ATMIN = atmin
    r = s4.run((spec, era, mass, kr, round(start - _cb(era, mass, kr), 3), 30, seed, False, src)); r.update(start=start, atmin=atmin); return r

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]; out = sys.argv[sys.argv.index("--outdir") + 1]
    base._init(w); season = statistics.fmean(t for t in base.WEATHER["w"][0] if t < 15)
    krs = {f"{e}|{m}": kr_for(e, m, season) for e, m in HOUSES}
    print("season", round(season, 2), "gains W", round(GAINS_W), krs, flush=True)
    with Pool(6, initializer=_init, initargs=(w, krs)) as p:
        sweep = p.map(sweep_job, [(round(0.8 + 0.1 * i, 1), e, m, src) for i in range(19) for e, m in HOUSES for src in ("gas", "hp30", "hp60")], chunksize=2)
        tun = [("B", 2, 1.0, 0.1, 0.0), ("B", 3, 1.0, 0.1, 0.0), ("B", 2, 1.0, 0.1, 0.4)]
        specs = [(("none",), False)] + [(t, a) for t in tun for a in (False, True)]
        res = p.map(tuner_job, [(sp, a, e, m, st, sd, src) for sp, a in specs for e, m in HOUSES for st in (1.0, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")], chunksize=2)
    json.dump({"krs": krs, "res": sweep}, open(os.path.join(out, "round5b_calib_sweep.json"), "w"))
    json.dump(res, open(os.path.join(out, "round5b_runs.json"), "w")); print("done", len(sweep), len(res))
