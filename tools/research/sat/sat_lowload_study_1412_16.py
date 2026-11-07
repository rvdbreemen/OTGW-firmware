"""TASK-1412.16 (LL-016, review items R12/R13/R42): the repaired SAT duty estimate and PID against controlled plant
models, compared with simple bounded baselines on identical scenarios. Simulation only: no energy, COP or comfort claim
follows from it, and no controller change is proposed by it (AC3).

What is SAT here (proven vs modelled, see docs/sat/sim-1412-16/report.md for the table):
- proven equal to the firmware by tests/test_sat_sim_port_equivalence.py (4/4 on b1fe73d2e): heating curve, PID
  (SATpid.ino), PWM duty-range mapper (satApplyPWM), via tools/research/sat/sat_sim_port.py;
- modelled after the firmware/thermo-nova, NOT proven equal: the cold cutoff with 0.5 K latch (SATchEnable.h,
  SAT_COLD_HYSTERESIS_C), the 60 s overshoot switch at flow >= CS + 2 (entries 22/41), the duty formula
  (pid - base) / (Teff - base) with Teff reseeded at ignition and averaged 0.3/0.7 for 180 s (SATcontrol.ino:1398-1439,
  thermo-nova pwm.py:130-168, 229-237), the ON-phase setpoints (return + 18 before ignition and for 20 s after it, then flow - 1,
  heating_control.py:385-448, const.py:151-153), OFF phase CS=10/CH=0, 3 starts per hour, the 5 min saturation exit;
- absent: DHW, sensor loss, transport queues, presets, solar offset, multi-zone.

Plant (model limits in the report): two-node house of round 3 (tools/research/sat/sat_curve_selftune_study3.py), one radiator water
node 0.6 MJ/K with exponent 1.3 (K balanced so curve 1.6 carries the house at 5 C outdoor), or an underfloor screed
node (round 12 values); gas boiler with its own on/off and flow control (start below CS - 2 K after 3 min standstill,
stop above CS + 5 K at minimum output (assumption, sensitivity 3 K); output = emitter draw + 1000 W/K x (CS - flow), between Pmin and 24 kW). In PWM the
boiler either honours MM=0 (runs at Pmin) or ignores MsgID 14 and modulates by itself (gruijter's Intergas, 2026-11-03/04).
Flow: 'fixed' (constant pump) or 'variable' (TRV throttling: emitter conductance and pump flow scale with the room
error, minimum 15 %).

Run: python tools/research/sat/sat_lowload_study_1412_16.py --out docs/sat/sim-1412-16 [--quick]
"""
import argparse, hashlib, json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_sim_port as port
import sat_curve_selftune_study3 as s3

DT = 10.0                      # plant step, s
TARGET = 20.0
GAS_PERIOD_S, GAS_MINON_S, GAS_MAX_STARTS_H = 1200, 180, 3   # gas PWM, entries 30/52/80
OV_MARGIN = 2.0                # entries 22/37/41
COLD_HYST = 0.5                # SAT_COLD_HYSTERESIS_C, entry 199
COLD_CUT_MIN_BURN = False      # TASK-1652 (entry 275): a lit PWM ON phase burns 180 s before the cold cutoff ends it; False = today
PMAX = 24000.0
BOILER_STOP_K = 5.0          # A: the boiler stops at CS + 5 K at minimum output (sensitivity 3 K)
BOILER_START_K = 2.0
MCP0 = 628.0                   # radiator loop water flow, W/K (round 4)
C_RAD = 0.6e6                  # radiator water/steel node, J/K
UFH_C, UFH_MCP, UFH_EXP = 8.0e6, 1050.0, 1.1


def emitter(kind):
    if kind == "rad":
        return {"base": 27.2, "cold": 28.2, "C": C_RAD, "mcp": MCP0, "exp": 1.3, "ufh": False, "spmax": 62.0}
    return {"base": 20.0, "cold": 21.0, "C": UFH_C, "mcp": UFH_MCP, "exp": UFH_EXP, "ufh": True, "spmax": 50.0}


