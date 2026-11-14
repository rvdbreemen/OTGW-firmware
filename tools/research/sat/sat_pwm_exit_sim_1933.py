"""TASK-1933: PWM-to-continuous exit, today's underheat exit against Sergeant D's entry-342 rule (simulation).

Pre-registered thresholds: docs/sat/sim-1933/thresholds.md (committed before the first grid run). Simulation only: no
result is physical boiler proof; every number this script prints is a MODEL RESULT.

Plant, boiler, faults and the SAT controller are those of tools/research/sat/sat_closedloop_sim_1412_19.py (itself the
1412.16 controller through the proven port); that module is not edited. This script swaps in a subclass of its Sat
with the mode rules under study (sources opened 2026-11-14):
- Entry continuous -> PWM (both variants): flame on and flow - requested setpoint >= 1.5 C for 30 s (Sergeant D entry
  327, which supersedes the 2.0 C / 60 s rule of entry 22). The firmware failsafe 3.5 C / 30 s (SATcycles.ino:35-38,
  entries 288/289) is covered by it and not modelled separately.
- Exit PWM -> continuous, baseline ("today"): thermo-nova _maybe_disable_pwm_runtime / _on_sustained_underheat
  (heating_control.py:308-356): only while PWM is enabled and its status is ON (:321-323), not during hot water or 300 s
  after it (:313-315, const.py:19), not with the request at or below the cold setpoint (:317-319); flow <= request - 3.0
  (cycles/const.py:11 margin, UNDERSHOOT_MARGIN -3.0) for 180 s (const.py:22). Firmware SATcycles.ino:39-40.
- Exit PWM -> continuous, variant ("e342"): Sergeant D entries 339/342: flame on (not a hot-water flame) and the request
  above the flow (boiler) temperature for 60 s, with the same gates as the baseline (PWM status ON, no hot water and its
  300 s guard, request above the cold setpoint). Open point 2 of entry 342 (PWM ON only) is modelled as the thermo-nova
  gate; with the flame-on condition the rule can only hold during the ON phase anyway.
- Both variants keep the saturation exit (300 s of continuous duty, heating_control.py:358-370; the 1412.16 model).

Measures per run (MODEL RESULT): PWM entries per day, PWM exits per day by cause, toggles (a PWM re-entry within 10 min
of an underheat-type exit) per day, burner starts per day (S4 of 1412.19), comfort S1-S3 of 1412.19.

Run: python tools/research/sat/sat_pwm_exit_sim_1933.py --out docs/sat/sim-1933 [--quick] [--jobs 4] [--interval 30]
"""
import argparse
import hashlib
import json
import multiprocessing as mp
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_closedloop_sim_1412_19 as cl  # noqa: E402
import sat_lowload_study_1412_16 as s16  # noqa: E402

ENTRY_MARGIN = 1.5      # entry 327
ENTRY_SUSTAIN = 30.0    # entry 327
UNDER_MARGIN = 3.0      # thermo-nova UNDERSHOOT_MARGIN_CELSIUS (-3.0), SATcycles.ino:39
UNDER_SUSTAIN = 180.0   # thermo-nova const.py:22, SATcycles.ino:40
E342_SUSTAIN = 60.0     # entry 339/342
TOGGLE_WINDOW = 600.0   # a PWM re-entry within 10 min of an underheat-type exit counts as a toggle
CLASSES = ("very_low_load", "thermal_lag", "minimum_power", "dhw", "ignition")
VARIANTS = ("today", "e342")
GRUIJTER_LIKE = ("3_1983-92", "rad", "fixed", 5.5, False)   # radiators, MM ignored (gruijter's Intergas, 1412.16)

_LAST = []   # the controller of the run in progress (one run per process at a time)


