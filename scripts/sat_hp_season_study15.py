"""TASK-1527 round 15: the heat-pump options of rounds 13-14 over full heating seasons (1 October .. 30 April, 212 days)
instead of November .. February (Robert 2026-10-31: "Heb je ook simulatie uitgevoerd met een hele winterdata set?").

October, March and April carry the lowest loads, where an on/off or 60 % heat pump switches most. Seasons 2010-11
(cold), 2013-14 (mild) and 2025-26 (average), Open-Meteo archive, same grid cell; data
docs/sat/sim-hp-lowload/weather_season/.

Model fix against rounds 12-14: the underfloor emitter constant K was calibrated on the season mean of the weather
being simulated, so the "same" house differed slightly per winter (paired comparisons inside a winter were not
affected). Here K is calibrated once on the reference winter mean (Nov-Feb 2025-26, 4.69 C), so every season runs the
same house. The radiator plant (B2) already used fixed radiator sizes (round 5b).

B1 on/off heat pump on underfloor heating (heavy and light screed): today's PWM 1800 / 2400 s, the third option
7200 s, time-proportional 120 / 240 min, degree minutes -120 / -240, room hysteresis 0.2 C, own control.
B2 modulating heat pumps 30 % / 60 % on radiators: SAT PWM 1800 / 2400 / 7200 s.
Run: python scripts/sat_hp_season_study15.py --seasondir <weather_season dir> --krs <round5c_runs.json> --outdir <dir>
"""
import glob, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_hp_onoff_study12 as h12
import sat_hp_onoff_study13 as h13

REF_SEASON_MEAN = 4.69
_k_orig = h12.k_for

def _k_fixed(Heff, season, c=1.8, gains=135.0):
    return _k_orig(Heff, REF_SEASON_MEAN, c, gains)

def _init(w, krs):
    base._init(w); s5b.KRS = krs; h12.k_for = _k_fixed
    s4.OVERSHOOT_MARGIN = 2.0; s4.MAX_SP_GAS = 62.0; s4.MAX_SP_HP = 40.0; s4.EXCLUDE_PWM = True; s4.PID_D_HOLD = True

def job(a):
    if a[0] == "B1":
        _, v, p, era, scr, sd = a
        r = h13.run((v, p, era, "heavy", scr, sd)); r["plant"] = "onoff_ufh"; return r
    _, period, era, mass, src, sd = a
    s4.HP_PERIOD_S = period; kr = s5b.KRS[f"{era}|{mass}|1.8"]
    r = s4.run((("none",), era, mass, kr, round(1.8 - s5b._cb(era, mass, kr), 3), 30, sd, False, src))
    r.update(plant="mod_rad", period_s=period); return r

B1 = [("pwm", 30), ("pwm", 40), ("pwm", 120), ("tp", 120), ("tp", 240), ("dm", 120), ("dm", 240), ("rh", 0.2), ("own", 0)]

def jobs():
    out = [("B1", v, p, e, scr, sd) for v, p in B1 for e in ("4_1992-2012", "5_2015", "6_BENG2021") for scr in (8.0e6, 3.0e6) for sd in (1, 2)]
    out += [("B2", per, e, m, src, sd) for per in (1800, 2400, 7200) for e, m in s5b.HOUSES for src in ("hp30", "hp60") for sd in (1, 2)]
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; res = {}
    for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
        label = os.path.basename(w)[7:-5]
        with Pool(8, initializer=_init, initargs=(w, krs)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round15_hp_season_runs.json"), "w"))