def calibrate_K(e, Heff, coeff=1.6, to_ref=5.0):
    """K so that the curve at coeff carries the steady loss at to_ref (mean water = curve - half the loop drop)."""
    q = Heff * (TARGET - to_ref)
    f = float(port.curve(coeff, TARGET, to_ref, e["base"]))
    return q / max(f - q / (2 * e["mcp"]) - TARGET, 0.5) ** e["exp"]


SCENARIOS = {
    # name: (hours, outdoor(t_h), gains_W(t_h), room start offset K)
    "steady_m5": (24, lambda t: -5.0, lambda t: 0.0, 0.0),
    "steady_5": (24, lambda t: 5.0, lambda t: 0.0, 0.0),
    "steady_12": (24, lambda t: 12.0, lambda t: 0.0, 0.0),
    "warmup_5": (12, lambda t: 5.0, lambda t: 0.0, -2.5),
    "falling_demand": (24, lambda t: 2.0 + min(max(t - 2, 0) / 6.0, 1.0) * 10.0, lambda t: 0.0, 0.0),
    "gain_step_8": (24, lambda t: 8.0, lambda t: 1500.0 if 6 <= t < 10 else 0.0, 0.0),
}


class Boiler:
    def __init__(self, pmin, mm_honoured):
        self.pmin = pmin; self.mm = mm_honoured; self.on = False; self.off_s = 999.0; self.on_s = 0.0; self.starts = 0

    def step(self, cs, ch, flow, draw, pwm_active):
        start_ok = self.off_s >= 180.0
        if not ch:
            want = False
        elif self.on:
            p_try = min(PMAX, max(self.pmin, draw + 1000.0 * (cs - flow)))
            want = not (p_try <= self.pmin + 1 and flow > cs + BOILER_STOP_K)
        else:
            want = start_ok and flow < cs - BOILER_START_K
        if want and not self.on: self.starts += 1; self.on_s = 0.0
        self.on = want
        if self.on: self.on_s += DT; self.off_s = 0.0
        else: self.off_s += DT
        if not self.on: return 0.0
        if pwm_active and self.mm: return self.pmin
        return min(PMAX, max(self.pmin, draw + 1000.0 * (cs - flow)))


class SatCtl:
    """SAT as described in the module docstring. Tick 30 s; PID through the proven port (60 s cadence inside)."""
    name = "sat"

    def __init__(self, e, coeff):
        self.e = e; self.pid = port.Pid(coeff, e["ufh"]); self.c = coeff
        self.cut = False; self.pwm = False; self.ov_s = 0.0; self.ph_on = False; self.ph_el = 0.0; self.sat_s = 0.0
        self.teff = None; self.flame_at = None; self.starts = []; self.cs = 10.0; self.ch = False; self.duty = None
        self.hold = None
        self.log = []

    def tick(self, now_s, room, tout, flow, ret, flame):
        e = self.e
        cv = float(port.curve(self.c, TARGET, tout, e["base"]))
        ms = int(1000000 + now_s * 1000)
        self.pid.service(ms, room, TARGET, cv); self.pid.note_room(room, ms)
        sp = min(e["spmax"], max(10.0, float(self.pid.output(cv))))
        self.cut = (sp <= e["cold"] + COLD_HYST) if self.cut else (sp <= e["cold"])
        # TASK-1652: a lit ON phase (flame_at None = it lit since the last tick) is not cut before 180 s since its ignition
        if self.cut and not (COLD_CUT_MIN_BURN and self.pwm and self.ph_on and flame
                             and (self.flame_at is None or now_s - self.flame_at < 180.0)):
            self.pwm = False; self.cs, self.ch = 10.0, False; self.duty = None; return sp
        if not self.pwm:
            self.ov_s = self.ov_s + 30.0 if (flame and flow >= sp + OV_MARGIN) else 0.0
            if self.ov_s >= 60.0:
                self.pwm = True; self.ov_s = 0.0; self.ph_on = True; self.ph_el = 0.0; self.sat_s = 0.0
                self.teff = None; self.flame_at = None; self.starts.append(now_s)
            else:
                self.cs, self.ch = sp, True; self.duty = None; return sp
        # PWM
        if flame and self.teff is None: self.teff = flow
        if self.ph_on and flame:
            if self.flame_at is None: self.flame_at = now_s; self.teff = flow
            elif now_s - self.flame_at <= 180.0: self.teff = 0.3 * flow + 0.7 * self.teff
        ref = max(self.teff if self.teff else flow, e["base"] + 1.0)
        duty = min(max((sp - e["base"]) / (ref - e["base"]), 0.0), 1.0); self.duty = duty
        on_t, off_t, cont = port.pwm_times(duty, GAS_PERIOD_S, GAS_MINON_S, flame)
        self.ph_el += 30.0
        if self.ph_on:
            if not cont and self.ph_el >= on_t: self.ph_on = False; self.ph_el = 0.0; self.flame_at = None
        elif cont or (on_t >= 180 and self.ph_el >= off_t):
            if len([t for t in self.starts if now_s - t < 3600]) < GAS_MAX_STARTS_H:
                self.ph_on = True; self.ph_el = 0.0; self.starts.append(now_s); self.flame_at = None
        self.sat_s = self.sat_s + 30.0 if cont else 0.0
        if self.sat_s >= 300.0: self.pwm = False; self.sat_s = 0.0
        if self.ph_on:
            if not flame:
                self.hold = min(max(ret + 18.0, 22.0), e["spmax"]); self.cs = self.hold
            elif self.flame_at is not None and now_s - self.flame_at < 20.0 and self.hold is not None:
                self.cs = self.hold
            else:
                self.hold = None; self.cs = max(10.0, flow - 1.0)
            self.ch = True
        else:
            self.cs, self.ch = 10.0, False
        return sp


