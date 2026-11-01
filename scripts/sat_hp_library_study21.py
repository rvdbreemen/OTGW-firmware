"""TASK-1527 round 21 (2026-11-01): a library of ten heat pumps and hybrids from the Dutch / EU market, simulated with
SAT through the proven port (scripts/sat_sim_port.py, tests/test_sat_sim_port_equivalence.py), over ten full heating
seasons (1 Oct - 30 Apr, 2015-16 .. 2024-25, Open-Meteo archive, 52.13 N 6.13 E).

Every unit is a parameter set for one generic modulating plant (round 20). Sources per unit are in UNITS[*]["src"];
values marked "A:" are assumptions because the manufacturer does not publish them.
Generic assumptions (all units): own flow control P = clamp(q_floor + 1000 W/K x (CS - flow), PMIN, PMAX); the unit
stops when the flow is 3 K above CS at PMIN and restarts 1 K below CS; own minimum run 10 min and standstill 10 min
unless a source gives it; COP at W35 interpolated between the published points (A7, A2, A-7; A2 = 0.73 x A7 when not
published, the mean ratio of Elga Ace, Panasonic J and Mitsubishi WM50), Carnot-scaled for other flow temperatures;
hybrids: boiler backup after the heat pump is at PMAX with the flow 2 K under CS for 8-30 min (Elga Ace HP047-050),
boiler 3.1-26 kW, efficiency 0.97; room sensor resolution 0.1 C; CH = 0 stops the unit.

Variants:
- "sat": SAT as decided for heat pumps (entries 87, 197, 198): continuous CS = PID output; the overshoot switch
  (flow >= CS + 2 K for 60 s) enters PWM, period 1800 s, minimum ON and OFF 600 s, the ON-phase setpoints of
  thermo-nova's overshoot protection (return + 18 C before the start, then flow - 1 C), OFF phase CS = 10 and CH = 0;
  PWM leaves after 5 min of saturation.
- "cont": the same SAT curve and PID, SAT never switches to PWM (the unit modulates and cycles by itself).
Run: python scripts/sat_hp_library_study21.py --seasondir docs/sat/sim-hp-lowload/weather_season10 --outdir <dir>
     [--sweep] [--best <json>] [--workers N]
"""
import glob, json, math, os, random, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
import sat_hp_onoff_study12 as h12
import sat_hp_season_study15 as s15
import sat_sim_port as port
from sat_curve_selftune_study import TARGET, DT

