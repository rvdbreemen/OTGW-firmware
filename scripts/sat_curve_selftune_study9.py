"""TASK-1519 round 9: the decided rule over 20 winters (Robert 2026-10-31: "twenty years of runs").

Winters 2005-06 .. 2024-25 (Nov-Feb, Open-Meteo archive, grid cell 52.13N 6.13E, the same cell as rounds 1-8). The
radiator sizes stay as calibrated on 2025-26, so all 20 winters are out of sample. Per winter:
- a coarse fixed-coefficient sweep (no tuner, seed 1, c = 1.0 1.2 1.4 1.6 1.8 2.0 2.4) for the empirical optimum;
- the decided rule (curve integral, clean hours without PWM and minimum output, symmetric 1 C, step 0.1) with a
  2-day window (Robert) and a 3-day window (Q-1519h, entry 168), against no tuner, starts 1.0 1.2 1.4 1.8 2.4,
  2 seeds, gas / hp30 / hp60, paired.
Results are written per winter to <outdir>/<winter>.json; a winter whose file exists is skipped (resumable).
Run: python scripts/sat_curve_selftune_study9.py --weatherdir <dir with weather_YYYY_YY.json> --krs <round5c_runs.json> --outdir <dir>
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study8 as s8

SWEEP_C = (1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4)
STARTS = (1.0, 1.2, 1.4, 1.8, 2.4)

def jobs():
    out = [("sweep", ("none",), e, m, c, 1, src) for c in SWEEP_C for e, m in s5b.HOUSES for src in ("gas", "hp30", "hp60")]
    for spec in (("none",), ("A", 2, 1.0, 1.0, 0.1), ("A", 3, 1.0, 1.0, 0.1)):
        out += [("rule", spec, e, m, st, sd, src) for e, m in s5b.HOUSES for st in STARTS for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; out = arg("--outdir"); os.makedirs(out, exist_ok=True)
    workers = int(arg("--workers")) if "--workers" in sys.argv else 8
    for w in sorted(glob.glob(os.path.join(arg("--weatherdir"), "weather_*.json"))):
        label = os.path.basename(w)[8:-5]; dest = os.path.join(out, f"{label}.json")
        if os.path.exists(dest): print(label, "exists, skipped", flush=True); continue
        with Pool(workers, initializer=s8._init, initargs=(w, krs)) as p: res = p.map(s8.job, jobs(), chunksize=2)
        tmp = dest + ".tmp"; json.dump(res, open(tmp, "w")); os.replace(tmp, dest)
        print(label, "done", len(res), flush=True)