class PiCtl:
    """Bounded PI on the room error -> flow setpoint (curve as feed-forward), output clamped to [cold, spmax],
    back-calculation anti-windup (tracking gain 1/Tt). A textbook baseline, not a proposal."""
    name = "pi"

    def __init__(self, e, coeff, kp=6.0, ti_h=2.0):
        self.e = e; self.c = coeff; self.kp = kp; self.ki = kp / (ti_h * 3600.0); self.i = 0.0; self.cs = 10.0
        self.ch = False; self.duty = None

    def tick(self, now_s, room, tout, flow, ret, flame):
        e = self.e; cv = float(port.curve(self.c, TARGET, tout, e["base"]))
        err = TARGET - room; u = cv + self.kp * err + self.i
        us = min(e["spmax"], max(e["cold"], u))
        self.i += 30.0 * (self.ki * err + (us - u) / 1800.0)
        self.cs, self.ch = (us, True) if us > e["cold"] else (10.0, False)
        return us


class TpiCtl:
    """Time-proportional room on/off (cycle and proportional band tuned, integral 1 h clamped to +-band/2 and frozen
    while the duty is saturated), fixed CS 60 C (radiators) / 40 C (underfloor) during the ON part (A)."""
    name = "tpi"

    def __init__(self, e, coeff, cycle=600.0, band=1.5):
        self.e = e; self.c = coeff; self.cycle = cycle; self.band = band; self.i = 0.0; self.cs = 10.0; self.ch = False
        self.duty = None

    def tick(self, now_s, room, tout, flow, ret, flame):
        e = self.e; cv = float(port.curve(self.c, TARGET, tout, e["base"]))
        err = TARGET - room
        raw = 0.5 + (err + self.i) / self.band
        if 0.0 < raw < 1.0 or (raw >= 1.0 and err < 0) or (raw <= 0.0 and err > 0):   # freeze while saturated
            lim = self.band / 2.0
            self.i = min(lim, max(-lim, self.i + 30.0 * err / 3600.0))
        d = min(1.0, max(0.0, 0.5 + (err + self.i) / self.band)); self.duty = d
        on = (now_s % self.cycle) < d * self.cycle
        fixed = 40.0 if e["ufh"] else 60.0          # A: a conventional on/off install runs a fixed boiler setpoint
        self.cs, self.ch = (fixed, True) if on else (10.0, False)
        return fixed