# pmax: list of (outdoor C, kW); cop35: list of (outdoor C, COP at W35); pmin in kW
UNITS = {
    "remeha_elga_ace_4": {"type": "hybrid", "pmin": 1.3, "pmax": [(-7, 3.5), (2, 4.45), (7, 4.45)],
        "cop35": [(2, 4.05), (7, 5.4)], "standstill": 10,
        "src": "Remeha product datasheet Elga Ace All-in-one p.9 (A7/W35 4.45 kW COP 5.4; A2/W35 4.45 kW COP 4.05); "
               "installation manual 7868458-02 Tab.17 p.20 (Pdh +12 C 1.3 kW, -7 C 3.5 kW), p.118 (HP155 standstill 10 min)"},
    "intergas_xtend_5": {"type": "hybrid", "pmin": 1.8, "pmax": [(7, 5.0)], "cop35": [(7, 4.68)],
        "src": "Intergas Xtend brochure (2022/2024): A7/W35 5.00 kW, modulation range 1.8-5 kW, COP 4.68 (monoblock) / 4.5 (split); "
               "A: A2 COP = 0.73 x A7, PMAX flat"},
    "atag_energion_m_hybrid_5": {"type": "hybrid", "pmin": 1.5, "pmax": [(-7, 5.0), (7, 4.4)],
        "cop35": [(-7, 3.06), (7, 5.02)],
        "src": "ATAG Energion M datasheet: A7/W35 4.4 kW COP 5.02, A-7/W35 5.0 kW COP 3.06; "
               "A: PMIN 1.5 kW from the Energion M Hybrid Light 50 leaflet (power range 1.5-6.5 kW), not a confirmed minimum for this variant"},
    "bosch_compress_7000i_aw_5": {"type": "hybrid", "pmin": 2.0, "pmax": [(2, 6.0)], "cop35": [(7, 4.7)],
        "src": "Bosch Compress 7000i AW datasheet (PL, 06.2022, via search): A2/W35 range 2-6 kW; Bosch CH page: COP A7/W35 4.7 "
               "(used in the Bosch/Nefit Hybrid 7000i). Secondary: not read in a primary PDF; A: A2 COP = 0.73 x A7"},
    "mitsubishi_ecodan_puz_wm50": {"type": "electric", "pmin": 1.8, "pmax": [(-15, 3.9), (-7, 5.0), (7, 5.6)],
        "cop35": [(-7, 3.0), (2, 3.7), (7, 5.0)],
        "src": "Mitsubishi Electric product sheet ERPX-VM6D / PUZ-WM50VHA: +7/35 min-nom-max 1.80-5.00-5.60 kW, COP 5.00; "
               "-7/35 5.0 kW; -15/35 3.9 kW; UK PI sheet: A2/W35 COP 3.70, A-7/W35 COP 3.00"},
    "vaillant_arotherm_plus_7": {"type": "electric", "pmin": 3.25, "pmax": [(7, 7.37)], "cop35": [(2, 3.18), (7, 4.42)],
        "src": "HP Keymark summary VWL 75/6 A 230V (reg. 40050985): EN 14511 low temperature 7.37 kW COP 4.42; "
               "Pdh +7 C 3.25 kW, +12 C 3.73 kW (colder climate, above demand: minimum); A2 point from the warmer-climate Pdh table"},
    "nibe_s2125_8": {"type": "electric", "pmin": 2.4, "pmax": [(7, 6.7)], "cop35": [(7, 5.18)],
        "src": "NIBE S2125 datasheet NL/FR (via search): output 2.4-6.7 kW at 7/35, 3.15 kW at COP 5.18; secondary; "
               "A: A2 COP = 0.73 x A7; NIBE F1345 manual p.18 timer rule 5 min stop-start, 20 min start-start (A: same family)"},
    "daikin_altherma3_r_erga04": {"type": "electric", "pmin": 3.2, "pmax": [(-7, 5.5), (7, 4.3)],
        "cop35": [(-7, 2.90), (2, 4.33), (7, 5.10)],
        "src": "Daikin data book ERGA04-08EV EEDEN20 p.5-7: EHBH04+ERGA04 4.30 kW COP 5.10 at A7/W35; 35 C ErP: Pdh +7 C 3.2 kW "
               "(demand 2.1 kW: minimum), +12 C 3.3 kW, -7 C 5.5 kW COPd 2.90, +2 C COPd 4.33 (part load)"},
    "daikin_altherma3_r_erga08": {"type": "electric", "pmin": 3.3, "pmax": [(-7, 7.0), (7, 7.5)],
        "cop35": [(-7, 2.77), (2, 4.35), (7, 4.60)],
        "src": "Daikin data book ERGA04-08EV p.5-7: EHBH08+ERGA08 7.50 kW COP 4.60 at A7/W35; 35 C ErP: Pdh +7 C 3.3, +12 C 3.9 kW "
               "(column mapping of merged cells read as 04/06 | 08, reading ours), -7 C 7.0 kW COPd 2.77"},
    "panasonic_aquarea_j_7": {"type": "electric", "pmin": 3.4, "pmax": [(-7, 6.8), (2, 7.0), (7, 7.0)],
        "cop35": [(-7, 2.81), (2, 3.40), (7, 4.76)],
        "src": "Panasonic EU model page WH-MDC07J3E5: A7/W35 7.00 kW COP 4.76, A2/W35 7.00 kW COP 3.40, A-7/W35 6.80 kW COP 2.81; "
               "HP Keymark (via search, 55 C): Pdh +12 C 3.40 kW: used as minimum (secondary)"},
}

