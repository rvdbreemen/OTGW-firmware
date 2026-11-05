"""TASK-1655: does a lower KD shorten SAT's warm-up without hurting regulation? KD x1.0 (today) vs x0.9 vs x0.8,
scored against the thresholds and the decision rule pre-registered in docs/sat/sim-1655/thresholds.md.

Question (Sergeant D, docs/sat/sergeant-d-corrections.md entries 277/278): the slow warm-up comes from the derivative
term, not from the integral; the integral stays exactly as thermo-nova (entry 277). thermo-nova at 7beaaf5:
kd = round(0.07 * 8400 * kp, 6) (custom_components/sat/pid.py:84-93), D = round(kd * raw_derivative, 3)
(pid.py:109-114), raw derivative frozen inside the 0.1 C deadband (pid.py:205-207). Firmware: SATpid.ino:219
kd = satPidRound3(0.07f * SAT_PID_AGGRESSION_V3 * kp) (entry 95: 3-decimal rounding).

The variant multiplies inside the float32 product before the 3-decimal rounding:
    kd = round3(f32(KD_FACTOR) * f32(0.07) * AGGRESSION * kp)
which is how a firmware change would read: satPidRound3(KD_FACTOR * 0.07f * SAT_PID_AGGRESSION_V3 * kp).
It is injected by replacing sat_sim_port.Pid._gains for the duration of a run; sat_sim_port.py itself is not edited
(its sha256 is pinned in earlier runs.json files and by tests/test_sat_sim_port_equivalence.py).

Controller: the TASK-1412.19 SAT (tools/research/sat/sat_closedloop_sim_1412_19.py class Sat: the 1412.16 SAT plus the hourly start
limit on the PWM entry and the CS clamp), driven by a fault-free copy of sat_closedloop_sim_1412_19.run with a room start
offset d0 (the 1412.19 runs all start at the target). tests/test_sat_kd_warmup_sim_1655.py proves that the copy with
KD x1.0 and d0 = 0 gives the same scored numbers as sat_closedloop_sim_1412_19.run.

Simulation only: no result is physical boiler proof, and a pass says nothing about the firmware outside the modelled logic.
Run: python tools/research/sat/sat_kd_warmup_sim_1655.py --out docs/sat/sim-1655 --classes <list> ; then --merge
"""
import argparse, hashlib, json, math, os, statistics, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_sim_port as port
import sat_curve_selftune_study3 as s3
import sat_lowload_study_1412_16 as s16
import sat_closedloop_sim_1412_19 as c19

f32 = port.f32
DT = s16.DT
TARGET = s16.TARGET
HOURS = 24
FACTORS = (1.0, 0.9, 0.8)
ERAS = ("3_1983-92", "5_2015")
MASSES = ("light", "heavy")
EMITTERS = ("rad", "ufh")
PLANT_CLASSES = tuple(f"{era}/{mass}/{em}" for era in ERAS for mass in MASSES for em in EMITTERS)
WARMUP_OUTDOOR = (-5.0, 5.0, 12.0)
WARMUP_D0 = (-2.5, -4.0)
GUARD_SCENARIOS = {   # the non-warm-up 1412.16 scenarios (sat_lowload_study_1412_16.py SCENARIOS), d0 = 0
    "steady_5": (lambda t: 5.0, lambda t: 0.0),
    "gain_step_8": (lambda t: 8.0, lambda t: 1500.0 if 6 <= t < 10 else 0.0),
    "falling_demand": (lambda t: 2.0 + min(max(t - 2, 0) / 6.0, 1.0) * 10.0, lambda t: 0.0)}
GUARD_SKIP_H = 2.0    # as sat_lowload_study_1412_16.simulate for d0 == 0 runs of 24 h

_ORIG_GAINS = port.Pid._gains


def _gains_with(factor):
    fa = f32(factor)

    def _gains(self, cv):
        self.kp = port.round3(self.coeff * f32(cv) / self.divisor)
        self.ki = port.round3(self.kp / port.AGGRESSION)
        self.kd = port.round3(fa * f32(0.07) * port.AGGRESSION * self.kp)
    return _gains


class kd_factor:
    """Context manager: run the port PID with kd = round3(factor * 0.07 * 8400 * kp); restores the original after."""
    def __init__(self, factor): self.factor = factor

    def __enter__(self):
        port.Pid._gains = _ORIG_GAINS if self.factor == 1.0 else _gains_with(self.factor)
        return self

    def __exit__(self, *a):
        port.Pid._gains = _ORIG_GAINS


