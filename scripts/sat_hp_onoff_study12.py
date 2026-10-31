"""TASK-1527 round 12: an on/off ground-source heat pump on underfloor heating (maarten_41762, #alpha-testing
2026-10-31 14:01 UTC, msg 1566089657492705431) under SAT PWM with longer cycle periods.

Robert 2026-10-31: the goal for a heat pump is the minimum number of switchings with maximum comfort.
Maarten: "my heatpump is a dumb on/off system ... in relatively good isolated home with lot of internal mass ...
in my home a PWM period of 3 hours is a good balance between having a bit longer runs and relative low amount of
starts per year while maintaining indoor temperatures quite stable at setpoint."

Model (assumptions labelled; nothing here is validated against Maarten's installation):
- House: the two-node houses of round 3 (heavy 1992-2012, 2015, BENG), outdoor air and solar as in rounds 1-11.
- Emitter: underfloor heating. The screed is the emitter node (heat capacity CSCREED, assumption 8 MJ/K, about
  120 m2 of 5 cm screed); heat to the room K * (Tscreed - Troom) ** 1.1. Flow = Tscreed + P / (2 * MCP_UFH).
  K is calibrated per house so the season balance is at coefficient 1.8 with the underfloor base offset 20
  (SATcontrol.ino:127), as round 5b did for radiators.
- Heat pump: on/off only (no modulation), power sized at the design load at -10 C plus 10 %. Electricity from a
  COP of 0.5 x Carnot with a 5 C brine source (assumption for ground source).
- SAT: thermo-nova curve and PID (as rounds 1-11, PID derivative held in the deadband), max flow 40 C
  (SAT_MAXTSET_HEAT_PUMP_CAP, entry 155). Variants:
    "pwm P / on M": SAT always runs PWM with period P minutes; the ON part of each period = duty x P, at least
    M minutes, using the SAT duty formula (pwm.py, SATcontrol.ino:1172-1264) with the 20 C base offset;
    "pwmlive P / on M": a time-proportional controller as floor-heating controllers use: the duty is
    re-evaluated every minute inside the period, at most one start per period, the ON run at least M minutes;
    "own": no SAT PWM, the heat pump's own control: on below CS - 5 K flow, off above CS + 5 K, minimum on M.
- Scores: compressor starts per day, minutes per day outside +-0.3 C, mean |room - target|, electricity.
Run: python scripts/sat_hp_onoff_study12.py --weatherdir <weather20 dir> --weather2526 <json> --outdir <dir>
"""
import json, math, os, random, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
from sat_curve_selftune_study import TARGET, DT

BASE_UFH = 20.0
CSCREED = 8.0e6
MCP_UFH = 1050.0
EXP_UFH = 1.1
SP_MAX = 40.0
HYST = 5.0

def curve_ufh(c, to):
    return round(BASE_UFH + (c / 4) * (4 * (TARGET - 20) + 0.03 * (to - 20) ** 2 - 0.4 * (to - 20)), 1)

def k_for(Heff, season, c=1.8, gains=135.0):
    """Emitter constant so that coefficient c balances the house at the season mean (round 5b method)."""
    Q = max(Heff * (TARGET - season) - gains, 50.0)
    flow = curve_ufh(c, season)
    tscreed = flow - Q / (2 * MCP_UFH)
    return Q / max(tscreed - TARGET, 0.5) ** EXP_UFH