ERAS = ("4_1992-2012", "5_2015", "6_BENG2021")
HOUSES = [(e, 8.0e6) for e in ERAS] + [("6_BENG2021", 3.0e6)]
HP_STOP_K, HP_START_K, HP_GAIN = 3.0, 1.0, 1000.0
BOILER_MIN, BOILER_MAX, BOILER_EFF = 3100.0, 26100.0, 0.97
PWM_PERIOD_S, PWM_MINON_S, PWM_MINOFF_S = 1800, 600, 600
SENSOR_RES = 0.1
# Round 22 (2026-11-01, entry 203): how the boiler backs up a hybrid. "native": the heat pump's own control starts the
# boiler (after the HP047-050 delay) and the boiler modulates from its minimum; "slave": Sergeant D's proposal, a slave
# SAT runs the boiler with the gas PWM (3 cycles per hour, 1200 s period, 180 s minimum ON, at minimum modulation,
# thermo-nova pwm.py:53-54) on the heat the heat pump cannot deliver.
HYBRID_MODE = "native"
GAS_PERIOD_S, GAS_MINON_S, GAS_MAX_STARTS_H = 1200, 180, 3
# Round 22: PLANT "rad" = radiators: one water/radiator node of capacity `screed` (pass about 0.6e6 J/K, A), emitter
# exponent 1.3, K calibrated so curve 1.6 balances the season mean (as h12.k_for for underfloor), base offset 27.2,
# cold setpoint 28.2, SAT max setpoint 62 C for a hybrid (entry 188) and 40 C for a heat pump (entry 155); a hybrid's
# SAT PWM follows entry 188: heat-pump cycle 1800 s, gas minimum ON 180 s, no minimum OFF, at most 3 starts per hour.
PLANT = "ufh"
RAD_EXP = 1.3
# Round 22 (entry 208): SEPARATOR = True gives the boiler its own short loop behind a hydraulic separator (Elga Ace):
# loop capacity SEP_C (A, about 7 l of water plus the heat exchanger), boiler pump SEP_G W/K while the boiler fires (A),
# a small standing loss to the system when it does not. The boiler controls on its own loop flow; the system receives
# the separator transfer, not the burner power.
SEPARATOR = False
SEP_C, SEP_G, SEP_G_OFF, MCP_BOILER = 30000.0, 930.0, 20.0, 930.0


def interp(points, t):
    pts = sorted(points)
    if t <= pts[0][0]: return pts[0][1]
    if t >= pts[-1][0]: return pts[-1][1]
    for (a, va), (b, vb) in zip(pts, pts[1:]):
        if a <= t <= b: return va + (vb - va) * (t - a) / (b - a)
    return pts[-1][1]


def cop35(u, t):
    pts = list(u["cop35"])
    if len(pts) == 1: pts = [(2, 0.73 * pts[0][1]), (7, pts[0][1])]
    if min(p[0] for p in pts) > -7:   # extrapolate below the coldest point with the defrost slope of round 20
        tmin, cmin = min(pts); pts.append((-15, max(1.5, cmin - 0.15 * (tmin + 15))))
    c = interp(pts, t)
    if t > max(p[0] for p in pts): c = min(6.5, c + 0.1 * (t - max(p[0] for p in pts)))
    return c


def cop(u, flow, t):
    carnot = lambda f: (f + 273.15) / max(f - t + 10.0, 5.0)
    return max(1.0, cop35(u, t) * carnot(flow) / carnot(35.0))


def backup_delay(t):
    return 8 if t <= -10 else 30 if t >= 15 else 8 + 22 * (t + 10) / 25.0


