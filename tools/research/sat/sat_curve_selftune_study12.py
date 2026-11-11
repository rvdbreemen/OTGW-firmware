"""TASK-1796 round 12: the ADR-228 phase-agreement rule against the ADR-205 rule and no tuner, per house type.

Study evidence from the research model (sat_curve_selftune_study4.run), not a test of the firmware: the firmware tuner is
tested by tests/test_sat_curve_selftune.py, which compiles the real code.

Rule P (ADR-228, Sergeant D entries 303, 304): every heating minute of a clean hour goes to the warm-up phase (|err| > 0.1 C)
or the floating phase (|err| <= 0.1 C); each phase keeps its own tick-weighted mean of setpoint - curve over the last 3 counted
days (>= 6 clean hours); above +1.0 C a phase says higher, below -1.0 C lower; the coefficient moves 0.1 only when both phases
say the same, and the window starts empty after a change. A phase without a minute in the window holds.
Variant Q (NOT decided, comparison data for Sergeant D): as P, but a phase without a minute in the window does not veto;
the other phase decides alone. Round 12 smoke run: with too low a curve the room never reaches the deadband in clean hours,
so P never raises (reason no_floating).
Rule A = ADR-205 as decided (round 10 setting: 3-day window, symmetric 1.0, step 0.1, PID kept on a step, ADR-204).
Same houses, radiator sizes (round 5c krs), starts, seeds and sources as rounds 9-10, paired, on 2025-26 (calibration
winter) and 2010-11 (coldest of the 20).
Run: python tools/research/sat/sat_curve_selftune_study12.py --weather2526 <json> --weather1011 <json> --krs <round5c_runs.json> --out <json>
"""
import json, os, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study5c as s5c
import sat_curve_selftune_study8 as s8

class PhaseDaily:
    def __init__(self, D, THR, STEP, NOVETO=False):
        self.D, self.THR, self.STEP, self.NOVETO = D, THR, STEP, NOVETO
        self.name = f"{'Q' if NOVETO else 'P'} {D}d thr{THR} st{STEP}"
        self.ph_hour = (0.0, 0, 0.0, 0)
        self.clean_today = 0; self.today = [0.0, 0, 0.0, 0]; self.days = []
        self.holds = {"no_warmup": 0, "no_floating": 0, "inside_band": 0, "phases_disagree": 0, "few_days": 0}
    def hour(self, c, day, mean_corr, clean):
        if clean:
            self.clean_today += 1
            for i in range(4): self.today[i] += self.ph_hour[i]
        return c
    def end_of_day(self, c, day):
        if self.clean_today >= 6: self.days = (self.days + [tuple(self.today)])[-self.D:]
        self.clean_today = 0; self.today = [0.0, 0, 0.0, 0]
        if len(self.days) < self.D: self.holds["few_days"] += 1; return c
        ws = sum(d[0] for d in self.days); wn = sum(d[1] for d in self.days)
        fs = sum(d[2] for d in self.days); fn = sum(d[3] for d in self.days)
        say = lambda m: 1 if m > self.THR else (-1 if m < -self.THR else 0)
        if wn == 0 and fn == 0: self.holds["no_warmup"] += 1; return c
        if not self.NOVETO:
            if wn == 0: self.holds["no_warmup"] += 1; return c
            if fn == 0: self.holds["no_floating"] += 1; return c
        w = say(ws / wn) if wn else None
        f = say(fs / fn) if fn else None
        if w is None: w = f
        if f is None: f = w
        if w == 0 and f == 0: self.holds["inside_band"] += 1; return c
        if w != f: self.holds["phases_disagree"] += 1; return c
        self.days = []
        return round(min(5.0, max(0.1, c + w * self.STEP)), 2)

def make12(spec):
    if spec[0] == "P": return PhaseDaily(*spec[1:])
    if spec[0] == "Q": return PhaseDaily(*spec[1:], NOVETO=True)
    return s5c.make5c(spec)

_last = {}
def _init(w, krs):
    s8._init(w, krs); s4.STEP_KEEPS_PID = True; s4.make = _make_keep

def _make_keep(spec):
    st = make12(spec); _last["st"] = st; return st

def job(a):
    r = s8.job(a)
    st = _last.get("st")
    if isinstance(st, PhaseDaily): r["holds"] = dict(st.holds)
    return r

def jobs():
    out = []
    for spec in (("none",), ("A", 3, 1.0, 1.0, 0.1), ("P", 3, 1.0, 0.1), ("Q", 3, 1.0, 0.1)):
        out += [("rule", spec, e, m, st, sd, src) for e, m in s5b.HOUSES for st in (1.0, 1.2, 1.4, 1.8, 2.4) for sd in (1, 2)
                for src in ("gas", "hp30", "hp60")]
    return out

def summary(res):
    rows = {}
    for r in res:
        k = (r["era"], r["mass"], r["source"], r["spec"].split()[0])
        rows.setdefault(k, []).append(r)
    out = []
    for (era, mass, src, spec), rs in sorted(rows.items()):
        out.append({"house": f"{era}|{mass}", "source": src, "rule": spec, "n": len(rs),
                    "rms": round(statistics.fmean(r["rms"] for r in rs), 3),
                    "out_band_min_d": round(statistics.fmean(r["out_band_min_d"] for r in rs), 1),
                    "abs_c_end_minus_cbest": round(statistics.fmean(abs(r["c_end"] - r["cbest"]) for r in rs), 3),
                    "writes": round(statistics.fmean(r["writes"] for r in rs), 2),
                    "flips": round(statistics.fmean(r["flips"] for r in rs), 2)})
    return out

if __name__ == "__main__":
    arg = lambda k: sys.argv[sys.argv.index(k) + 1]
    krs = json.load(open(arg("--krs")))["krs"]; res = {}
    workers = int(arg("--workers")) if "--workers" in sys.argv else 8
    for label, w in (("2025_26", arg("--weather2526")), ("2010_11", arg("--weather1011"))):
        with Pool(workers, initializer=_init, initargs=(w, krs)) as p: res[label] = p.map(job, jobs(), chunksize=2)
        print(label, "done", len(res[label]), flush=True)
    json.dump({"runs": res, "summary": {k: summary(v) for k, v in res.items()}}, open(arg("--out"), "w"), indent=1)
