"""TASK-1527 round 13: control options for long runs, heat-pump mode only (Robert 2026-10-31: "a new way of control
where the PWM is extended specifically for this type of heat pump ... long runs instead of PWM with a 40-minute
cycle ... only for the heat-pump mode, never for the general SAT control").

Plant and SAT curve/PID as round 12 (`sat_hp_onoff_study12.py`): on/off ground-source heat pump at full power on
underfloor heating, heavy well-insulated houses. Added: a light screed (3 MJ/K) as a robustness case.
Compressor protection for every new option: at least MIN_ON 20 min on and MIN_OFF 10 min off (assumption; Trane
Symbio uses 5 / 3 min, a forum source cites 10 min for scroll lubrication; no ground-source datasheet found).

Options (`variant`, parameter):
- "pwm" P: today's SAT heat-pump PWM, ON time fixed at the period start (P = 30 / 40 min, minimum ON 3 min).
- "own": SAT continuous, the heat pump cycles on its own control around the SAT CS (+-5 K flow).
- "tp" P: time-proportional PWM, ON time re-evaluated every minute, at most one start per period (P = 180 / 240).
- "dm" S: degree minutes, as NIBE (F1145 manual menu 4.9.3: start compressor default -60 DM, range -1000..-30;
  "a higher value gives more compressor starts"): DM += (flow - SAT setpoint) every minute, clamped to [-1500, +100];
  start when DM <= -S, stop when DM >= 0 (stop at 0 from community sources, not from the manual). S = 60..480.
- "rh" h: room hysteresis: on when room < target - h, off when room > target + h (h = 0.1 / 0.2 / 0.3 C).
- "mr" R: minimum-run PWM: every ON run lasts R minutes; the OFF time after it follows from SAT's duty d:
  R * (1 - d) / d, clamped to [MIN_OFF, 600] min (R = 60 / 90 / 120).
Run: python scripts/sat_hp_onoff_study13.py --weatherdir <weather20 dir> --weather2526 <json> --outdir <dir>
"""
import json, math, os, random, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
import sat_hp_onoff_study12 as h12
from sat_curve_selftune_study import TARGET, DT

MIN_ON, MIN_OFF = 20, 10

