"""TASK-1566 round 25: can SAT keep the room inside the comfort band (+-0.3 C) under sun and mild weather?

Strategies, each paired against the baseline on the same house, weather and gains (round 21 library, SAT through the
proven port): 'base' (today), 'cut' (heating stops above target + 0.1 C, resumes below target - 0.1 C), 'f12' / 'f24'
(the curve uses the mean outdoor temperature of the coming 12 / 24 h plus the expected solar gain as degrees, from the
season's own weather: a perfect forecast, the upper bound of forecasting), 'f24cut' (both), 'f24err' (24 h forecast
with a drifting error of sd 1.5 K) and 'f24errcut'.
Score: share of ALL hours after the 14-day warm-up with the room above target + 0.3 C and below target - 0.3 C (per
minute), starts per day, electricity.
Run: python scripts/sat_comfort_band_study25.py --seasondir <dir> --outdir <dir> [--workers N] [--quick]
  --quick: NIBE S2125-8, 2015 house, winter 2024-25 only, both SAT variants.
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_library_study21 as lib

STRATS = {"base": (None, None), "cut": (None, (0.1, -0.1)), "f12": ({"hours": 12}, None), "f24": ({"hours": 24}, None),
          "f24cut": ({"hours": 24}, (0.1, -0.1)), "f24err": ({"hours": 24, "error_k": 1.5}, None),
          "f24errcut": ({"hours": 24, "error_k": 1.5}, (0.1, -0.1)),
          # sweep after the first run: shorter horizon, heavier solar weight, earlier cut
          "f6": ({"hours": 6}, None), "f12s2": ({"hours": 12, "solar": 2.0}, None), "f12s3": ({"hours": 12, "solar": 3.0}, None),
          "f6s3": ({"hours": 6, "solar": 3.0}, None), "cut0": (None, (0.0, -0.2)),
          "f12s3cut0": ({"hours": 12, "solar": 3.0}, (0.0, -0.2)), "f12s3cut1": ({"hours": 12, "solar": 3.0}, (0.1, -0.1))}
SWEEP = ("f6", "f12s2", "f12s3", "f6s3", "cut0", "f12s3cut0", "f12s3cut1")
UNITS = ("mitsubishi_ecodan_puz_wm50", "nibe_s2125_8", "daikin_altherma3_r_erga04")
WARM = 14 * 1440


def _init(w): lib._init(w)


def job(a):
    strat, unit, variant, era, screed, c = a
    lib.HOUSE = None; lib.PLANT = "ufh"; lib.SEPARATOR = False; lib.HYBRID_MODE = "native"
    lib.PWM_PERIOD_S, lib.PWM_MINON_S, lib.PWM_MINOFF_S = 1800, 600, 600
    lib.ANTICIPATE, lib.ROOM_CUT = STRATS[strat]
    lib.TRACE = []
    r = lib.run((unit, variant, c, era, screed))
    tr = lib.TRACE[WARM:]; lib.TRACE = None; lib.ANTICIPATE = lib.ROOM_CUT = None
    n = len(tr)
    r.update(strategy=strat, above_pct=100 * sum(x[1] > lib.TARGET + 0.3 for x in tr) / n,
             below_pct=100 * sum(x[1] < lib.TARGET - 0.3 for x in tr) / n,
             within_all_pct=100 * sum(abs(x[1] - lib.TARGET) <= 0.3 for x in tr) / n,
             room_max=max(x[1] for x in tr), room_min=min(x[1] for x in tr))
    return {k: v for k, v in r.items() if not isinstance(v, list)}


if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 3
    best = json.load(open(os.path.join(arg("--outdir"), "round21_best_curves.json")))
    if "--quick" in sys.argv:
        J = [(s, "nibe_s2125_8", v, "5_2015", 8.0e6, best[f"nibe_s2125_8|{v}"]) for s in STRATS for v in ("cont", "sat")]
        seasons = [w for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))) if "2024_25" in w]
    else:
        J = [(s, u, v, e, sc, best[f"{u}|{v}"]) for s in STRATS for u in UNITS for v in ("cont", "sat") for e, sc in lib.HOUSES]
        seasons = sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json")))
    tag = "round25_quick" if "--quick" in sys.argv else "round25"
    part = os.path.join(arg("--outdir"), tag + "_parts"); os.makedirs(part, exist_ok=True); res = {}
    for w in seasons:
        label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
        if os.path.exists(dest): res[label] = json.load(open(dest)); continue
        with Pool(workers, initializer=_init, initargs=(w,)) as p: res[label] = p.map(job, J, chunksize=1)
        json.dump(res[label], open(dest, "w")); print(label, "done", flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), tag + "_runs.json"), "w"))
    if "--quick" in sys.argv:
        for r in res[list(res)[0]]:
            print(f"{r['variant']:4s} {r['strategy']:10s} above {r['above_pct']:5.1f}%  below {r['below_pct']:4.1f}%  within {r['within_all_pct']:5.1f}%  "
                  f"max {r['room_max']:.2f}  min {r['room_min']:.2f}  starts {r['starts_per_day']:5.1f}  elec {r['elec_kwh']:5.0f}")
