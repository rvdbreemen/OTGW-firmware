"""TASK-1527 round 16: degree minutes for every heat-pump type we model (Robert 2026-10-31 15:10 UTC, #dev-sat-mqtt
msg 1566107128047865907: "I really like the degree minutes version. How does that work and also simulate all other
types of heatpumps we have modeled.").

Heat pumps on radiators (round 4/5b plant, calibrated radiators, SAT curve + PID, coefficient at the balance, no
tuner), full heating seasons 1 Oct .. 30 Apr (2010-11, 2013-14, 2025-26), 5 houses, 2 seeds:
- hp30: modulates down to 30 % of its power; hp60: down to 60 %; hp100: on/off (minimum = maximum).
Control modes (study4.HP_CTRL, heat pumps only):
- "sat" 1800 / 2400 s: SAT PWM as today (entries 40/50);
- "own": SAT never switches to PWM; the heat pump modulates on the SAT setpoint and cycles itself at its minimum
  (off above CS + 5 K, on below CS - 5 K after 3 min off);
- "dm" S: degree minutes, the NIBE method: DM += (flow - SAT setpoint) every minute; the compressor starts at
  DM <= -S and stops at DM >= 0, at least 20 min on and 10 min off; while it runs the heat pump modulates itself.
The on/off heat pump on underfloor heating is in rounds 13-15 (degree minutes 60-480 there).
Run: python scripts/sat_hp_dm_study16.py --seasondir <weather_season dir> --krs <round5c_runs.json> --outdir <dir>
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b

def _init(w, krs):
    base._init(w); s5b.KRS = krs
    s4.OVERSHOOT_MARGIN = 2.0; s4.MAX_SP_GAS = 62.0; s4.MAX_SP_HP = 40.0; s4.EXCLUDE_PWM = True; s4.PID_D_HOLD = True

MODES = [("sat", 1800), ("sat", 2400), ("own", 0), ("dm", 60), ("dm", 120), ("dm", 240), ("dm", 480)]

def job(a):
    mode, par, era, mass, src, sd = a
    s4.HP_CTRL = mode
    if mode == "sat": s4.HP_PERIOD_S = par
    if mode == "dm": s4.HP_DM_START = float(par)
    kr = s5b.KRS[f"{era}|{mass}|1.8"]
    r = s4.run((("none",), era, mass, kr, round(1.8 - s5b._cb(era, mass, kr), 3), 30, sd, False, src))
    r.update(mode=mode, par=par); return r

def jobs():
    return [(mo, p, e, m, src, sd) for mo, p in MODES for e, m in s5b.HOUSES for src in ("hp30", "hp60", "hp100") for sd in (1, 2)]

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; res = {}
    part = os.path.join(arg("--outdir"), "round16_parts"); os.makedirs(part, exist_ok=True)   # one file per season: resumable
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
        if os.path.exists(dest): res[label] = json.load(open(dest)); print(label, "exists, skipped", flush=True); continue
        with Pool(int(arg("--workers")) if "--workers" in sys.argv else 8, initializer=_init, initargs=(w, krs)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        tmp = dest + ".tmp"; json.dump(res[label], open(tmp, "w")); os.replace(tmp, dest)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round16_hp_dm_runs.json"), "w"))