class WcCtl:
    """Native weather compensation (curve only), optionally with room influence k x (target - room)."""
    def __init__(self, e, coeff, k_room=0.0):
        self.e = e; self.c = coeff; self.k = k_room; self.cs = 10.0; self.ch = False; self.duty = None
        self.name = "wc" if k_room == 0 else "wc_room"

    def tick(self, now_s, room, tout, flow, ret, flame):
        e = self.e; cv = float(port.curve(self.c, TARGET, tout, e["base"])) + self.k * (TARGET - room)
        cv = min(e["spmax"], cv)
        self.cs, self.ch = (cv, True) if cv > e["cold"] else (10.0, False)
        return cv


TUNE_GRID = {   # small published grid per baseline; the best (lowest summed RMS room error over the tuning scenarios) wins
    "pi": [{"kp": kp, "ti_h": ti} for kp in (3.0, 6.0, 12.0, 24.0) for ti in (0.5, 1.0, 2.0, 4.0)],
    "tpi": [{"band": b, "cycle": c} for b in (0.5, 1.0, 1.5, 3.0) for c in (600.0, 1200.0)],
    "wc_room": [{"k_room": k} for k in (1.0, 3.0, 6.0, 10.0, 15.0)],
    "wc": [{}]}
TUNE_SCENARIOS = ("steady_5", "steady_12", "gain_step_8")


def make_ctl(name, e, coeff, params=None):
    p = params or {}
    if name == "sat": return SatCtl(e, coeff)
    if name == "pi": return PiCtl(e, coeff, **p)
    if name == "tpi": return TpiCtl(e, coeff, **p)
    if name == "wc": return WcCtl(e, coeff, 0.0)
    if name == "wc_room": return WcCtl(e, coeff, p.get("k_room", 3.0))
    raise ValueError(name)