class Sat1933(cl.Sat):
    variant = "today"
    interval = 30.0

    def __init__(self, e, coeff):
        super().__init__(e, coeff)
        self.ex_s = 0.0
        self.events = []           # (t, "entry"|"exit_under"|"exit_e342"|"exit_sat")
        self.last_t = None
        self.skip = False
        _LAST.append(self)

    def tick2(self, now_s, room, tout, flow, ret, flame, dhw):
        # a control interval of 60 s: every other 30 s call of the harness is skipped (commands stay as they were)
        if self.interval >= 60.0:
            self.skip = not self.skip
            if self.skip:
                return None
        return super().tick2(now_s, room, tout, flow, ret, flame, dhw)

    def tick(self, now_s, room, tout, flow, ret, flame):
        e = self.e
        step = self.interval
        cv = float(cl.port.curve(self.c, cl.TARGET, tout, e["base"]))
        ms = int(1000000 + now_s * 1000)
        self.pid.service(ms, room, cl.TARGET, cv); self.pid.note_room(room, ms)
        sp = min(e["spmax"], max(10.0, float(self.pid.output(cv))))
        self.cut = (sp <= e["cold"] + s16.COLD_HYST) if self.cut else (sp <= e["cold"])
        if self.cut:
            self.pwm = False; self.cs, self.ch = 10.0, False; self.duty = None; self.ex_s = 0.0; return sp
        guard = now_s < self.dhw_guard_until
        if not self.pwm:
            self.ex_s = 0.0
            self.ov_s = self.ov_s + step if (flame and not guard and flow - sp >= ENTRY_MARGIN) else 0.0
            if self.ov_s >= ENTRY_SUSTAIN:
                self.pwm = True; self.ov_s = 0.0; self.ph_on = True; self.ph_el = 0.0; self.sat_s = 0.0
                self.teff = None; self.flame_at = None; self.starts.append(now_s)
                self.events.append((now_s, "entry"))
            else:
                self.cs, self.ch = sp, True; self.duty = None; return sp
        # PWM (the 1412.16 sequence)
        if flame and self.teff is None: self.teff = flow
        if self.ph_on and flame:
            if self.flame_at is None: self.flame_at = now_s; self.teff = flow
            elif now_s - self.flame_at <= 180.0: self.teff = 0.3 * flow + 0.7 * self.teff
        ref = max(self.teff if self.teff else flow, e["base"] + 1.0)
        duty = min(max((sp - e["base"]) / (ref - e["base"]), 0.0), 1.0); self.duty = duty
        on_t, off_t, cont = cl.port.pwm_times(duty, s16.GAS_PERIOD_S, s16.GAS_MINON_S, flame)
        self.ph_el += step
        if self.ph_on:
            if not cont and self.ph_el >= on_t: self.ph_on = False; self.ph_el = 0.0; self.flame_at = None
        elif cont or (on_t >= 180 and self.ph_el >= off_t):
            if len([t for t in self.starts if now_s - t < 3600]) < s16.GAS_MAX_STARTS_H:
                self.ph_on = True; self.ph_el = 0.0; self.starts.append(now_s); self.flame_at = None
        # exits: gates of heating_control.py:308-323 (PWM status ON, no hot water guard, request above cold)
        if self.ph_on and not guard and sp > e["cold"]:
            if self.variant == "today":
                self.ex_s = self.ex_s + step if flow <= sp - UNDER_MARGIN else 0.0
                limit, cause = UNDER_SUSTAIN, "exit_under"
            else:
                self.ex_s = self.ex_s + step if (flame and sp > flow) else 0.0
                limit, cause = E342_SUSTAIN, "exit_e342"
            if self.ex_s >= limit:
                self.pwm = False; self.ex_s = 0.0; self.sat_s = 0.0; self.events.append((now_s, cause))
                self.cs, self.ch = sp, True; self.duty = None; return sp
        else:
            self.ex_s = 0.0
        self.sat_s = self.sat_s + step if cont else 0.0
        if self.sat_s >= 300.0:
            self.pwm = False; self.sat_s = 0.0; self.ex_s = 0.0; self.events.append((now_s, "exit_sat"))
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