def run(era, mass, em, fl, pmin, mm, outdoor, d0, factor=1.0, gain=None, hours=HOURS, coeff=1.6, trace=False,
        skip_h=GUARD_SKIP_H):
    """One fault-free closed-loop run. outdoor: a float (steady) or a callable t_h -> C. gain: callable t_h -> W or None.
    Plant and controller code as sat_closedloop_sim_1412_19.run without the fault, DHW and ignition branches."""
    with kd_factor(factor):
        r = _run(era, mass, em, fl, pmin, mm, outdoor, d0, gain, hours, coeff, trace, skip_h)
    r["kd_factor"] = factor
    return r


def _run(era, mass, em, fl, pmin, mm, outdoor, d0, gain, hours, coeff, trace, skip_h):
    f_out = outdoor if callable(outdoor) else (lambda t, v=float(outdoor): v)
    f_gain = gain or (lambda t: 0.0)
    Ca, Cw, Gao, Gwo, Heff, _ = s3.house(era, mass, 1.0)
    e = s16.emitter(em); K = s16.calibrate_K(e, Heff)
    to0 = f_out(0.0)
    # start: room at target + d0, wall at its equilibrium for that room, water at the curve - 2 (as 1412.16 / 1412.19)
    Ta = TARGET + d0; Tw = (s3.G_AW * Ta + Gwo * to0) / (s3.G_AW + Gwo)
    Tr = max(Ta, float(port.curve(coeff, TARGET, to0, e["base"])) - 2.0)
    ctl = c19.Sat(e, coeff)
    boiler = c19.Boiler2(pmin * 1000.0, mm, None, False)
    steps = int(hours * 3600 / DT)
    P = 0.0; max_cs = 0.0; h2 = 0
    err = []; dlog = []; tr = []
    for k in range(steps):
        t = k * DT; th = t / 3600.0; tout = f_out(th); g = f_gain(th)
        v = 1.0
        if fl == "variable": v = min(1.0, max(0.15, 0.5 + (TARGET + 0.3 - Ta) / 0.6))
        mcp = e["mcp"] * v
        draw = K * v * max(Tr - Ta, 0.0) ** e["exp"]
        flow = Tr + P / (2 * mcp) if P > 0 else Tr; ret = Tr - P / (2 * mcp) if P > 0 else Tr
        room = round(Ta / 0.1) * 0.1
        if k % 3 == 0:
            ctl.tick2(t, room, tout, flow, ret, boiler.flame, False)
            max_cs = max(max_cs, ctl.cs)
            if ctl.cs > e["spmax"] + 1e-6: h2 += 1
            pid = ctl.pid
            if hasattr(pid, "kd") and not pid.first_output_pending:
                dlog.append((th, float(TARGET - room), float(port.round3(pid.kd * pid.raw_d))))
        pwm_active = ctl.pwm
        P = boiler.step2(ctl.cs, ctl.ch, flow, draw, pwm_active, False)
        Tr += (P - draw) * DT / e["C"]
        Ta += (draw + g - Gao * (Ta - tout) - s3.G_AW * (Ta - Tw)) * DT / Ca
        Tw += (s3.G_AW * (Ta - Tw) - Gwo * (Tw - tout)) * DT / Cw
        err.append(Ta - TARGET)
        if trace and k % 6 == 0:
            tr.append((round(th, 4), round(Ta, 3), round(flow, 2), round(ctl.cs, 2), int(ctl.ch), int(boiler.flame), round(P)))
    st = ctl.starts; h1 = max([sum(1 for b in st if a <= b < a + 3600.0) for a in st] or [0])

    def first(level):
        for i, x in enumerate(err):
            if x >= level: return round(i * DT / 60.0, 1)
        return None

    t03, t01, t00 = first(-0.3), first(-0.1), first(0.0)
    full = hours * 60.0
    after = err[int(t03 * 60 / DT):] if t03 is not None else []
    rms = lambda xs: round(math.sqrt(sum(x * x for x in xs) / len(xs)), 4) if xs else None
    # D during the approach: error outside the deadband, before the room first reaches -0.1
    appr = [d for (th, er, d) in dlog if er > 0.1 and (t01 is None or th * 60.0 < t01)]
    skip = int(skip_h * 3600 / DT)
    sc = err[skip:]
    res = {"era": era, "mass": mass, "emitter": em, "flow": fl, "pmin_kw": pmin, "mm_honoured": mm,
           "outdoor": outdoor if not callable(outdoor) else None, "d0": d0, "hours": hours,
           "min_to_0_3": t03, "min_to_0_1": t01, "min_to_0_0": t00,
           "min_to_0_3_scored": t03 if t03 is not None else full,
           "min_to_0_1_scored": t01 if t01 is not None else full,
           "max_over": round(max(err), 3), "max_under": round(min(err), 3),
           "rms_all": rms(err), "rms_after_0_3": rms(after),
           "starts_per_day": round(boiler.starts * 24.0 / hours, 2), "pwm_starts_max_per_hour": h1,
           "hard": {"H1": h1 <= 3, "H2": h2 == 0}, "max_cs": round(max_cs, 2),
           "D_approach_mean": round(statistics.fmean(appr), 3) if appr else None,
           "D_approach_min": round(min(appr), 3) if appr else None,
           "share_in_0_3_scored": round(sum(1 for x in sc if abs(x) <= 0.3) / len(sc), 4),
           "max_over_scored": round(max(sc), 3)}
    if trace: res["trace"] = tr
    return res