def simulate(scn, ctl_name, emitter_kind, flow_kind, era, mass, pmin_kw, mm_honoured, coeff=1.6, heff_scale=1.0,
             k_scale=1.0, trace=False, params=None):
    hours, f_out, f_gain, d0 = SCENARIOS[scn]
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, mass, 1.0)
    Gao *= heff_scale; Gwo *= heff_scale; Heff *= heff_scale
    e = emitter(emitter_kind)
    K = calibrate_K(e, Heff / heff_scale) * k_scale     # the installer balanced for the nominal house
    to0 = f_out(0.0)
    # steady start: room at target + d0, wall at its equilibrium for that room, water at the curve
    Ta = TARGET + d0; Tw = (s3.G_AW * Ta + Gwo * to0) / (s3.G_AW + Gwo)
    Tr = max(Ta, float(port.curve(coeff, TARGET, to0, e["base"])) - 2.0)
    ctl = make_ctl(ctl_name, e, coeff, params); boiler = Boiler(pmin_kw * 1000.0, mm_honoured)
    steps = int(hours * 3600 / DT); P = 0.0
    out = {"room_err": [], "q": 0.0, "loss": 0.0, "gain": 0.0, "periods": []}
    U0 = Ca * Ta + Cw * Tw + e["C"] * Tr
    per = None; tr = []; last_nst = 0
    for k in range(steps):
        t = k * DT; th = t / 3600.0; tout = f_out(th); g = f_gain(th)
        v = 1.0
        if flow_kind == "variable":
            v = min(1.0, max(0.15, 0.5 + (TARGET + 0.3 - Ta) / 0.6))
        mcp = e["mcp"] * v
        draw = K * v * max(Tr - Ta, 0.0) ** e["exp"]
        flow = Tr + P / (2 * mcp) if P > 0 else Tr
        ret = Tr - P / (2 * mcp) if P > 0 else Tr
        room = round(Ta / 0.1) * 0.1
        if k % 3 == 0:
            ctl.tick(t, room, tout, flow, ret, boiler.on)
        pwm_active = isinstance(ctl, SatCtl) and ctl.pwm
        P = boiler.step(ctl.cs, ctl.ch, flow, draw, pwm_active)
        # PWM period bookkeeping (SAT only): a period runs from one ON-phase start to the next (or to the PWM exit)
        if isinstance(ctl, SatCtl):
            n_st = len(ctl.starts)
            new_on = pwm_active and n_st != last_nst
            if per is not None and not per.get("closed") and (new_on or not pwm_active):
                per["closed"] = True; per["T"] = t - per["t0"]; per["starts"] = boiler.starts - per["starts0"]
                per["end"] = "next_on" if (new_on and pwm_active) else "pwm_exit"
            if new_on:
                per = {"t0": t, "duty": ctl.duty, "duty_settled": None, "q": 0.0, "flame_s": 0.0, "loss": 0.0,
                       "starts0": boiler.starts}
                out["periods"].append(per)
            last_nst = n_st
            if per is not None and not per.get("closed"):
                if per["duty_settled"] is None and t - per["t0"] >= 240.0: per["duty_settled"] = ctl.duty
                per["q"] += P * DT; per["flame_s"] += DT if boiler.on else 0.0
                per["loss"] += (Gao * (Ta - tout) + Gwo * (Tw - tout) - g) * DT
        out["loss"] += (Gao * (Ta - tout) + Gwo * (Tw - tout)) * DT
        dTr = (P - draw) * DT / e["C"]
        dTa = (draw + g - Gao * (Ta - tout) - s3.G_AW * (Ta - Tw)) * DT / Ca
        dTw = (s3.G_AW * (Ta - Tw) - Gwo * (Tw - tout)) * DT / Cw
        Tr += dTr; Ta += dTa; Tw += dTw
        out["q"] += P * DT; out["gain"] += g * DT
        out["room_err"].append(Ta - TARGET)
        if trace and k % 6 == 0:
            tr.append((round(th, 4), round(Ta, 3), round(flow, 2), round(ctl.cs, 2), int(ctl.ch), int(boiler.on),
                       round(P), None if getattr(ctl, "duty", None) is None else round(ctl.duty, 3)))
    re = out["room_err"]; skip = int(min(2.0, hours / 6) * 3600 / DT) if d0 == 0 else 0
    rr = re[skip:]
    res = {"scenario": scn, "ctl": ctl_name, "emitter": emitter_kind, "flow": flow_kind, "era": era, "mass": mass,
           "pmin_kw": pmin_kw, "mm_honoured": mm_honoured, "params": params or {}, "heff_scale": heff_scale, "k_scale": k_scale,
           "rms_err": round(math.sqrt(sum(x * x for x in rr) / len(rr)), 4),
           "share_in_0_3": round(sum(1 for x in rr if abs(x) <= 0.3) / len(rr), 4),
           "max_over": round(max(rr), 3), "max_under": round(min(rr), 3),
           "starts_per_day": round(boiler.starts * 24.0 / hours, 2),
           "heat_kwh": round(out["q"] / 3.6e6, 3), "loss_kwh": round(out["loss"] / 3.6e6, 3),
           "stored_kwh": round((Ca * Ta + Cw * Tw + e["C"] * Tr - U0) / 3.6e6, 3),
           "energy_closure": round(abs(out["q"] + out["gain"] - out["loss"] - (Ca * Ta + Cw * Tw + e["C"] * Tr - U0))
                                   / max(out["loss"], 1.0), 8)}
    if d0 != 0:   # warm-up: time to come within 0.3 and within 0.1
        def first(th_k):
            for i, x in enumerate(re):
                if x >= -th_k: return round(i * DT / 60.0, 1)
            return None
        res["min_to_0_3"] = first(0.3); res["min_to_0_1"] = first(0.1)
    ps = [p for p in out["periods"] if p.get("closed") and p.get("end") == "next_on" and p["duty"] is not None]
    res["pwm_exits"] = sum(1 for p in out["periods"] if p.get("end") == "pwm_exit")
    if ps:
        res["periods"] = [{"duty": round(p["duty"], 3), "T_s": p["T"],
                           "duty_settled": None if p["duty_settled"] is None else round(p["duty_settled"], 3),
                           "on_frac": round(p["flame_s"] / p["T"], 3),
                           "q_kwh": round(p["q"] / 3.6e6, 4), "need_kwh": round(p["loss"] / 3.6e6, 4),
                           "flame_power_kw": round(p["q"] / max(p["flame_s"], 1.0) / 1000.0, 3),
                           "starts": p["starts"]} for p in ps]
    if trace: res["trace"] = tr
    return res