def run(args):
    unit_name, variant, c, era, screed = args
    u = UNITS[unit_name]; pmin = u["pmin"] * 1000.0; standstill = u.get("standstill", 10); minrun = 10
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, "heavy", 1.0)
    season = statistics.fmean(t for t in tm if t < 15)
    ufh = PLANT == "ufh"; hybrid = u["type"] == "hybrid"
    base_off = 20.0 if ufh else 27.2; cold_sp = 21.0 if ufh else 28.2
    spmax = h12.SP_MAX if (ufh or not hybrid) else 62.0; exp_e = h12.EXP_UFH if ufh else RAD_EXP
    if ufh: K = h12.k_for(Heff, season)
    else:
        Qs = max(Heff * (TARGET - season) - 135.0, 50.0); fs = float(port.curve(1.6, TARGET, season, 27.2))
        K = Qs / max(fs - Qs / (2 * h12.MCP_UFH) - TARGET, 0.5) ** RAD_EXP
    pwm_minon = 180 if hybrid else PWM_MINON_S; pwm_minoff = 0 if hybrid else PWM_MINOFF_S; pwm_starts = []
    rnd = random.Random(1); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    Ta = TARGET; Tw = (s3.G_AW * TARGET + Gwo * season) / (s3.G_AW + Gwo)
    Ts = float(port.curve(c, TARGET, season, base_off)) - 2.0
    pid = port.Pid(c, ufh)
    on = False; on_for = 0; off_for = 999; starts = 0; run_lens = []
    P = 0.0; boiler = False; deficit = 0; b_starts = 0; b_starts_t = []; b_el = 99; b_last_stop = None
    Tb = None; Pb_prev = 0.0
    spare = 0; b_req = False; s_pwm = False; s_on = False; s_el = 0; s_ov = 0; s_sat = s_uh = 0; s_eff = 0.0
    pwm = False; ov = 0; sat_s = 0; ph_on = False; ph_el = 0; eff_on = None
    sq = n = out = absum = 0.0; elec = gas = heat_hp = 0.0; run_min = at_min = pwm_min = heat_min = boiler_min = 0
    for m in range(minutes):
        tout, sw = tm[m], sm[m]; heating = tout < 15
        room_meas = round(Ta / SENSOR_RES) * SENSOR_RES
        cv = float(port.curve(c, TARGET, tout, base_off))
        now = 1000000 + m * 60000
        pid.service(now, room_meas, TARGET, cv); pid.note_room(room_meas, now)
        sp = min(spmax, max(10.0, float(pid.output(cv)))) if heating else 10.0
        flow_now = Ts + P / (2 * h12.MCP_UFH); ret_now = Ts - P / (2 * h12.MCP_UFH)
        cs, ch = sp, heating and sp > cold_sp
        if heating: heat_min += 1
        if variant == "sat" and heating and sp > cold_sp:
            if not pwm:
                ov = ov + 1 if (on and flow_now >= sp + 2.0) else 0
                if ov >= 1: pwm = True; ov = 0; ph_on = True; ph_el = 0; sat_s = 0; eff_on = None
            if pwm:
                pwm_min += 1
                ref = eff_on if eff_on else max(flow_now, cold_sp)
                duty = min(max((sp - base_off) / max(ref - base_off, 1.0), 0.0), 1.0)
                on_t, off_t, cont = port.pwm_times(duty, PWM_PERIOD_S, pwm_minon, on)
                on_t /= 60.0; off_t /= 60.0
                if ph_on:
                    if not cont and ph_el >= on_t: ph_on = False; ph_el = 0
                elif (ph_el >= pwm_minoff / 60 and (cont or (on_t >= 3 and ph_el >= off_t))
                        and (not hybrid or len([t for t in pwm_starts if m - t < 60]) < 3)):
                    ph_on = True; ph_el = 0; pwm_starts.append(m)
                ph_el += 1
                sat_s = sat_s + 1 if cont else 0
                if sat_s >= 5: pwm = False; sat_s = 0; eff_on = None
                if ph_on:
                    if not on: cs = min(ret_now + 18.0, spmax); eff_on = None
                    else:
                        if eff_on is None or on_for <= 3: eff_on = flow_now if eff_on is None else 0.3 * flow_now + 0.7 * eff_on
                        cs = max(10.0, flow_now - 1.0)
                    ch = True
                else:
                    cs, ch = 10.0, False
        elif not heating or sp <= cold_sp:
            pwm = False
        if not ch or not heating: want = False
        elif on: want = not (P <= pmin + 1 and flow_now > cs + HP_STOP_K)
        else: want = flow_now < cs - HP_START_K
        if on and not want and on_for < minrun: want = ch and heating
        if not on and want and off_for < standstill: want = False
        if want and not on: starts += 1; on_for = 0
        if on and not want: run_lens.append(on_for)
        on = want
        if on: on_for += 1; off_for = 0
        else: off_for += 1
        qfloor = K * max(Ts - Ta, 0.0) ** exp_e
        pm = interp([(t, v * 1000.0) for t, v in u["pmax"]], tout)
        P = min(pm, max(pmin, qfloor + HP_GAIN * (cs - flow_now))) if on else 0.0
        Pb = 0.0
        if Tb is None: Tb = Ts
        fb_now = (Tb + Pb_prev / (2 * MCP_BOILER)) if SEPARATOR else flow_now
        if u["type"] == "hybrid":
            was = boiler
            if HYBRID_MODE in ("native", "native_nodelay"):
                deficit = deficit + 1 if (on and P >= pm - 1 and flow_now < cs - 2.0) else 0
                # "native_nodelay" (sensitivity): the HP047-050 delay applies once per heat-pump run; after a boiler stop
                # the boiler may restart after a 3 min anti-cycling time (A)
                thr = backup_delay(tout)
                stopped_this_run = b_last_stop is not None and on and m - b_last_stop < on_for
                if HYBRID_MODE == "native_nodelay" and stopped_this_run: thr = 3
                if not boiler and deficit >= thr: boiler = True
                if boiler and (not ch or fb_now >= cs): boiler = False; b_last_stop = m
                Pb = min(BOILER_MAX, max(BOILER_MIN, HP_GAIN * (cs - flow_now))) if boiler else 0.0
            else:
                # "own" / "slave" (entries 206/207). The heat pump's backup request (CH=1 + its CS to the boiler): starts
                # after the HP047-050 delay with the heat pump at full output and the flow 2 K short of CS; ends when the
                # heat pump has had spare capacity for 15 min or CH ends (A).
                deficit = deficit + 1 if (on and P >= pm - 1 and flow_now < cs - 2.0) else 0
                spare = spare + 1 if (P < pm - 1) else 0
                if not b_req and ch and deficit >= backup_delay(tout): b_req = True
                if b_req and (not ch or spare >= 15): b_req = False
                need = qfloor + HP_GAIN * (cs - flow_now) - P
                if HYBRID_MODE == "own":
                    # the boiler's own control on the heat pump's CS (A): on below CS - 2 K after 3 min off, off above CS + 2 K
                    b_el += 1
                    if boiler and (not b_req or fb_now >= cs + 2.0): boiler = False; b_el = 0
                    elif not boiler and b_req and fb_now < cs - 2.0 and b_el >= 3: boiler = True; b_el = 0
                    Pb = min(BOILER_MAX, max(BOILER_MIN, need)) if boiler else 0.0
                else:
                    # slave SAT: requested setpoint = the heat pump's CS; continuous until the overshoot switch, then the
                    # gas PWM (period 1200 s, min ON 180 s, 3 starts/h, ON phase at minimum modulation), back through the
                    # saturation (300 s) and underheat (180 s) exits (thermo-nova heating_control.py:262-329, :358-376)
                    if not b_req:
                        boiler = False; s_pwm = False; s_ov = 0; b_el = 0
                    elif not s_pwm:
                        b_el += 1
                        s_ov = s_ov + 1 if (boiler and fb_now >= cs + 2.0) else 0
                        if s_ov >= 1: s_pwm = True; s_on = True; s_el = 0; s_sat = s_uh = 0; s_eff = fb_now
                        if not boiler and fb_now < cs - 2.0 and b_el >= 3: boiler = True; b_el = 0   # continuous: the boiler's own on/off
                        elif boiler and fb_now >= cs + 2.0 and not s_pwm: boiler = False; b_el = 0
                        Pb = min(BOILER_MAX, max(BOILER_MIN, need)) if boiler else 0.0
                    if b_req and s_pwm:
                        duty_b = min(max((cs - base_off) / max(s_eff - base_off, 1.0), 0.0), 1.0)
                        on_b, off_b, cont_b = port.pwm_times(duty_b, GAS_PERIOD_S, GAS_MINON_S, boiler)
                        s_el += 1
                        if s_on:
                            if not cont_b and s_el * 60 >= on_b: s_on = False; s_el = 0
                        elif (cont_b or (on_b >= 180 and s_el * 60 >= off_b)) and len([t for t in b_starts_t if m - t < 60]) < GAS_MAX_STARTS_H:
                            s_on = True; s_el = 0
                        boiler = s_on
                        if boiler and s_el <= 3: s_eff = fb_now if s_el <= 1 else 0.3 * fb_now + 0.7 * s_eff
                        s_sat = s_sat + 1 if cont_b else 0
                        s_uh = s_uh + 1 if (s_on and fb_now <= cs - 2.0) else 0
                        if s_sat >= 5 or s_uh >= 3: s_pwm = False; b_el = 99
                        Pb = BOILER_MIN if boiler else 0.0   # ON phase: CS = flow - 1, the boiler at its minimum
            if boiler and not was: b_starts += 1; b_starts_t.append(m)
        if SEPARATOR:
            g = SEP_G if Pb > 0 else SEP_G_OFF
            teq = Ts + Pb / g
            tb_new = teq + (Tb - teq) * math.exp(-g * DT / SEP_C)
            q_sys = Pb - SEP_C * (tb_new - Tb) / DT; Tb = tb_new
        else:
            q_sys = Pb
        Pb_prev = Pb
        flow = Ts + (P + q_sys) / (2 * h12.MCP_UFH)
        if P > 0:
            elec += P * DT / cop(u, flow, tout); heat_hp += P * DT; run_min += 1
            if P <= pmin + 1: at_min += 1
        if Pb > 0: gas += Pb * DT / BOILER_EFF; boiler_min += 1
        Ts += (P + q_sys - qfloor) * DT / screed
        qaw = s3.G_AW * (Ta - Tw); solar = 1.5 * sw
        Ta += (qfloor + gains[m] + 0.5 * solar - qaw - Gao * (Ta - tout)) * DT / Ca
        Tw += (qaw + 0.5 * solar - Gwo * (Tw - tout)) * DT / Cw
        if heating and m >= 14 * 1440:
            e = Ta - TARGET; sq += e * e; n += 1; absum += abs(e)
            if abs(e) > 0.3: out += 1
    days = minutes / 1440
    return {"unit": unit_name, "variant": variant, "coeff": c, "era": era, "screed": screed,
            "starts_per_day": starts / days, "run_median_min": statistics.median(run_lens) if run_lens else 0,
            "mean_abs_err": absum / n if n else 0, "out_band_min_d": out / (n / 1440) if n else 0,
            "elec_kwh": elec / 3.6e6, "gas_kwh": gas / 3.6e6, "scop": heat_hp / elec if elec else 0,
            "share_at_pmin": at_min / run_min if run_min else 0, "pwm_share": pwm_min / max(1, heat_min),
            "boiler_hours": boiler_min / 60, "boiler_starts": b_starts,
            "boiler_run_mean_min": (boiler_min / b_starts) if b_starts else 0, "hybrid_mode": HYBRID_MODE, "plant": PLANT, "separator": SEPARATOR}


