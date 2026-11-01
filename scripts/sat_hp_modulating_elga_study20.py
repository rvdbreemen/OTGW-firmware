"""TASK-1527 round 20 (2026-11-01): a modulating air-to-water heat pump with hybrid gas backup, modelled on the
Remeha Elga Ace All-in-one 4 kW, under SAT and under its own control. Separate model from rounds 12-19 (on/off).

Specifications used (primary sources, read 2026-11-01; anything else is an assumption, marked A):
- Product datasheet "Remeha Elga Ace All-in-one" (PD, technim.nl, p.9): 4 kW model heat output 4.45 kW at A7/W35
  (COP 5.4) and 4.45 kW at A2/W35 (COP 4.05), EN 14511-2. Boiler 28c: 3.1-26.1 kW at 50/30 C, modulating.
- Installation and service manual 7868458-02 (2024), Tab.17 p.20 (ErP, 55 C application): part-load capacity Pdh at
  Tj +12 C = 1.3 kW, +7 = 1.4, +2 = 2.2, -7 = 3.5 kW (4 kW model); TOL -10 C. The +12 C value is above the reference
  demand at +12 C, so the unit cannot modulate below about 1.3 kW: used as the minimum output PMIN.
- Same manual p.118, HP155: minimum compressor standstill after a stop 8-10 min, default 10 min.
- Same manual p.105-106 and p.116 (HP061 hybrid mode default "Geen": heat pump first, boiler backup when needed;
  HP054 COP threshold 2.5 only in the optimising modes) and p.128 (backup start delay from outdoor temperature:
  HP047 8 min at HP049 -10 C to HP048 30 min at HP050 +15 C). Note: p.106 calls "Kosten" the factory setting, the
  parameter table p.116 lists "Geen" as default; the model uses "Geen".
Assumptions (A): own flow control P = clamp(q_floor + 1000 W/K x (CS - flow), PMIN, PMAX); the unit stops when the
flow exceeds CS + 3 K at PMIN and restarts below CS - 1 K after the 10 min standstill; minimum run 10 min (entry 197);
PMAX 4.45 kW down to 3.5 kW at -7 C (Pdh), linear between; COP at W35 interpolated between the two datasheet points,
-0.15 per K below +2 C (defrost), +0.1 per K above +7 C (cap 6.5), Carnot-scaled for other flow temperatures; boiler
efficiency 0.97 at low return temperature; CH = 0 stops the unit (no circulation); the boiler adds heat only while
the heat pump is at PMAX and the flow stays 2 K below CS for the backup delay, and stops when the flow reaches CS.

Plant: rounds 15/19 (underfloor, 3 houses 1992-2012 / 2015 / BENG 2021, heavy and light screed, K fixed on the
reference winter). Design loads at -10 C: 4.8 / 2.8 / 2.4 kW, so the 4 kW unit is undersized for the 1992-2012
house (backup runs) and oversized for the newer two (minimum output above the mild-weather load).

Variants:
- "sat": SAT as the firmware runs it for a heat pump: continuous mode sends the PID output as CS; the overshoot
  switch (flow >= CS + 2 K for 60 s, entry 87) enters PWM; PWM uses the firmware mapper (period 30 min, minimum ON
  and OFF 10 min, entries 197/198) with the ON-phase setpoints of thermo-nova's overshoot protection
  (heating_control.py:380-408): first minute CS = return + 18 (cap 40), then CS = flow - 1; OFF phase CS = 10, CH = 0.
  PWM leaves to continuous after 5 min of saturation (duty at maximum).
- "own": SAT sends the PID output as CS and never switches to PWM; the unit modulates and cycles by itself.
- "wc": the unit on its own weather curve, no room feedback (CS = curve).
Run: python scripts/sat_hp_modulating_elga_study20.py --seasondir docs/sat/sim-hp-lowload/weather_season --outdir <dir>
     [--sweep] [--workers N]
"""
import glob, json, math, os, random, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
import sat_hp_onoff_study12 as h12
import sat_hp_season_study15 as s15
from sat_curve_selftune_study import TARGET, DT

PMIN = 1300.0
PMAX_NOM, PMAX_COLD = 4450.0, 3500.0
STANDSTILL, MINRUN = 10, 10
HP_STOP_K, HP_START_K, HP_GAIN = 3.0, 1.0, 1000.0
BOILER_MIN, BOILER_MAX, BOILER_EFF = 3100.0, 26100.0, 0.97
PWM_P, PWM_MON, PWM_MOFF = 30, 10, 10