def warmup_grid(pc):
    era, mass, em = pc.split("/")
    for fl in ("fixed", "variable"):
        for pmin in (3.5, 5.5):
            for mm in (True, False):
                for out in WARMUP_OUTDOOR:
                    for d0 in WARMUP_D0:
                        yield (era, mass, em, fl, pmin, mm, out, d0)


def guard_grid(pc):
    era, mass, em = pc.split("/")
    for scn in GUARD_SCENARIOS:
        for fl in ("fixed", "variable"):
            for pmin in (3.5, 5.5):
                for mm in (True, False):
                    yield scn, (era, mass, em, fl, pmin, mm)


def run_class(pc):
    rows = []
    for f in FACTORS:
        for a in warmup_grid(pc):
            r = run(*a, factor=f); r["kind"] = "warmup"; r["plant_class"] = pc; rows.append(r)
        for scn, a in guard_grid(pc):
            fo, fg = GUARD_SCENARIOS[scn]
            r = run(*a, fo, 0.0, factor=f, gain=fg); r["kind"] = "guard"; r["scenario"] = scn; r["plant_class"] = pc
            rows.append(r)
    return rows


def _key(r):
    return (r["kind"], r.get("scenario"), r["flow"], r["pmin_kw"], r["mm_honoured"], r["outdoor"], r["d0"])


def judge(rows):
    """Apply the pre-registered decision rule (docs/sat/sim-1655/thresholds.md) to one plant class."""
    base = {_key(r): r for r in rows if r["kd_factor"] == 1.0}
    out = {}
    med = lambda xs: statistics.median(xs) if xs else None
    for f in FACTORS[1:]:
        var = [r for r in rows if r["kd_factor"] == f]
        wu = [(base[_key(r)], r) for r in var if r["kind"] == "warmup"]
        gd = [(base[_key(r)], r) for r in var if r["kind"] == "guard"]
        rel = [(b["min_to_0_3_scored"] - v["min_to_0_3_scored"]) / b["min_to_0_3_scored"]
               for b, v in wu if b["min_to_0_3_scored"] > 0]
        d_t01 = [v["min_to_0_1_scored"] - b["min_to_0_1_scored"] for b, v in wu]
        d_over = [v["max_over"] - b["max_over"] for b, v in wu]
        d_starts = [(v["starts_per_day"] - b["starts_per_day"]) / max(b["starts_per_day"], 1.0) for b, v in wu]
        d_rms = [v["rms_after_0_3"] - b["rms_after_0_3"] for b, v in wu
                 if v["rms_after_0_3"] is not None and b["rms_after_0_3"] is not None]
        lost = sum(1 for b, v in wu if b["min_to_0_3"] is not None and v["min_to_0_3"] is None)
        d_s1 = [v["share_in_0_3_scored"] - b["share_in_0_3_scored"] for b, v in gd]
        s = {
            "B_median_t03_reduction": round(med(rel), 4) if rel else None,
            "B_median_t03_minutes_saved": round(med([b["min_to_0_3_scored"] - v["min_to_0_3_scored"] for b, v in wu]), 1),
            "G1_max_over_all_runs": max(v["max_over"] for b, v in wu),
            "G1_median_over_increase": round(med(d_over), 3),
            "G2_max_starts_per_day": max(v["starts_per_day"] for b, v in wu + gd),
            "G2_median_starts_increase": round(med(d_starts), 4),
            "G3_median_rms_after_increase": round(med(d_rms), 4) if d_rms else None,
            "G4_median_t01_delta_min": round(med(d_t01), 1),
            "G4_runs_reaching_0_3_lost": lost,
            "G5_hard_fail_runs": sum(1 for b, v in wu + gd if not all(v["hard"].values())),
            "G6_median_s1_decrease": round(-med(d_s1), 4),
            "G6_max_over_guard": max(v["max_over_scored"] for b, v in gd)}
        s["B"] = s["B_median_t03_reduction"] is not None and s["B_median_t03_reduction"] >= 0.10
        s["G"] = {"G1": s["G1_max_over_all_runs"] <= 0.6 and s["G1_median_over_increase"] <= 0.1,
                  "G2": s["G2_max_starts_per_day"] <= 72 and s["G2_median_starts_increase"] <= 0.10,
                  "G3": s["G3_median_rms_after_increase"] is None or s["G3_median_rms_after_increase"] <= 0.02,
                  "G4": s["G4_median_t01_delta_min"] <= 0 and lost == 0,
                  "G5": s["G5_hard_fail_runs"] == 0,
                  "G6": s["G6_median_s1_decrease"] <= 0.02 and s["G6_max_over_guard"] <= 0.6}
        s["class_pass"] = s["B"] and all(s["G"].values())
        out[str(f)] = s
    bw = [r for r in rows if r["kd_factor"] == 1.0 and r["kind"] == "warmup" and r["D_approach_mean"] is not None]
    out["baseline_D_approach_median"] = round(med([r["D_approach_mean"] for r in bw]), 3) if bw else None
    out["baseline_median_t03_min"] = med([r["min_to_0_3_scored"] for r in rows if r["kd_factor"] == 1.0 and r["kind"] == "warmup"])
    return out