def _init(w): s15._init(w, {})


if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    workers = int(arg("--workers")) if "--workers" in sys.argv else 4
    seasons = sorted(glob.glob(os.path.join(arg("--seasondir"), "season_*.json")))
    if "--sweep" in sys.argv:
        J = [(un, v, c, e, s) for un in UNITS for v in ("sat", "cont") for c in (1.0, 1.6) for e, s in HOUSES]
        w = [s for s in seasons if "2024_25" in s][0]
        with Pool(workers, initializer=_init, initargs=(w,)) as p: res = p.map(run, J, chunksize=1)
        json.dump(res, open(os.path.join(arg("--outdir"), "round21_library_sweep_runs.json"), "w")); print("sweep", len(res))
    else:
        best = json.loads(open(arg("--best")).read())   # {"unit|variant": coeff}
        J = [(un, v, best[f"{un}|{v}"], e, s) for un in UNITS for v in ("sat", "cont") for e, s in HOUSES]
        part = os.path.join(arg("--outdir"), "round21_parts"); os.makedirs(part, exist_ok=True); res = {}
        for w in seasons:
            label = os.path.basename(w)[7:-5]; dest = os.path.join(part, label + ".json")
            if os.path.exists(dest): res[label] = json.load(open(dest)); continue
            with Pool(workers, initializer=_init, initargs=(w,)) as p: res[label] = p.map(run, J, chunksize=1)
            json.dump(res[label], open(dest, "w")); print(label, "done", flush=True)
        json.dump(res, open(os.path.join(arg("--outdir"), "round21_library_runs.json"), "w")); print("done")
