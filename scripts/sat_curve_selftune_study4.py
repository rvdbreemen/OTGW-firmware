"""TASK-1519 round 4: gas use and low load. Two-node houses of round 3 plus a boiler with minimum power, condensing
efficiency, start losses, and SAT PWM (thermo-nova pwm.py at the pin 7beaaf5).

Boiler / water loop (assumptions, labelled in docs/sat/sim-1519/round4-model.md):
  Pmin 3 kW, Pmax 24 kW; loop water 60 l (250 kJ/K); pump 0.15 kg/s (628 W/K); radiator Q = K*(Tmean - Tair)^1.3;
  flow = loop + dT/2, return = loop - dT/2 with dT = Q / 628.
  Boiler's own control: P = clamp(Q + 2000 W/K * (setpoint - flow), Pmin, Pmax); at Pmin with flow > setpoint + 5 C it
  stops, stays off at least 3 minutes, restarts below setpoint - 5 C (typical anti-cycling, estimate).
  Efficiency from the return temperature: 97.5 % at <= 30 C, linear to 87 % at 54 C (dew point), 86 % above 60 C
  (GSA condensing-boiler findings, CSE Magazine, R.F. MacDonald). Start loss per ignition: 0 / 30 / 100 kJ (assumption,
  no measured source found; run as a sensitivity).
SAT PWM (pwm.py _calculate_duty_cycle and thresholds, const.py PWM_*): enable when flow > setpoint + 0.5 C at low
modulation (< 10 %) for 6 ticks; disable when flow < setpoint - 1.5 C or modulation > 30 %. duty = (sp - base) /
(T_on - base), T_on = flow when the flame came on; on/off times from the thresholds with 3 cycles per hour
(upper 1200 s, lower 180 s); during ON the boiler burns at Pmin, during OFF it is off.
Scores: fuel (gas, J), comfort, overshoot, burner starts per day, burns shorter than 5 min, coefficient changes.
Run: python scripts/sat_curve_selftune_study4.py --weather <json> [--out file] [--validate]
"""
import json, math, os, random, statistics, sys, itertools
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
from sat_curve_selftune_study import curve, TARGET, DT

PMIN, PMAX, CLOOP, MCP = 3000.0, 24000.0, 250e3, 628.0
BASE_OFF = 27.2
TRACE_EVERY = 10
EXCLUDE_PWM = False   # True: hours with SAT PWM active do not count as clean for the tuner
UPPER, LOWER = 1200.0, 180.0
DUTY_LO, DUTY_HI = LOWER / UPPER, 1 - LOWER / UPPER
DUTY_MIN = DUTY_LO / 2

def eff(tret):
    if tret <= 30: return 0.975
    if tret <= 54: return 0.975 - (tret - 30) * (0.975 - 0.87) / 24
    if tret <= 60: return 0.87 - (tret - 54) * 0.01 / 6
    return 0.86

class BiasDaily(base.Daily):
    """KISS daily rule aimed slightly low: steps on (mean correction - BIAS); BIAS > 0 keeps the curve a little low
    so the PID does the last part (lower flow and return)."""
    def __init__(self, D, THR, STEP, BIAS):
        super().__init__(D, THR, STEP, False); self.BIAS = BIAS; self.name = f"D {D}d thr{THR} st{STEP} aim-low{BIAS}"
    def hour(self, c, day, mean_corr, clean, *a):
        return super().hour(c, day, mean_corr - self.BIAS, clean)

def make(spec):
    k = spec[0]
    if k == "none": return base.NoTune()
    if k == "H": return base.Hourly(*spec[1:])
    if k == "D": return base.Daily(*spec[1:])
    return BiasDaily(*spec[1:])

