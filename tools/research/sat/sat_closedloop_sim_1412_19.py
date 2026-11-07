"""TASK-1412.19: closed-loop low-load simulation of SAT against documented plant models, with seeded faults, scored
against the thresholds pre-registered in docs/sat/sim-1412-19/thresholds.md (committed b3b096a1a before any run).

SAT is the controller of tools/research/sat/sat_lowload_study_1412_16.py (curve, PID and PWM mapper through the proven port;
cold cutoff, overshoot switch, duty formula, ON-phase setpoints modelled with citations there) plus the fault behaviour
below. Simulation only: no result is physical boiler or compressor proof (AC3).

Added behaviour (sources opened 2026-11-05):
- DHW: during a draw the boiler heats only hot water (no CH heat), the flame is on and the DHW bit set; SAT keeps its
  last command, pauses the PWM ON timer and restarts the start-up sequence after the draw (TASK-1422, entry 95); the
  sustained overshoot switch is not evaluated for 300 s after the draw (TASK-1429; thermo-nova const.py:19); a DHW
  flame never reseeds the effective boiler temperature and is not a PWM start (TASK-1421, entries 89/90).
- Delayed ignition: the ON phase is timed from the flame, SAT waits for it (TASK-1382, entry 80).
- Room reading lost: requested setpoint = the curve, P/I/D reset (entries 38, 46; SATcontrol.ino:2298, :6943).
- Outside reading missing over 1 h: SAT hands back (entries 56, 57; SATcontrol.ino:2407-2420).
- OpenTherm silence over 30 s: hand-back once (CS=0, CH=1, MM=T), then nothing until a boiler reply (entries 39, 47;
  SATcontrol.ino:3825-3859).
Assumptions (A): after a hand-back the room thermostat behind the gateway takes over with time-proportional on/off at a
fixed boiler setpoint (the 1412.16 TPI baseline, band 1 K, 10 min); during an OpenTherm silence the boiler keeps acting
on the last command it received; during a draw the flow sensor reads the hot-water heat exchanger (60 C, decaying with
tau 120 s after the draw).

Run: python tools/research/sat/sat_closedloop_sim_1412_19.py --out docs/sat/sim-1412-19 [--quick]
"""
import argparse, hashlib, json, math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_sim_port as port
import sat_curve_selftune_study3 as s3
import sat_lowload_study_1412_16 as s16

DT = s16.DT
TARGET = s16.TARGET
HOURS = 48
SETTLE_H = 6
CLASSES = ("very_low_load", "thermal_lag", "minimum_power", "trv_pump", "dhw", "ignition", "room_fault",
           "outside_fault", "transport")
OUTDOOR = {"steady_5": lambda t: 5.0, "steady_12": lambda t: 12.0, "steady_14": lambda t: 14.0,
           "diurnal_5": lambda t: 5.0 + 4.0 * math.sin(2 * math.pi * (t - 9.0) / 24.0)}


# Switches for the mutant checks in tests/test_sat_closedloop_sim_1412_19.py (True = the modelled firmware behaviour).
ROOM_LOST_CURVE_ONLY = True      # entries 38/46
SILENT_AFTER_HANDBACK = True     # entries 39/47
S6_DIAG = []                     # diagnostics only: why a short PWM ON phase ended (not part of the scored output)


class _CurveOnly:
    """Stands in for the PID while the room reading is lost: output = the curve (entries 38, 46)."""
    def service(self, *a): pass
    def note_room(self, *a): pass
    def output(self, cv): return cv