def pmax(t):
    if t >= 2: return PMAX_NOM
    if t <= -7: return PMAX_COLD
    return PMAX_COLD + (PMAX_NOM - PMAX_COLD) * (t + 7) / 9.0

def cop(flow, t):
    if t >= 7: c35 = min(6.5, 5.4 + 0.1 * (t - 7))
    elif t >= 2: c35 = 4.05 + (5.4 - 4.05) * (t - 2) / 5.0
    else: c35 = max(1.5, 4.05 - 0.15 * (2 - t))
    carnot = lambda f: (f + 273.15) / max(f - t + 10.0, 5.0)
    return max(1.0, c35 * carnot(flow) / carnot(35.0))

def backup_delay(t):
    if t <= -10: return 8
    if t >= 15: return 30
    return 8 + (30 - 8) * (t + 10) / 25.0

def run(args):
    variant, c, era, screed, seed = args
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, "heavy", 1.0)
    season = statistics.fmean(t for t in tm if t < 15)
    K = h12.k_for(Heff, season)
    rnd = random.Random(seed); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    Ta = TARGET; Tw = (s3.G_AW * TARGET + Gwo * season) / (s3.G_AW + Gwo)
    Ts = h12.curve_ufh(c, season) - 2.0
    integ = 0.0; last = None; rd = 0.0
    on = False; on_for = 0; off_for = 999; starts = 0; run_lens = []; starts_day = {}
    P = 0.0; boiler = False; deficit_min = 0
    pwm = False; ov = 0; sat_s = 0; ph_on = False; ph_el = 0; ph_entry = False; eff_on = None
    sq = n = out = absum = 0.0; elec = gas = 0.0; heat_hp = 0.0; at_min = run_min = 0; pwm_min = 0; boiler_min = 0
    for m in range(minutes):
        tout, sw = tm[m], sm[m]; heating = tout < 15
        cv = h12.curve_ufh(c, tout)
        if variant == "wc": kp = ki = kd = 0.0
        else: kp = c * cv / 4; ki = kp / 8400; kd = 588.0 * kp
        err = TARGET - Ta
        if last is not None and abs(err) > 0.1 and Ta != last: rd = -(Ta - last) / DT
        last = Ta
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = min(h12.SP_MAX, max(10.0, cv + kp * err + integ + kd * rd)) if heating else 10.0
        flow_now = Ts + P / (2 * h12.MCP_UFH)
        ret_now = Ts - P / (2 * h12.MCP_UFH)
        # ---- SAT: the control setpoint and CH sent to the unit ----
        cs, ch = sp, heating and sp > 21.0          # cold setpoint underfloor 21 C (SATcontrol.ino:56)
        if variant == "sat" and heating:
            if not pwm:
                ov = ov + 1 if (on and flow_now >= sp + 2.0) else 0
                if ov >= 1: pwm = True; ov = 0; ph_on = True; ph_el = 0; ph_entry = True; sat_s = 0
            if pwm:
                pwm_min += 1
                ref = eff_on if eff_on else max(flow_now, 21.0)
                duty = min(max((sp - h12.BASE_UFH) / max(ref - h12.BASE_UFH, 1.0), 0.0), 1.0)
                dl = PWM_MON / PWM_P; du = 1 - dl; dmin = dl / 2; dmax = 1 - dmin; mx = 2 * PWM_P
                if duty >= dmax: on_t, off_t, cont = mx, 0.0, True
                else:
                    cont = False
                    if duty < dmin: on_t, off_t = (PWM_MON, mx - PWM_MON) if on else (0.0, mx)
                    elif duty <= dl: on_t, off_t = PWM_MON, min(PWM_MON * (1 - duty) / duty, mx - PWM_MON)
                    elif duty <= du: on_t, off_t = max(duty * PWM_P, PWM_MON), PWM_P - duty * PWM_P
                    else: on_t, off_t = max(PWM_MON / (1 - duty) - PWM_MON, PWM_MON), PWM_MON
                if ph_on:
                    if not cont and ph_el >= on_t: ph_on = False; ph_el = 0
                elif ph_el >= PWM_MOFF and (cont or (on_t >= 3 and ph_el >= off_t)):
                    ph_on = True; ph_el = 0
                ph_el += 1
                sat_s = sat_s + 1 if cont else 0
                if sat_s >= 5: pwm = False; sat_s = 0; eff_on = None
                if ph_on:
                    if not on: cs = min(ret_now + 18.0, h12.SP_MAX); eff_on = None      # waiting for the compressor
                    else:
                        if eff_on is None or on_for <= 3: eff_on = flow_now if eff_on is None else 0.3 * flow_now + 0.7 * eff_on
                        cs = max(10.0, flow_now - 1.0)                                  # suppression setpoint
                    ch = True
                else:
                    cs, ch = 10.0, False
        elif not heating:
            pwm = False
        # ---- the unit's own control ----
        if not ch or not heating:
            want = False
        elif on:
            want = not (P <= PMIN + 1 and flow_now > cs + HP_STOP_K)
        else:
            want = flow_now < cs - HP_START_K
        if on and not want and on_for < MINRUN: want = ch and heating
        if not on and want and off_for < STANDSTILL: want = False
        if want and not on:
            starts += 1; on_for = 0; starts_day[m // 1440] = starts_day.get(m // 1440, 0) + 1
        if on and not want: run_lens.append(on_for)
        on = want
        if on: on_for += 1; off_for = 0
        else: off_for += 1
        qfloor = K * max(Ts - Ta, 0.0) ** h12.EXP_UFH
        pm = pmax(tout)
        P = min(pm, max(PMIN, qfloor + HP_GAIN * (cs - flow_now))) if on else 0.0
        # ---- hybrid backup (HP061 "Geen") ----
        if on and P >= pm - 1 and flow_now < cs - 2.0: deficit_min += 1
        else: deficit_min = 0
        if not boiler and deficit_min >= backup_delay(tout): boiler = True
        if boiler and (not ch or flow_now >= cs): boiler = False
        Pb = min(BOILER_MAX, max(BOILER_MIN, HP_GAIN * (cs - flow_now))) if boiler else 0.0
        flow = Ts + (P + Pb) / (2 * h12.MCP_UFH)
        if P > 0:
            cp_ = cop(flow, tout); elec += P * DT / cp_; heat_hp += P * DT
            run_min += 1
            if P <= PMIN + 1: at_min += 1
        if Pb > 0: gas += Pb * DT / BOILER_EFF; boiler_min += 1
        Ts += (P + Pb - qfloor) * DT / screed
        qaw = s3.G_AW * (Ta - Tw); solar = 1.5 * sw
        Ta += (qfloor + gains[m] + 0.5 * solar - qaw - Gao * (Ta - tout)) * DT / Ca
        Tw += (qaw + 0.5 * solar - Gwo * (Tw - tout)) * DT / Cw
        if heating and m >= 14 * 1440:
            e = Ta - TARGET; sq += e * e; n += 1; absum += abs(e)
            if abs(e) > 0.3: out += 1
    days = minutes / 1440
    return {"variant": variant, "coeff": c, "era": era, "screed": screed, "seed": seed,
            "starts_per_day": starts / days, "run_median_min": statistics.median(run_lens) if run_lens else 0,
            "mean_abs_err": absum / n if n else 0, "rms": math.sqrt(sq / n) if n else 0,
            "out_band_min_d": out / (n / 1440) if n else 0, "elec_kwh": elec / 3.6e6, "gas_kwh": gas / 3.6e6,
            "scop": heat_hp / elec if elec else 0, "share_at_pmin": at_min / run_min if run_min else 0,
            "run_hours": run_min / 60, "pwm_share": pwm_min / max(1, sum(1 for t in tm if t < 15)),
            "boiler_hours": boiler_min / 60}

def _init(w): s15._init(w, {})

ERAS = ("4_1992-2012", "5_2015", "6_BENG2021")
SCREEDS = (8.0e6, 3.0e6)
VARIANTS = ("sat", "own", "wc")

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    if "--sweep" in sys.argv:
        J = [(v, c, e, s, 1) for v in VARIANTS for c in (0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0) for e in ERAS for s in SCREEDS]
        with Pool(workers, initializer=_init, initargs=(os.path.join(arg("--seasondir"), "season_2025_26.json"),)) as p:
            res = p.map(run, J, chunksize=2)
        json.dump(res, open(os.path.join(arg("--outdir"), "round20_elga_sweep_runs.json"), "w")); print("sweep", len(res))
    else:
        best = json.loads(arg("--best"))   # e.g. '{"sat": 1.0, "own": 1.6, "wc": 1.4}'
        J = [(v, best[v], e, s, sd) for v in VARIANTS for e in ERAS for s in SCREEDS for sd in (1, 2)]
        res = {}
        for w in sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json"))):
            with Pool(workers, initializer=_init, initargs=(w,)) as p: res[os.path.basename(w)[7:-5]] = p.map(run, J, chunksize=2)
        json.dump(res, open(os.path.join(arg("--outdir"), "round20_elga_runs.json"), "w")); print("validation done")