def run_one(args):
    variant, interval, g = args
    Sat1933.variant = variant
    Sat1933.interval = interval
    cl.Sat = Sat1933
    _LAST.clear()
    r = cl.run(*g)
    ctl = _LAST[-1]
    days = cl.HOURS / 24.0
    ev = ctl.events
    entries = [t for t, k in ev if k == "entry"]
    under = [t for t, k in ev if k in ("exit_under", "exit_e342")]
    toggles = sum(1 for t in under if any(0.0 < e2 - t <= TOGGLE_WINDOW for e2 in entries))
    r.pop("trace", None)
    r.update({"variant": variant, "interval_s": interval,
              "pwm_entries_per_day": round(len(entries) / days, 2),
              "exit_under_per_day": round(len(under) / days, 2),
              "exit_sat_per_day": round(sum(1 for _, k in ev if k == "exit_sat") / days, 2),
              "toggles_per_day": round(toggles / days, 2),
              "gruijter_like": tuple(g[1:6]) == GRUIJTER_LIKE})
    return r


def grid(quick):
    eras = ("3_1983-92",) if quick else ("3_1983-92", "5_2015")
    for cls in CLASSES:
        for era in eras:
            for em in ("rad", "ufh"):
                for fl in ("fixed", "variable"):
                    for pmin in ((5.5,) if quick else (3.5, 5.5)):
                        for mm in (True, False):
                            for seed in ((1, 2) if quick else (1, 2, 3, 4, 5)):
                                yield (cls, era, em, fl, pmin, mm, seed)


def summarize(runs):
    out = {}
    for v in VARIANTS:
        for cls in CLASSES:
            rs = [r for r in runs if r["variant"] == v and r["class"] == cls]
            if not rs:
                continue
            n = len(rs)
            mean = lambda k: round(sum(r[k] for r in rs) / n, 3)
            out[f"{v}/{cls}"] = {"runs": n, "pwm_entries_per_day": mean("pwm_entries_per_day"),
                                 "exit_under_per_day": mean("exit_under_per_day"), "toggles_per_day": mean("toggles_per_day"),
                                 "toggles_worst": max(r["toggles_per_day"] for r in rs),
                                 "starts_per_day": mean("S4_starts_per_day"), "S1_share": mean("S1_share_in_0_3"),
                                 "S3_worst_under": min(r["S3_max_under"] for r in rs)}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--interval", type=float, default=30.0, help="control interval in s (30 or 60)")
    ap.add_argument("--validity-only", action="store_true", help="only the gruijter-like baseline runs (check V1)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    gs = list(grid(a.quick))
    if a.validity_only:
        jobs = [("today", a.interval, g) for g in gs if tuple(g[1:6]) == GRUIJTER_LIKE]
    else:
        jobs = [(v, a.interval, g) for v in VARIANTS for g in gs]
    with mp.Pool(a.jobs) as pool:
        runs = pool.map(run_one, jobs, chunksize=1)
    me = os.path.abspath(__file__)
    sha = lambda f: hashlib.sha256(open(f, "rb").read()).hexdigest()
    d = os.path.dirname(me)
    meta = {"script_sha256": sha(me), "closedloop_sha256": sha(os.path.join(d, "sat_closedloop_sim_1412_19.py")),
            "study16_sha256": sha(os.path.join(d, "sat_lowload_study_1412_16.py")),
            "port_sha256": sha(os.path.join(d, "sat_sim_port.py")), "hours": cl.HOURS, "interval_s": a.interval,
            "quick": a.quick, "validity_only": a.validity_only, "label": "MODEL RESULT"}
    name = ("validity" if a.validity_only else "runs") + ("" if a.interval == 30.0 else f"_i{int(a.interval)}") + \
           ("_quick" if a.quick else "") + ".json"
    summ = summarize(runs)
    with open(os.path.join(a.out, name), "w", encoding="utf-8") as f:
        json.dump({"meta": meta, "summary": summ, "runs": runs}, f, indent=0, sort_keys=True)
    for k, s in summ.items():
        print("MODEL RESULT", k, s, flush=True)


if __name__ == "__main__":
    main()