class Sat(s16.SatCtl):
    """1412.16 SAT plus DHW, room-loss and hand-back handling (module docstring), and two firmware rules the 1412.16
    model did not have (found by the smoke run, see the report): the hourly PWM start limit also applies to the PWM
    entry and survives a PWM reset (TASK-1401, SATcontrol.ino:1652-1656), and every CS is clamped to the effective
    maximum (SATcontrol.ino:1645-1646); the ON time runs from the first ignition of the phase (TASK-1382)."""

    def __init__(self, e, coeff):
        super().__init__(e, coeff)
        self.dhw_guard_until = -1.0; self.was_dhw = False; self.last_room = TARGET; self.real_pid = self.pid

    def tick2(self, now_s, room, tout, flow, ret, flame, dhw):
        e = self.e
        if room is None and not ROOM_LOST_CURVE_ONLY:
            room = self.last_room                     # mutant: regulate on the stale reading
        if room is not None: self.last_room = room
        if dhw:            # TASK-1422: keep the last command, pause PWM, restart the start-up sequence after the draw
            self.was_dhw = True; return None
        if self.was_dhw:
            self.was_dhw = False; self.dhw_guard_until = now_s + 300.0
            self.flame_at = None; self.teff = None; self.ph_el = 0.0
        if now_s < self.dhw_guard_until: self.ov_s = 0.0   # TASK-1429: no sustained switch for 300 s
        if room is None:   # entries 38/46: curve only, P/I/D reset
            self.real_pid.reset(int(1000000 + now_s * 1000)); self.pid = _CurveOnly(); room_in = TARGET
        else:
            self.pid = self.real_pid; room_in = room
        n0 = len(self.starts); was_pwm = self.pwm; fa0 = self.flame_at; on0 = self.ph_on
        sp = self.tick(now_s, room_in, tout, flow, ret, flame)
        if on0 and self.ph_on and fa0 is None and self.flame_at is not None:
            self.ph_el = 0.0       # first ignition of this ON phase: the ON time starts now (TASK-1382, pwm.py:186-193)
        if self.pwm and not was_pwm and len(self.starts) > n0:     # PWM entry: subject to the hourly limit too
            prev = [t for t in self.starts[:-1] if now_s - t < 3600.0]
            if len(prev) >= s16.GAS_MAX_STARTS_H:
                self.starts.pop(); self.ph_on = False; self.ph_el = 0.0; self.cs, self.ch = 10.0, False
        self.cs = min(self.cs, e["spmax"])                          # SATcontrol.ino:1645-1646
        return sp


class Boiler2(s16.Boiler):
    """1412.16 boiler with seeded ignition delay / failure and DHW priority."""

    def __init__(self, pmin, mm, rnd, ign):
        super().__init__(pmin, mm); self.rnd = rnd; self.ign = ign; self.lit_at = None; self.flame = False

    def step2(self, cs, ch, flow, draw, pwm_active, dhw):
        if dhw:
            self.flame = True; self.on = True; self.off_s = 0.0; return 0.0
        was_on = self.on
        p = self.step(cs, ch, flow, draw, pwm_active)
        if self.on and not was_on:
            if self.ign:
                d = self.rnd.uniform(5.0, 120.0) + (60.0 if self.rnd.random() < 0.1 else 0.0)
            else:
                d = 0.0
            self.lit_at = self.on_s + d
        if not self.on: self.flame = False; self.lit_at = None; return 0.0
        self.flame = self.lit_at is None or self.on_s >= self.lit_at
        return p if self.flame else 0.0