def run(args):
    spec, era, mass, kr, c_off, start_loss, seed = args[:7]
    trace = [] if len(args) > 7 and args[7] else None
    source = args[8] if len(args) > 8 else "gas"
    if source == "gas": pmin, pmax, upper = PMIN, PMAX, UPPER
    else:
        Heff_ = s3.house(era, mass, kr)[4]
        pmax = max(4000.0, round(1.1 * Heff_ * (TARGET + 10.0), -2))   # sized to the design load at -10 C (+10 %)
        pmin = pmax * (0.3 if source == "hp30" else 0.6); upper = 1800.0   # entry 40: HP cycles 1800 s
    duty_lo = LOWER / upper; duty_hi = 1 - duty_lo; duty_min = duty_lo / 2
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    Ca, Cw, Gao, Gwo, Heff, K = s3.house(era, mass, kr)
    rnd = random.Random(seed); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    season = statistics.fmean(t for t in tm if t < 15)
    cbest = s3.best_coeff2(Heff, K, season)
    c = round(min(5.0, max(0.1, cbest + c_off)), 3)
    st = make(spec)
    Ta = TARGET; Tw = (s3.G_AW * TARGET + Gwo * season) / (s3.G_AW + Gwo)
    Tloop = 35.0; qrad = 0.0; burner = False; off_min = 99; on_min = 0
    integ = 0.0; last = None; rd = 0.0
    pwm = False; pwm_phase_on = False; pwm_left = 0; pwm_new = False; t_on = 40.0; Pcur = 0.0
    ov_s = uh_s = sat_s = 0; last_draw_m = -10**6; last_off_s = 1
    corr_acc = 0.0; corr_n = 0; clean = True; stepped = False
    sq = n = out_band = over = 0.0; fuel = 0.0; heat = 0.0; starts = 0; short_burns = 0; pwm_min = 0; flame_min = 0
    writes = flips = 0; last_dir = 0
    for m in range(minutes):
        day = m // 1440; tout, sw = tm[m], sm[m]; heating = tout < 15
        cv = curve(c, tout); kp = c * cv / 3; ki = kp / 8400; kd = 588.0 * kp
        err = TARGET - Ta
        if last is not None:
            d_ = Ta - last; rd = 0.0 if d_ == 0 else -d_ / DT
        last = Ta
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = round(min(80.0, max(10.0, cv + kp * err + integ + kd * rd)), 1) if heating else 10.0   # SAT clamps CS to [MINIMUM_SETPOINT 10, maximum]
        draw = (420 <= m % 1440 < 430) or (1140 <= m % 1440 < 1150)
        dT = qrad / MCP; flow = Tloop + dT / 2; tret = Tloop - dT / 2
        # ---- SAT PWM enable/disable, thermo-nova heating_control.py:262-350 at the pin:
        # enable: flame on, no hot water (and not within 300 s after it), flow >= requested + 3.0 C for 60 s;
        # disable only during the PWM ON phase: flow <= requested - 3.0 C for 180 s (underheat),
        # or off time 0 for 300 s (saturation). Margins: cycles/const.py:10-11.
        guard = draw or (m - last_draw_m) * 60 < 300
        if heating and not guard and sp > 10.0:
            if not pwm:
                ov_s = ov_s + 60 if (burner and flow >= sp + 3.0) else 0
                if ov_s >= 60: pwm = True; pwm_new = True; pwm_left = 0; ov_s = 0; uh_s = 0; sat_s = 0
            elif pwm_phase_on:
                uh_s = uh_s + 60 if flow <= sp - 3.0 else 0
                sat_s = sat_s + 60 if last_off_s == 0 else 0
                if uh_s >= 180 or sat_s >= 300: pwm = False; uh_s = 0; sat_s = 0
        else:
            ov_s = 0; uh_s = 0; sat_s = 0
            if not heating or sp <= 10.0: pwm = False
        if draw: last_draw_m = m
        if pwm:
            if pwm_left <= 0:
                duty = min(max((sp - BASE_OFF) / max(t_on - BASE_OFF, 1.0), 0.0), 1.0)
                if duty < duty_min: on_s, off_s = 0.0, 2 * upper
                elif duty <= duty_lo: on_s, off_s = LOWER, LOWER / duty - LOWER
                elif duty <= duty_hi: on_s, off_s = upper * duty, upper * (1 - duty)
                else: on_s, off_s = upper, 0.0
                pwm_phase_on = True if pwm_new else not pwm_phase_on   # first phase ON (pwm.py, entry 43)
                pwm_new = False
                last_off_s = off_s
                pwm_left = int(round((on_s if pwm_phase_on else off_s) / 60)) or 1
            pwm_left -= 1
            want = pwm_phase_on
            P = pmin if want else 0.0
        else:
            # boiler's own modulation and anti-cycling
            if not heating or draw or sp <= 10.0: want = False
            elif burner: want = not (flow > sp + 5.0)
            else: want = off_min >= 3 and flow < sp - 5.0
            P = min(pmax, max(pmin, qrad + 2000.0 * (sp - flow))) if want else 0.0   # boiler's own flow loop (2000 W/K matched the field flame fraction best)
        if want and not burner:
            starts += 1; fuel += start_loss * 1000.0; on_min = 0; t_on = flow
        if burner and not want and on_min < 5: short_burns += 1
        burner = want; Pcur = P
        if burner: on_min += 1; off_min = 0; flame_min += 1
        else: off_min += 1
        if pwm: pwm_min += 1
        if source == "gas": fuel += P * DT / eff(tret)
        else: fuel += P * DT / max(1.0, min(6.0, 0.45 * (flow + 273.15) / max(flow - tout, 5.0)))   # electricity, COP assumption
        qrad = K * max(Tloop - Ta, 0.0) ** 1.3 if not draw else 0.0     # mean water temp ~ loop temperature
        Tloop += (P - qrad) * DT / CLOOP
        qaw = s3.G_AW * (Ta - Tw); solar = 1.5 * sw
        Ta += (qrad + gains[m] + 0.5 * solar - qaw - Gao * (Ta - tout)) * DT / Ca
        Tw += (qaw + 0.5 * solar - Gwo * (Tw - tout)) * DT / Cw
        if heating:
            corr_acc += sp - cv; corr_n += 1
            if day >= 14:
                sq += err * err; n += 1; heat += qrad * DT
                if abs(Ta - TARGET) > 0.3: out_band += 1
                if Ta > TARGET + 0.3: over += 1
        clean &= heating and not draw and sw < 50.0 and not (EXCLUDE_PWM and pwm)
        if trace is not None and m % TRACE_EVERY == 0:
            trace.append((m / 60, round(tout, 2), round(Ta, 3), round(flow, 1), round(tret, 1), sp, round(P), int(pwm), c))
        if m % 60 == 59:
            mc = corr_acc / corr_n if corr_n else 0.0
            new_c = st.hour(c, day, mc, clean and not stepped and corr_n > 0)
            corr_acc = 0.0; corr_n = 0; clean = True; stepped = False
            if new_c != c:
                dr = 1 if new_c > c else -1
                if last_dir and dr != last_dir: flips += 1
                last_dir = dr; writes += 1; c = new_c; integ = 0.0; rd = 0.0; stepped = True
        if m % 1440 == 1439:
            new_c = st.end_of_day(c, day)
            if new_c != c:
                dr = 1 if new_c > c else -1
                if last_dir and dr != last_dir: flips += 1
                last_dir = dr; writes += 1; c = new_c; integ = 0.0; rd = 0.0
    if trace is not None: return {"spec": st.name, "trace": trace}
    days = minutes / 1440
    return {"spec": st.name, "source": source, "era": era, "mass": mass, "kr": kr, "c_off": c_off, "start_loss_kJ": start_loss, "seed": seed,
            "rms": math.sqrt(sq / n), "out_band_min_d": out_band / (n / 1440), "over_min_d": over / (n / 1440),
            "fuel_J": fuel, "heat_J": heat, "starts_per_day": starts / days, "short_burns_per_day": short_burns / days,
            "pwm_frac": pwm_min / minutes, "flame_frac": flame_min / minutes, "writes": writes, "flips": flips,
            "c_end": c, "cbest": cbest}

