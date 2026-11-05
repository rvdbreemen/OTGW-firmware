"""TASK-1527 round 18b (2026-11-01): heat-pump PWM as the firmware actually runs it.

Rounds 12-17 modelled "SAT PWM as now" with the ON time fixed at the start of each period. The firmware (and
thermo-nova, pwm.py:135 and :205) re-evaluates the 5-range mapper on every step, so the ON phase follows the room
(entry 196). This round reruns the underfloor on/off heat pump (round 15 plant: K fixed on the reference winter,
full heating seasons 1 Oct .. 30 Apr, 2010-11 / 2013-14 / 2025-26, heavy and light screed, 3 houses, 2 seeds) with
the "fw" variant of sat_hp_onoff_study13.py, including Sergeant D's proposed type (entry 195: period 180 min,
minimum ON 60 min). Radiators are not rerun: their calibration file (round5c_runs.json) is not in the repo.

Run: python tools/research/sat/sat_hp_fw_pwm_study18b.py --seasondir docs/sat/sim-hp-lowload/weather_season --outdir <dir> [--workers N]
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_onoff_study13 as h13
import sat_hp_season_study15 as s15

VARIANTS = [("pwm", 30), ("fw", (30, 3)), ("fw", (40, 3)), ("fw", (120, 3)), ("fw", (180, 3)),
            ("fw", (120, 60)), ("fw", (180, 60)), ("dm", 120), ("own", 0)]
# Round 18c (Robert 2026-11-01, entry 197): a heat pump's minimum ON time is always 10 minutes.
VARIANTS_18C = [("fw", (30, 3)), ("fw", (30, 10)), ("fw", (40, 10)), ("fw", (60, 10)), ("fw", (120, 10)),
                ("fw", (180, 10)), ("fw", (180, 3)), ("fw", (180, 60)), ("dm", 120), ("own", 0)]
# Round 18d (Sergeant D 2026-11-01, entry 198): minimum PWM OFF time 600 s for heat pumps, with minimum ON 600 s.
VARIANTS_18D = [("fw", (30, 10)), ("fw", (30, 10, 10)), ("fw", (40, 10, 10)), ("fw", (60, 10, 10)),
                ("fw", (120, 10, 10)), ("fw", (180, 10)), ("fw", (180, 10, 10))]
ERAS = ("4_1992-2012", "5_2015", "6_BENG2021")
SCREEDS = (8.0e6, 3.0e6)

def _init(w):
    s15._init(w, {})

def job(a):
    v, p, era, scr, sd = a
    r = h13.run((v, p, era, "heavy", scr, sd)); r["par"] = list(p) if isinstance(p, tuple) else p; return r

def jobs(variants=VARIANTS):
    return [(v, p, e, scr, sd) for v, p in variants for e in ERAS for scr in SCREEDS for sd in (1, 2)]

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    r18c = "--round18c" in sys.argv; r18d = "--round18d" in sys.argv
    variants = VARIANTS_18D if r18d else (VARIANTS_18C if r18c else VARIANTS)
    os.makedirs(arg("--outdir"), exist_ok=True); res = {}
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]
        with Pool(workers, initializer=_init, initargs=(w,)) as p:
            res[label] = p.map(job, jobs(variants), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    name = "round18d_minoff10_runs.json" if r18d else ("round18c_minon10_runs.json" if r18c else "round18b_fw_pwm_runs.json")
    json.dump(res, open(os.path.join(arg("--outdir"), name), "w"))