def run(args):
    variant, par, era, mass, screed, seed = args
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, mass, 1.0)
    season = statistics.fmean(t for t in tm if t < 15)
    K = h12.k_for(Heff, season)
    pmax = max(3000.0, round(1.1 * Heff * (TARGET + 10.0), -2))
    rnd = random.Random(seed); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    c = 1.8; dflow = pmax / (2 * h12.MCP_UFH)
    Ta = TARGET; Tw = (s3.G_AW * TARGET + Gwo * season) / (s3.G_AW + Gwo)
    Ts = h12.curve_ufh(c, season) - 2.0
    integ = 0.0; last = None; rd = 0.0
    on = False; on_for = 0; off_for = 999; starts = 0; run_lens = []; starts_day = {}
    pos = 0; on_m = 0; stopped = False; dm = 0.0; mr_off_left = 0
    minon = 3 if variant == "pwm" else MIN_ON
    minoff = 0 if variant in ("pwm", "own") else MIN_OFF
    sq = n = out = absum = 0.0; elec = 0.0
    for m in range(minutes):
        tout, sw = tm[m], sm[m]; heating = tout < 15
        cv = h12.curve_ufh(c, tout); kp = c * cv / 4; ki = kp / 8400; kd = 588.0 * kp
        err = TARGET - Ta
        if last is not None and abs(err) > 0.1 and Ta != last: rd = -(Ta - last) / DT
        last = Ta
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = min(h12.SP_MAX, max(10.0, cv + kp * err + integ + kd * rd)) if heating else 10.0
        flow = Ts + (dflow if on else 0.0)
        duty = min(max((sp - h12.BASE_UFH) / max(Ts + dflow - h12.BASE_UFH, 1.0), 0.0), 1.0)
        if not heating:
            want = False
        elif variant == "own":
            want = (not (flow > sp + h12.HYST)) if on else (flow < sp - h12.HYST)
        elif variant == "pwm":
            if pos == 0:
                on_m = int(round(duty * par))
                if 0 < on_m < minon: on_m = minon if duty * par >= minon / 2 else 0
            want = pos < on_m; pos = (pos + 1) % par
        elif variant == "tp":
            tgt = int(round(duty * par))
            if 0 < tgt < minon: tgt = minon if duty * par >= minon / 2 else 0
            if pos == 0: stopped = False
            want = (not stopped) and pos < tgt
            if on and not want: stopped = True
            if not on and pos > 0 and stopped: want = False
            pos = (pos + 1) % par
        elif variant == "dm":
            dm = max(-1500.0, min(100.0, dm + (flow - sp)))
            want = (dm < 0.0) if on else (dm <= -par)
        elif variant == "rh":
            want = (Ta < TARGET + par) if on else (Ta < TARGET - par)
        elif variant == "mr":
            if on: want = on_for < par
            else:
                if mr_off_left > 0: mr_off_left -= 1
                want = mr_off_left <= 0 and duty > 0.0
        # compressor protection
        if on and not want and on_for < minon: want = True
        if not on and want and off_for < minoff: want = False
        if want and not on:
            starts += 1; on_for = 0; starts_day[m // 1440] = starts_day.get(m // 1440, 0) + 1
        if on and not want:
            run_lens.append(on_for)
            if variant == "mr":
                d_ = max(duty, 0.02); mr_off_left = int(min(600, max(MIN_OFF, par * (1 - d_) / d_)))
        on = want
        if on: on_for += 1; off_for = 0
        else: off_for += 1
        P = pmax if on else 0.0
        flow = Ts + (P / (2 * h12.MCP_UFH))
        if P > 0: elec += P * DT / (0.5 * (flow + 273.15) / max(flow - 5.0, 5.0))
        q = K * max(Ts - Ta, 0.0) ** h12.EXP_UFH
        Ts += (P - q) * DT / screed
        qaw = s3.G_AW * (Ta - Tw); solar = 1.5 * sw
        Ta += (q + gains[m] + 0.5 * solar - qaw - Gao * (Ta - tout)) * DT / Ca
        Tw += (qaw + 0.5 * solar - Gwo * (Tw - tout)) * DT / Cw
        if heating and m >= 14 * 1440:
            e = Ta - TARGET; sq += e * e; n += 1; absum += abs(e)
            if abs(e) > 0.3: out += 1
    sd = [v for d, v in starts_day.items() if d >= 14]
    return {"variant": variant, "par": par, "era": era, "mass": mass, "screed": screed, "seed": seed,
            "starts_per_day": starts / (minutes / 1440), "starts_day_max": max(sd) if sd else 0,
            "run_median_min": statistics.median(run_lens) if run_lens else 0,
            "run_short_frac": (sum(1 for r in run_lens if r < 20) / len(run_lens)) if run_lens else 0,
            "out_band_min_d": out / (n / 1440) if n else 0, "mean_abs_err": absum / n if n else 0,
            "rms": math.sqrt(sq / n) if n else 0, "elec_kwh": elec / 3.6e6}

def _init(w): base._init(w)

VARIANTS = ([("pwm", p) for p in (30, 40)] + [("own", 0)] + [("tp", p) for p in (180, 240)]
            + [("dm", s) for s in (60, 120, 240, 480)] + [("rh", h) for h in (0.1, 0.2, 0.3)] + [("mr", r) for r in (60, 90, 120)])

def jobs():
    houses = [("4_1992-2012", "heavy"), ("5_2015", "heavy"), ("6_BENG2021", "heavy")]
    return [(v, p, e, ms, scr, sd) for v, p in VARIANTS for e, ms in houses for scr in (8.0e6, 3.0e6) for sd in (1, 2)]

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    wd = arg("--weatherdir"); res = {}
    for label, w in (("2010_11", os.path.join(wd, "weather_2010_11.json")), ("2013_14", os.path.join(wd, "weather_2013_14.json")), ("2025_26", arg("--weather2526"))):
        with Pool(8, initializer=_init, initargs=(w,)) as p: res[label] = p.map(run, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round13_hp_longrun_runs.json"), "w"))
