"""TASK-1527 round 14: Sergeant D (2026-10-31 14:46 UTC, #dev-sat-mqtt msg 1566100973019996253): "explore the
possibility to add a 3rd option for heat pumps a) 2x hour b) 1.5x hour c) 0.5x hour. This will give a third option
with PWM of 120 minutes. Run some tests and see if the PWM mechanism is not braking".

SAT's PWM as it is today (ON time fixed at the start of each period, thermo-nova duty formula, minimum ON 180 s),
heat-pump period 1800 s (a), 2400 s (b) and 7200 s (c), on two plants:
- B1: on/off heat pump on underfloor heating (round 12/13 plant), heavy and light screed; context options:
  degree minutes -120 and time-proportional 120 min (round 13 rules).
- B2: modulating heat pumps (minimum 30 % and 60 %) on radiators (round 4/5b plant, calibrated radiators, PWM entry
  and exit rules as thermo-nova heating_control.py:262-350), no tuner, coefficient at the balance (1.8).
Three winters (2010-11 cold, 2013-14 mild, 2025-26), 2 seeds.
Run: python scripts/sat_hp_period_study14.py --weatherdir <weather20 dir> --weather2526 <json> --krs <round5c_runs.json> --outdir <dir>
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_hp_onoff_study13 as h13

def _init(w, krs):
    base._init(w); s5b.KRS = krs
    s4.OVERSHOOT_MARGIN = 2.0; s4.MAX_SP_GAS = 62.0; s4.MAX_SP_HP = 40.0; s4.EXCLUDE_PWM = True; s4.PID_D_HOLD = True

def job(a):
    if a[0] == "B1":
        _, v, p, era, scr, sd = a
        r = h13.run((v, p, era, "heavy", scr, sd)); r["plant"] = "onoff_ufh"; return r
    _, period, era, mass, src, sd = a
    s4.HP_PERIOD_S = period; kr = s5b.KRS[f"{era}|{mass}|1.8"]
    r = s4.run((("none",), era, mass, kr, round(1.8 - s5b._cb(era, mass, kr), 3), 30, sd, False, src))
    r.update(plant="mod_rad", period_s=period); return r

def jobs():
    out = [("B1", v, p, e, scr, sd) for v, p in (("pwm", 30), ("pwm", 40), ("pwm", 120), ("dm", 120), ("tp", 120))
           for e in ("4_1992-2012", "5_2015", "6_BENG2021") for scr in (8.0e6, 3.0e6) for sd in (1, 2)]
    out += [("B2", per, e, m, src, sd) for per in (1800, 2400, 7200) for e, m in s5b.HOUSES for src in ("hp30", "hp60") for sd in (1, 2)]
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; wd = arg("--weatherdir"); res = {}
    for label, w in (("2010_11", os.path.join(wd, "weather_2010_11.json")), ("2013_14", os.path.join(wd, "weather_2013_14.json")), ("2025_26", arg("--weather2526"))):
        with Pool(8, initializer=_init, initargs=(w, krs)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round14_hp_period_runs.json"), "w"))