def plants(quick):
    eras = ("3_1983-92",) if quick else ("3_1983-92", "5_2015")
    return [(era, em, fl, pmin) for era in eras for em in ("rad", "ufh") for fl in ("fixed", "variable")
            for pmin in (3.5, 5.5)]


def tune(era, em, fl, pmin):
    """Two picks per baseline on this plant (MM honoured; the baselines never run SAT's PWM), from the published grid
    TUNE_GRID on TUNE_SCENARIOS: 'rms' = lowest summed RMS room error; 'fair' = lowest summed RMS among the params whose
    mean starts per day do not exceed SAT's on the same plant and scenarios (falls back to the fewest starts)."""
    sat = [simulate(scn, "sat", em, fl, era, "heavy", pmin, True) for scn in TUNE_SCENARIOS]
    sat_starts = sum(r["starts_per_day"] for r in sat) / len(sat)
    best = {"sat": {"starts_per_day": round(sat_starts, 2), "rms_sum": round(sum(r["rms_err"] for r in sat), 5)}}
    for ctl, cands in TUNE_GRID.items():
        if ctl == "sat": continue
        scored = []
        for p in cands:
            rs = [simulate(scn, ctl, em, fl, era, "heavy", pmin, True, params=p) for scn in TUNE_SCENARIOS]
            scored.append({"params": p, "rms_sum": round(sum(r["rms_err"] for r in rs), 5),
                           "starts_per_day": round(sum(r["starts_per_day"] for r in rs) / len(rs), 2)})
        by_rms = sorted(scored, key=lambda x: (x["rms_sum"], json.dumps(x["params"], sort_keys=True)))
        ok = [x for x in by_rms if x["starts_per_day"] <= sat_starts]
        fair = ok[0] if ok else sorted(scored, key=lambda x: (x["starts_per_day"], x["rms_sum"]))[0]
        edge = by_rms[0]["params"] in (cands[0], cands[-1]) if len(cands) > 1 else False
        best[ctl] = {"rms": by_rms[0], "fair": fair, "fair_within_sat_starts": bool(ok), "rms_pick_on_grid_edge": edge,
                     "grid": scored}
    return best


def summarize_periods(runs):
    """AC1: across all SAT PWM periods, how the duty estimate relates to delivered heat, need and starts."""
    def pearson(x, y):
        n = len(x)
        if n < 3: return None
        mx, my = sum(x) / n, sum(y) / n
        sx = math.sqrt(sum((a - mx) ** 2 for a in x)); sy = math.sqrt(sum((b - my) ** 2 for b in y))
        return None if sx == 0 or sy == 0 else round(sum((a - mx) * (b - my) for a, b in zip(x, y)) / (sx * sy), 3)
    groups = {}
    for r in runs:
        if r["ctl"] != "sat" or not r.get("periods"): continue
        key = (r["emitter"], r["flow"], r["pmin_kw"], r["mm_honoured"])
        groups.setdefault(key, []).extend(r["periods"])
    out = []
    for key, ps in sorted(groups.items()):
        d = [p["duty"] for p in ps]; ds = [p["duty_settled"] if p["duty_settled"] is not None else p["duty"] for p in ps]
        onf = [p["on_frac"] for p in ps]
        pw = [p["q_kwh"] * 3600.0 / p["T_s"] for p in ps]          # mean delivered power over the period, kW
        need = [p["need_kwh"] * 3600.0 / p["T_s"] for p in ps]      # mean heat the house lost over the period, kW
        fp = [p["flame_power_kw"] for p in ps]
        out.append({"emitter": key[0], "flow": key[1], "pmin_kw": key[2], "mm_honoured": key[3], "n_periods": len(ps),
                    "r_duty_vs_delivered_kw": pearson(d, pw), "r_duty_vs_need_kw": pearson(d, need),
                    "r_duty_vs_on_frac": pearson(d, onf),
                    "mean_abs_duty_minus_on_frac": round(sum(abs(a - b) for a, b in zip(d, onf)) / len(ps), 3),
                    "mean_abs_settled_minus_start_duty": round(sum(abs(a - b) for a, b in zip(ds, d)) / len(ps), 3),
                    "flame_power_kw_mean": round(sum(fp) / len(fp), 2), "flame_power_kw_max": round(max(fp), 2),
                    "delivered_over_need": round(sum(pw) / max(sum(need), 1e-9), 3),
                    "starts_per_period_mean": round(sum(p["starts"] for p in ps) / len(ps), 2)})
    return out