def recommend(per_class):
    """Pre-registered rule: x0.8 if it passes >= 6 of 8 plant classes and no guard (G1-G6) fails in any class;
    else x0.9 on the same rule; else keep KD (no B anywhere: 'KD is not the lever in this model')."""
    for f in ("0.8", "0.9"):
        passes = sum(1 for pc in per_class.values() if pc[f]["class_pass"])
        guards_ok = all(all(pc[f]["G"].values()) for pc in per_class.values())
        if passes >= 6 and guards_ok: return {"choice": "x" + f, "passes": passes}
    anyB = any(pc[f]["B"] for pc in per_class.values() for f in ("0.8", "0.9"))
    return {"choice": "keep", "reason": "trade-off: a guard fails" if anyB else "no 10 % warm-up gain in any class"}


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--out", required=True)
    ap.add_argument("--classes", default=",".join(PLANT_CLASSES))
    ap.add_argument("--merge", action="store_true")
    ap.add_argument("--thresholds-commit", default="")
    a = ap.parse_args()
    parts = os.path.join(a.out, "parts"); os.makedirs(parts, exist_ok=True)
    if not a.merge:
        for pc in a.classes.split(","):
            rows = run_class(pc)
            with open(os.path.join(parts, pc.replace("/", "_") + ".json"), "w", encoding="utf-8") as f:
                json.dump(rows, f, sort_keys=True)
            print(pc, json.dumps(judge(rows), sort_keys=True), flush=True)
        return
    per_class = {}; allrows = []
    for pc in PLANT_CLASSES:
        with open(os.path.join(parts, pc.replace("/", "_") + ".json"), encoding="utf-8") as f: rows = json.load(f)
        allrows.extend(rows); per_class[pc] = judge(rows)
    here = os.path.dirname(os.path.abspath(__file__))
    sha = lambda n: hashlib.sha256(open(os.path.join(here, n), "rb").read()).hexdigest()
    meta = {"script_sha256": sha("sat_kd_warmup_sim_1655.py"), "c19_sha256": sha("sat_closedloop_sim_1412_19.py"),
            "study16_sha256": sha("sat_lowload_study_1412_16.py"), "port_sha256": sha("sat_sim_port.py"),
            "thresholds_commit": a.thresholds_commit, "hours": HOURS, "dt_s": DT, "factors": FACTORS}
    with open(os.path.join(a.out, "runs.json"), "w", encoding="utf-8") as f:
        json.dump({"meta": meta, "per_class": per_class, "recommendation": recommend(per_class), "runs": allrows},
                  f, indent=0, sort_keys=True)
    print(json.dumps({"per_class": per_class, "recommendation": recommend(per_class)}, indent=1, sort_keys=True))


if __name__ == "__main__":
    main()
