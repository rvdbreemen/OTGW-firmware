"""TASK-1527 round 22 (2026-11-01): the boiler backup of a hybrid, native (the heat pump starts the boiler) against
Sergeant D's slave SAT (entries 206/207: the slave takes the heat pump's CS as its requested setpoint and runs SAT's gas
logic, continuous until the overshoot switch, then gas PWM). Modes: "own" (the boiler's own on/off at CS +- 2 K
with a 3 min anti-cycling time, under the heat pump's request) and "slave"; both with the hydraulic separator of entry
208. "native" (no boiler anti-cycling, restart the next minute) was dropped after the first test: with a separator it
restarts every minute, which no real boiler does.

Uses the round 21 library (scripts/sat_hp_library_study21.py, SAT through the proven port) with HYBRID_MODE; the four
hybrid units; houses where the heat pump is too small on cold days: 1983-92 (design load about 7 kW) and 1992-2012
(about 4.8 kW), on underfloor heating (heavy screed) and on radiators (water node 0.6 MJ/K, A); master SAT continuous ("cont", as Sergeant D
proposes); ten heating seasons 2015-16 .. 2024-25; curve 1.6. The underfloor 1992-2012 house is left out (the boiler
hardly runs there).
Run: python scripts/sat_hp_hybrid_backup_study22.py --seasondir docs/sat/sim-hp-lowload/weather_season10 --outdir <dir> [--workers N]
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_library_study21 as lib

HYBRIDS = [u for u, d in lib.UNITS.items() if d["type"] == "hybrid"]
HOUSES = [("ufh", "3_1983-92", 8.0e6), ("ufh", "4_1992-2012", 8.0e6), ("rad", "3_1983-92", 0.6e6), ("rad", "4_1992-2012", 0.6e6)]

def _init(w): lib._init(w)

def job(a):
    mode, un, v, plant, era, scr = a
    lib.HYBRID_MODE = mode; lib.PLANT = plant; lib.SEPARATOR = True   # entry 208: hydraulic separator (A: all four hybrids)
    if "--b" in sys.argv: lib.BOILER_MIN = 5500.0   # round 22b: older boiler minimum, entry 210
    return lib.run((un, v, 1.6, era, scr))

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    # master SAT continuous only (Sergeant D's proposal keeps the master continuous); the ufh 1992-2012 house dropped (no backup)
    J = [(mode, un, "cont", pl, e, s) for mode in ("own", "slave") for un in HYBRIDS
         for pl, e, s in HOUSES if not (pl == "ufh" and e == "4_1992-2012")]
    res = {}
    part = os.path.join(arg("--outdir"), "round22b_parts" if "--b" in sys.argv else "round22_parts"); os.makedirs(part, exist_ok=True)
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
        if os.path.exists(dest): res[label] = json.load(open(dest)); continue
        with Pool(workers, initializer=_init, initargs=(w,)) as p: res[label] = p.map(job, J, chunksize=1)
        json.dump(res[label], open(dest, "w")); print(label, "done", flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round22b_boiler55_runs.json" if "--b" in sys.argv else "round22_hybrid_backup_runs.json"), "w")); print("done")