def validate(wpath):
    """Mild-day check against the field capture (4 h, 9-15 C outside): flame on 24 % of the time, 4.0 starts per hour,
    CH burns 0.4/4.5/5.1 min (min/median/max)."""
    global TRACE_EVERY
    base._init(wpath); TRACE_EVERY = 1
    tm, sm = base.WEATHER["w"]
    days = {d for d in range(len(tm) // 1440) if 9 <= statistics.fmean(tm[d * 1440:(d + 1) * 1440]) <= 15}
    print("mild days (9-15 C mean):", len(days), "| field: flame 0.24, 4.0 starts/h, CH burn median 4.5 min")
    for era in s3.ERAS:
        tr = run((("none",), era, "heavy", 1.0, 0.0, 30, 1, True, "gas"))["trace"]
        rows = [x for x in tr if int(x[0] // 24) in days]
        on = [x[6] > 0 for x in rows]
        starts = sum(1 for i in range(1, len(on)) if on[i] and not on[i - 1])
        burns, run_len = [], 0
        for o in on:
            if o: run_len += 1
            elif run_len: burns.append(run_len); run_len = 0
        print(f"  {era:12s} flame {statistics.fmean(on):.2f}  starts/h {starts / (len(rows) / 60):.1f}  burn median {statistics.median(burns) if burns else 0:.0f} min  pwm {statistics.fmean(x[7] for x in rows):.2f}")

if __name__ == "__main__":
    wpath = sys.argv[sys.argv.index("--weather") + 1]
    if "--validate" in sys.argv: validate(wpath); sys.exit(0)
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "study4_results.json"
    specs = [("none",), ("D", 3, 1.0, 0.1, False), ("H", 3, 1, 1.0, 0.1), ("B", 3, 1.0, 0.1, 1.0), ("B", 3, 1.0, 0.1, 2.0)]
    houses = [(e, "heavy", 1.0) for e in s3.ERAS] + [("4_1992-2012", "light", 1.0), ("6_BENG2021", "light", 1.0)]
    jobs = [(s, *h, off, loss, sd, False, "gas") for s in specs for h in houses for off in (-0.5, 0.0, 0.5) for loss in (0, 30, 100) for sd in (1, 2)]
    jobs += [(s, *h, off, 0, sd, False, src) for s in specs for h in houses for off in (-0.5, 0.0, 0.5) for sd in (1, 2) for src in ("hp30", "hp60")]
    print(f"{len(specs)} tuners x {len(houses)} houses x 3 start curves x 3 start losses x 2 seeds = {len(jobs)} runs", flush=True)
    with Pool(6, initializer=base._init, initargs=(wpath,)) as p:
        res = p.map(run, jobs, chunksize=2)
    key = lambda r: (r["source"], r["era"], r["mass"], r["start_loss_kJ"], r["seed"])
    ref = {key(r): r["fuel_J"] for r in res if r["spec"] == "none" and r["c_off"] == 0.0}
    for r in res: r["gas_pct"] = 100.0 * r["fuel_J"] / ref[key(r)]
    json.dump(res, open(out, "w"))
    by = {}
    for r in res: by.setdefault(r["source"] + " | " + r["spec"], []).append(r)
    print(f"{'source | tuner':38s} {'rms':>6} {'out/d':>6} {'over/d':>6} {'gas%':>6} {'starts/d':>8} {'short/d':>7} {'pwm%':>5} {'writes':>6}")
    for k, rs in by.items():
        f = lambda key: statistics.fmean(r[key] for r in rs)
        print(f"{k:38s} {f('rms'):6.3f} {f('out_band_min_d'):6.1f} {f('over_min_d'):6.1f} {f('gas_pct'):6.1f} {f('starts_per_day'):8.1f} {f('short_burns_per_day'):7.1f} {100*f('pwm_frac'):5.1f} {f('writes'):6.1f}")