def run(args):
    variant, period_min, minon_min, era, mass, seed = args
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, mass, 1.0)
    season = statistics.fmean(t for t in tm if t < 15)
    K = k_for(Heff, season)
    pmax = max(3000.0, round(1.1 * Heff * (TARGET + 10.0), -2))
    rnd = random.Random(seed); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    c = 1.8
    Ta = TARGET; Tw = (s3.G_AW * TARGET + Gwo * season) / (s3.G_AW + Gwo)
    Ts = curve_ufh(c, season) - 2.0
    integ = 0.0; last = None; rd = 0.0
    on = False; on_for = 0; off_for = 999; starts = 0; run_lens = []
    pos = 0; on_m = 0; stopped = False; on_m_started = False
    period = period_min or 1      # minutes
    minon = minon_min
    sq = n = out = absum = 0.0; elec = 0.0
    for m in range(minutes):
        tout, sw = tm[m], sm[m]; heating = tout < 15
        cv = curve_ufh(c, tout); kp = c * cv / 4; ki = kp / 8400; kd = 588.0 * kp   # underfloor divisor 4 (pid.py:69)
        err = TARGET - Ta
        if last is not None and abs(err) > 0.1 and Ta != last: rd = -(Ta - last) / DT
        last = Ta
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = min(SP_MAX, max(10.0, cv + kp * err + integ + kd * rd)) if heating else 10.0
        flow = Ts + (pmax / (2 * MCP_UFH) if on else 0.0)
        if not heating:
            want = False
        elif variant == "own":
            if on: want = not (flow > sp + HYST and on_for >= minon)
            else: want = flow < sp - HYST
        else:
            if pos == 0:   # start of a PWM period: SAT duty (pwm.py) -> ON minutes, at least the minimum on time
                duty = min(max((sp - BASE_UFH) / max(Ts + pmax / (2 * MCP_UFH) - BASE_UFH, 1.0), 0.0), 1.0)
                on_m = int(round(duty * period))
                if 0 < on_m < minon: on_m = minon if duty * period >= minon / 2 else 0
            if variant == "pwmlive":   # time-proportional: duty re-evaluated every minute, at most one start per period
                duty = min(max((sp - BASE_UFH) / max(Ts + pmax / (2 * MCP_UFH) - BASE_UFH, 1.0), 0.0), 1.0)
                tgt = int(round(duty * period))
                if 0 < tgt < minon: tgt = minon if duty * period >= minon / 2 else 0
                if pos == 0: stopped = False
                want = (not stopped) and pos < tgt
                if on and not want and on_for >= minon: stopped = True
                if not on and pos > 0 and want and on_m_started: want = False   # no second start in the same period
                on_m_started = on_m_started or want if pos else want
            else:
                want = pos < on_m
            pos = (pos + 1) % period
            if on and not want and on_for < minon: want = True
        if want and not on: starts += 1; on_for = 0
        if on and not want: run_lens.append(on_for)
        on = want
        if on: on_for += 1; off_for = 0
        else: off_for += 1
        P = pmax if on else 0.0
        flow = Ts + (P / (2 * MCP_UFH))
        if P > 0: elec += P * DT / (0.5 * (flow + 273.15) / max(flow - 5.0, 5.0))
        q = K * max(Ts - Ta, 0.0) ** EXP_UFH
        Ts += (P - q) * DT / CSCREED
        qaw = s3.G_AW * (Ta - Tw); solar = 1.5 * sw
        Ta += (q + gains[m] + 0.5 * solar - qaw - Gao * (Ta - tout)) * DT / Ca
        Tw += (qaw + 0.5 * solar - Gwo * (Tw - tout)) * DT / Cw
        if heating and m >= 14 * 1440:
            e = Ta - TARGET; sq += e * e; n += 1; absum += abs(e)
            if abs(e) > 0.3: out += 1
    days = (minutes - 14 * 1440) / 1440
    return {"variant": variant, "period_min": period_min, "minon_min": minon_min, "era": era, "mass": mass, "seed": seed,
            "starts_per_day": starts / (minutes / 1440), "run_median_min": statistics.median(run_lens) if run_lens else 0,
            "out_band_min_d": out / (n / 1440) if n else 0, "mean_abs_err": absum / n if n else 0, "rms": math.sqrt(sq / n) if n else 0,
            "elec_kwh": elec / 3.6e6, "pmax_w": pmax}

def _init(w): base._init(w)

def jobs():
    houses = [("4_1992-2012", "heavy"), ("5_2015", "heavy"), ("6_BENG2021", "heavy")]
    v = [("own", 0, mo) for mo in (3, 20)] + [(k, p, mo) for k in ("pwm", "pwmlive") for p in (30, 40, 60, 90, 120, 180, 240) for mo in (3, 20)]
    return [(a, p, mo, e, ms, sd) for a, p, mo in v for e, ms in houses for sd in (1, 2)]

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    wd = arg("--weatherdir"); res = {}
    for label, w in (("2010_11", os.path.join(wd, "weather_2010_11.json")), ("2013_14", os.path.join(wd, "weather_2013_14.json")), ("2025_26", arg("--weather2526"))):
        with Pool(8, initializer=_init, initargs=(w,)) as p: res[label] = p.map(run, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump(res, open(os.path.join(arg("--outdir"), "round12_hp_onoff_runs.json"), "w"))