def sensitivity():
    """Parameter sensitivity on one plant (radiators, fixed flow, 1983-92, Pmin 5.5): house loss and emitter size
    +-20 %, light mass, boiler stop threshold 3 K instead of 5 K."""
    out = []
    for hs in (0.8, 1.2):
        for ks in (0.8, 1.2):
            for scn in ("steady_5", "steady_12", "gain_step_8"):
                out.append(dict(scn=scn, emitter_kind="rad", flow_kind="fixed", era="3_1983-92", mass="heavy",
                                pmin_kw=5.5, mm_honoured=True, heff_scale=hs, k_scale=ks))
    for scn in ("steady_5", "steady_12", "gain_step_8"):
        out.append(dict(scn=scn, emitter_kind="rad", flow_kind="fixed", era="3_1983-92", mass="light", pmin_kw=5.5,
                        mm_honoured=True))
        out.append(dict(scn=scn, emitter_kind="rad", flow_kind="fixed", era="3_1983-92", mass="heavy", pmin_kw=5.5,
                        mm_honoured=True, stop_k=3.0))
    return out


def main():
    global BOILER_STOP_K
    ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True); ap.add_argument("--quick", action="store_true")
    a = ap.parse_args(); os.makedirs(a.out, exist_ok=True)
    runs = []; tuned = {}
    for era, em, fl, pmin in plants(a.quick):
        best = tune(era, em, fl, pmin); tuned["|".join((era, em, fl, str(pmin)))] = best
        for scn in SCENARIOS:
            for mm in (True, False):
                runs.append(simulate(scn, "sat", em, fl, era, "heavy", pmin, mm))
            for ctl in ("pi", "tpi", "wc", "wc_room"):
                for pick in ("rms", "fair"):
                    if ctl == "wc" and pick == "fair": continue
                    r = simulate(scn, ctl, em, fl, era, "heavy", pmin, True, params=best[ctl][pick]["params"])
                    r["pick"] = pick; runs.append(r)
    pi_rad = tuned["3_1983-92|rad|fixed|5.5"]
    sens = []
    for kw in sensitivity():
        stop = kw.pop("stop_k", None)
        for ctl in ("sat", "pi", "wc_room"):
            if stop: BOILER_STOP_K = stop
            r = simulate(ctl_name=ctl, params=pi_rad[ctl]["fair"]["params"] if ctl != "sat" else None, **kw)
            r["boiler_stop_k"] = BOILER_STOP_K; sens.append(r)
            BOILER_STOP_K = 5.0
    traces = {f"steady_12_mm{int(mm)}": simulate("steady_12", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, mm,
                                                trace=True)["trace"] for mm in (True, False)}
    traces["warmup_5_sat"] = simulate("warmup_5", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, True, trace=True)["trace"]
    me = os.path.abspath(__file__)
    meta = {"script_sha256": hashlib.sha256(open(me, "rb").read()).hexdigest(),
            "port_sha256": hashlib.sha256(open(os.path.join(os.path.dirname(me), "sat_sim_port.py"), "rb").read()).hexdigest(),
            "deterministic": "no random numbers are used; same inputs give the same output", "dt_s": DT,
            "quick": a.quick}
    res = {"meta": meta, "tuned": tuned, "runs": runs, "sensitivity": sens, "periods_summary": summarize_periods(runs)}
    with open(os.path.join(a.out, "runs.json"), "w", encoding="utf-8") as f:
        json.dump(res, f, indent=0, sort_keys=True)
    with open(os.path.join(a.out, "traces.json"), "w", encoding="utf-8") as f:
        json.dump(traces, f, sort_keys=True)
    print("runs", len(runs), "sensitivity", len(sens))


if __name__ == "__main__":
    main()
