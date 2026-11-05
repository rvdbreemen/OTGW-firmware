"""TASK-1527 round 19 (2026-11-01): does SAT's design conflict with best-practice control of a heat pump with a large
thermal mass? Two testable points on the round 15/18 plant (on/off heat pump, underfloor heating, 3 houses, heavy and
light screed, full seasons 2010-11 / 2013-14 / 2025-26, 2 seeds):

A. Room feedback strength. SAT's PID: kp = coeff x curve / 4 (about 13 K flow per K room error on underfloor at
   coeff 1.8), plus I and D (SATpid.ino). Best practice for heat pumps is weather compensation with little room
   influence: NIBE "factor system" default 2.0, range 0-6 (F1245 manual menu 1.9.4). Variants: SAT PID, factor 2
   (P only), pure curve.
B. Setback. SAT presets: comfort 21, eco 18, sleep 16, away 15 (SATtypes.h:446-451). Best practice: no setback or at
   most ~2 K with weather compensation (Grant UK). Variants: none, 1 K and 3 K from 23:00 to 06:00 (comfort is scored
   only outside the setback hours; electricity over the whole season).

Controls: SAT PWM as the firmware runs it with 600 s minimum ON and OFF (entries 197/198) at 30 and 180 min, the heat
pump's own control around the SAT setpoint, and degree minutes -120.
Run: python tools/research/sat/sat_hp_bestpractice_study19.py --seasondir docs/sat/sim-hp-lowload/weather_season --outdir <dir> [--workers N]
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_hp_onoff_study13 as h13
import sat_hp_season_study15 as s15

CONTROLS = [("fw", (30, 10, 10)), ("fw", (180, 10, 10)), ("own", 0), ("dm", 120)]
CASES = ([("sat", None), ("2.0", None), ("wc", None)]
         + [("sat", (1.0, 23, 6)), ("sat", (3.0, 23, 6)), ("2.0", (3.0, 23, 6))])
ERAS = ("4_1992-2012", "5_2015", "6_BENG2021")
SCREEDS = (8.0e6, 3.0e6)

def _init(w):
    s15._init(w, {})

def job(a):
    v, p, kp, sb, era, scr, sd = a
    h13.KP_MODE = kp; h13.SETBACK = sb
    r = h13.run((v, p, era, "heavy", scr, sd))
    r.update(par=list(p) if isinstance(p, tuple) else p, kp_mode=kp, setback=list(sb) if sb else None)
    return r

def jobs():
    return [(v, p, kp, sb, e, scr, sd) for v, p in CONTROLS for kp, sb in CASES for e in ERAS for scr in SCREEDS for sd in (1, 2)]

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    os.makedirs(arg("--outdir"), exist_ok=True); res = {}
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]
        with Pool(workers, initializer=_init, initargs=(w,)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round19_bestpractice_runs.json"), "w"))