def run(cls, era, em, fl, pmin, mm, seed, outdoor="steady_5", mass="heavy", coeff=1.6, trace=False):
    rnd = random.Random(seed * 1000 + CLASSES.index(cls))
    if cls == "very_low_load" and outdoor == "steady_5": outdoor = "steady_14" if seed % 2 else "steady_12"
    if cls == "thermal_lag": mass = "light" if seed % 2 else "heavy"
    if cls == "minimum_power": pmin = 7.0
    f_out = OUTDOOR[outdoor]
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, mass, 1.0)
    e = s16.emitter(em); K = s16.calibrate_K(e, Heff)
    to0 = f_out(0.0); Ta = TARGET; Tw = (s3.G_AW * Ta + Gwo * to0) / (s3.G_AW + Gwo)
    Tr = max(Ta, float(port.curve(coeff, TARGET, to0, e["base"])) - 2.0)
    ctl = Sat(e, coeff); fallback = s16.TpiCtl(e, coeff, cycle=600.0, band=1.0)
    boiler = Boiler2(pmin * 1000.0, mm, rnd, cls == "ignition")
    steps = int(HOURS * 3600 / DT)
    # seeded events
    ev = {}
    if cls == "trv_pump": t0 = rnd.uniform(8, 30); ev["trv"] = (t0, t0 + 2.0); ev["pump_half_from"] = rnd.uniform(8, 40)
    if cls == "room_fault": t0 = rnd.uniform(8, 30); ev["room_lost"] = (t0, t0 + rnd.uniform(1.0, 6.0))
    if cls == "outside_fault": t0 = rnd.uniform(8, 30); ev["out_lost"] = (t0, t0 + rnd.uniform(1.5, 6.0))
    if cls == "transport":
        ev["delay_s"] = rnd.uniform(5.0, 30.0); t0 = rnd.uniform(8, 30); ev["ot_silence"] = (t0, t0 + 2.0 / 60.0)
    draws = []
    if cls == "dhw":
        for d in range(HOURS // 24):
            for _ in range(rnd.randint(0, 6)):
                st = d * 24 + rnd.uniform(6.0, 23.0); draws.append((st, st + rnd.uniform(2.0, 15.0) / 60.0))
    room_f = Ta; hx = None
    fault_windows = [v for k, v in ev.items() if isinstance(v, tuple)]
    P = 0.0; cmd_q = []; delivered = (10.0, False); handed = False; handback_s = None; cmds_after_handback = 0
    out_lost_since = None; viol = {"H1": 0, "H2": 0, "H3": 0, "H4": 0}; max_cs = 0.0
    room_err = []; scored = []; tr = []; pwm_phase = None; short_on = 0; dhw_hit = False; short_cause = {}
    last_out_ok = 0.0
    for k in range(steps):
        t = k * DT; th = t / 3600.0; tout = f_out(th)
        dhw = any(a <= th < b for a, b in draws)
        v = 1.0
        if fl == "variable": v = min(1.0, max(0.15, 0.5 + (TARGET + 0.3 - Ta) / 0.6))
        if "trv" in ev and ev["trv"][0] <= th < ev["trv"][1]: v = 0.15
        mcp = e["mcp"] * v * (0.5 if th >= ev.get("pump_half_from", 1e9) else 1.0)
        draw = K * v * max(Tr - Ta, 0.0) ** e["exp"]
        flow_w = Tr + P / (2 * mcp) if P > 0 else Tr; ret = Tr - P / (2 * mcp) if P > 0 else Tr
        if dhw: hx = 60.0
        elif hx is not None:
            hx = Tr + (hx - Tr) * math.exp(-DT / 120.0)
            if hx - Tr < 0.2: hx = None
        flow = max(flow_w, hx) if hx is not None else flow_w
        # room sensor (lag in the thermal_lag class)
        tau = 900.0 if cls == "thermal_lag" else 0.0
        room_f = room_f + (Ta - room_f) * (DT / (tau + DT)) if tau else Ta
        room_lost = "room_lost" in ev and ev["room_lost"][0] <= th < ev["room_lost"][1]
        room = None if room_lost else round(room_f / 0.1) * 0.1
        out_ok = not ("out_lost" in ev and ev["out_lost"][0] <= th < ev["out_lost"][1])
        if out_ok: last_out_ok = t
        silent = "ot_silence" in ev and ev["ot_silence"][0] <= th < ev["ot_silence"][1]
        if not silent: last_reply = t
        if k % 3 == 0:
            # hand-back triggers (entries 39/47 and 56/57)
            lost_ot = (t - last_reply) >= 30.0
            lost_out = (t - last_out_ok) >= 3600.0
            if (lost_ot or lost_out) and not handed:
                handed = True; handback_s = t; cmd_q.append((t, (0.0, True), "handback"))
            elif handed and not (lost_ot or lost_out):
                handed = False; ctl.pwm = False; ctl.pid.reset(int(1000000 + t * 1000))
            if not handed:
                # outside within its valid hour: SAT keeps using the last value it had (entry 57)
                sp = ctl.tick2(t, room, tout, flow, ret, boiler.flame, dhw)
                if room_lost and sp is not None:
                    cv = min(e["spmax"], max(10.0, float(port.curve(coeff, TARGET, tout, e["base"]))))
                    if abs(sp - cv) > 1e-6: viol["H4"] += 1
                if not silent: cmd_q.append((t, (ctl.cs, ctl.ch), "sat"))
                max_cs = max(max_cs, ctl.cs)
                if ctl.cs > e["spmax"] + 1e-6: viol["H2"] += 1
            else:
                # the room thermostat behind the gateway runs the boiler (A); SAT sends nothing (H3)
                fallback.tick(t, round(Ta / 0.1) * 0.1, tout, flow, ret, boiler.flame)
                if not SILENT_AFTER_HANDBACK:
                    cmd_q.append((t, (ctl.cs, ctl.ch), "sat")); cmds_after_handback += 1
        # deliver commands after the transport delay
        delay = ev.get("delay_s", 0.0)
        while cmd_q and t - cmd_q[0][0] >= delay:
            delivered = cmd_q.pop(0)[1]
        bcs, bch = delivered
        if handed and SILENT_AFTER_HANDBACK and handback_s is not None and t - handback_s >= delay:
            bcs, bch = fallback.cs, fallback.ch
        pwm_active = (not handed) and ctl.pwm
        P = boiler.step2(bcs, bch, flow, draw, pwm_active, dhw)
        # PWM ON phases: flame time per phase (S6)
        if pwm_active and ctl.ph_on:
            if pwm_phase is None: pwm_phase = {"flame": 0.0, "dhw": False}
            pwm_phase["flame"] += DT if (boiler.flame and not dhw) else 0.0; pwm_phase["dhw"] |= dhw
            pwm_phase["lit"] = boiler.flame
        elif pwm_phase is not None:
            if pwm_phase["flame"] < 180.0 and not pwm_phase["dhw"] and pwm_phase["flame"] > 0:
                short_on += 1
                cause = "cold_cutoff" if ctl.cut else "pwm_exit" if not ctl.pwm else "on_timer"
                if cause == "cold_cutoff" and not pwm_phase["lit"]: cause = "cold_cutoff_flame_out"   # TASK-1652 diagnostics
                S6_DIAG.append((cause, pwm_phase["flame"])); short_cause[cause] = short_cause.get(cause, 0) + 1
            pwm_phase = None
        Tr += (P - draw) * DT / e["C"]
        Ta += (draw - Gao * (Ta - tout) - s3.G_AW * (Ta - Tw)) * DT / Ca
        Tw += (s3.G_AW * (Ta - Tw) - Gwo * (Tw - tout)) * DT / Cw
        in_fault = any(a <= th < b for a, b in fault_windows)
        if th >= SETTLE_H and not in_fault: scored.append(Ta - TARGET)
        room_err.append((th, Ta - TARGET))
        if trace and k % 6 == 0:
            tr.append((round(th, 4), round(Ta, 3), round(flow, 2), round(bcs, 2), int(bch), int(boiler.flame), round(P),
                       int(dhw), int(handed)))
    # H1: PWM starts in any rolling hour
    st = ctl.starts; h1 = max([sum(1 for b in st if a <= b < a + 3600.0) for a in st] or [0])
    if h1 > 3: viol["H1"] = h1
    viol["H3"] = cmds_after_handback
    # S5 recovery
    rec = None
    ends = [b for k2, (a, b) in ((k2, v) for k2, v in ev.items() if isinstance(v, tuple))]
    if ends:
        end = max(ends); rec = None
        for th, err in room_err:
            if th >= end and abs(err) <= 0.3: rec = round((th - end) * 60.0, 1); break
    res = {"class": cls, "era": era, "emitter": em, "flow": fl, "pmin_kw": pmin, "mm_honoured": mm, "seed": seed,
           "outdoor": outdoor, "mass": mass, "events": {k2: v for k2, v in ev.items()}, "draws": len(draws),
           "hard": viol, "max_cs": round(max_cs, 2),
           "S1_share_in_0_3": round(sum(1 for x in scored if abs(x) <= 0.3) / max(len(scored), 1), 4),
           "S2_max_over": round(max(scored), 3), "S3_max_under": round(min(scored), 3),
           "S4_starts_per_day": round(boiler.starts * 24.0 / HOURS, 2), "S5_recovery_min": rec, "S6_short_on": short_on,
           "pwm_starts_max_per_hour": h1, "S6_short_cause": short_cause}
    res["pass"] = {"H1": viol["H1"] == 0, "H2": viol["H2"] == 0, "H3": viol["H3"] == 0, "H4": viol["H4"] == 0,
                   "S1": res["S1_share_in_0_3"] >= 0.80, "S2": res["S2_max_over"] <= 0.6, "S3": res["S3_max_under"] >= -0.6,
                   "S4": res["S4_starts_per_day"] <= 72, "S5": None if not ends else (rec is not None and rec <= 240),
                   "S6": short_on == 0}
    if trace: res["trace"] = tr
    return res


def grid(quick, classes=CLASSES):
    eras = ("3_1983-92",) if quick else ("3_1983-92", "5_2015")
    for cls in classes:
        for era in eras:
            for em in ("rad", "ufh"):
                for fl in ("fixed", "variable"):
                    for pmin in ((5.5,) if quick else (3.5, 5.5)):
                        for mm in (True, False):
                            for seed in ((1, 2) if quick else (1, 2, 3, 4, 5)):
                                yield (cls, era, em, fl, pmin, mm, seed)


def summarize(runs):
    out = {}
    for cls in CLASSES:
        rs = [r for r in runs if r["class"] == cls]
        if not rs: continue
        s = {"runs": len(rs)}
        for h in ("H1", "H2", "H3", "H4"):
            s[h + "_fail_runs"] = sum(1 for r in rs if not r["pass"][h])
        for sk in ("S1", "S2", "S3", "S4", "S5", "S6"):
            app = [r for r in rs if r["pass"][sk] is not None]
            s[sk + "_pass_share"] = None if not app else round(sum(1 for r in app if r["pass"][sk]) / len(app), 3)
        s["class_pass"] = all(s[h + "_fail_runs"] == 0 for h in ("H1", "H2", "H3", "H4")) and all(
            s[sk + "_pass_share"] is None or s[sk + "_pass_share"] >= 0.9 for sk in ("S1", "S2", "S3", "S4", "S5", "S6"))
        out[cls] = s
    return out


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True); ap.add_argument("--quick", action="store_true")
    ap.add_argument("--classes", default=",".join(CLASSES), help="comma list; runs only these classes")
    ap.add_argument("--merge", action="store_true", help="merge the per-class part files into runs.json")
    ap.add_argument("--cold-cut-min-burn", action="store_true",
                    help="TASK-1652 rule: the cold cutoff waits until a lit PWM ON phase has burnt 180 s (default: today)")
    a = ap.parse_args(); os.makedirs(a.out, exist_ok=True)
    s16.COLD_CUT_MIN_BURN = a.cold_cut_min_burn
    parts = os.path.join(a.out, "parts"); os.makedirs(parts, exist_ok=True)
    if not a.merge:
        for cls in a.classes.split(","):
            rs = [run(*g) for g in grid(a.quick, (cls,))]
            with open(os.path.join(parts, cls + ".json"), "w", encoding="utf-8") as f: json.dump(rs, f, sort_keys=True)
            print(cls, summarize(rs)[cls], flush=True)
        return
    runs = []
    for cls in CLASSES:
        with open(os.path.join(parts, cls + ".json"), encoding="utf-8") as f: runs.extend(json.load(f))
    summ = summarize(runs)
    me = os.path.abspath(__file__)
    sha = lambda f: hashlib.sha256(open(f, "rb").read()).hexdigest()
    meta = {"script_sha256": sha(me), "study16_sha256": sha(os.path.join(os.path.dirname(me), "sat_lowload_study_1412_16.py")),
            "port_sha256": sha(os.path.join(os.path.dirname(me), "sat_sim_port.py")), "thresholds_commit": "b3b096a1a",
            "hours": HOURS, "settle_h": SETTLE_H, "dt_s": DT, "quick": a.quick,
            "cold_cut_min_burn": a.cold_cut_min_burn}
    with open(os.path.join(a.out, "runs.json"), "w", encoding="utf-8") as f:
        json.dump({"meta": meta, "summary": summ, "runs": runs}, f, indent=0, sort_keys=True)
    for cls, s in summ.items(): print(cls, s)


if __name__ == "__main__":
    main()
