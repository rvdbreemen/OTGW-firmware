"""TASK-1519 round 5f (and 5g with --guard: the net skips oscillating days, swing > 0.5 C, and days > 60 min at the max-flow cap): an unbiased cold-day safety net next to the round 5b rule.

Rounds 5d/5e: counting PWM / minimum-output hours only when the room is cold rescues low-load houses from a curve
that is too low, but any one-sided selection on room temperature keeps the cold half of the cycling and drifts the
curve up on the plateau (gas worst +363-416 min/day). Here the main rule stays round 5b (curve integral on clean hours,
PWM and minimum-output hours excluded both ways). Next to it: if the mean room error over ALL heating minutes of the
last D days (no selection) is below -COLDDAY, step up by STEP. A warm room at low load triggers nothing: the curve is
not the lever there. Symmetric threshold 1.0, 2 days, step 0.1; COLDDAY 0.2 / 0.3 / 0.5.
Run: python scripts/sat_curve_selftune_study5f.py --weather <json> --krs <round5c_runs.json> --outdir <dir>
"""
import json, os, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b
import sat_curve_selftune_study5c as s5c

class ColdDayNet(s5c.AsymDaily):
    def __init__(self, D, UP, DN, STEP, COLD, GUARD=False):
        super().__init__(D, UP, DN, STEP); self.COLD = COLD; self.GUARD = GUARD; self.errs = []
        self.name += f" coldday{COLD}" + (" guard" if GUARD else "")
    def end_of_day(self, c, day):
        bad = self.GUARD and ((getattr(self, "day_swing", None) or 0.0) > 0.5 or getattr(self, "day_capped", 0) > 60)
        if bad: self.errs = []   # round 5g: an oscillating day (swing > 0.5 C, round 6) or a day at the max-flow cap says nothing about the curve
        elif getattr(self, "day_err", None) is not None: self.errs = (self.errs + [self.day_err])[-self.D:]
        n = super().end_of_day(c, day)
        if n == c and len(self.errs) >= self.D and statistics.fmean(self.errs) < -self.COLD:
            self.errs = []; self.days = []; return round(min(5.0, c + self.STEP), 2)
        if n != c: self.errs = []
        return n

def make5f(spec):
    if spec[0] == "none": return base.NoTune()
    return ColdDayNet(*spec[1:]) if len(spec) >= 6 else s5c.AsymDaily(*spec[1:])

def _init(w, krs):
    s5c._init(w, krs); s4.make = make5f; s4.EXCLUDE_PWM = True; s4.EXCLUDE_ATMIN = True

def job(a):
    spec, era, mass, start, seed, src = a; kr = s5b.KRS[f"{era}|{mass}|1.8"]
    r = s4.run((spec, era, mass, kr, round(start - s5b._cb(era, mass, kr), 3), 30, seed, False, src)); r.update(start=start); return r

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]; out = sys.argv[sys.argv.index("--outdir") + 1]
    krs = json.load(open(sys.argv[sys.argv.index("--krs") + 1]))["krs"]
    specs = [("none",), ("A", 2, 1.0, 1.0, 0.1)] + [("A", 2, 1.0, 1.0, 0.1, cd) for cd in (0.2, 0.3, 0.5)]
    if "--guard" in sys.argv: specs = [("none",)] + [("A", 2, 1.0, 1.0, 0.1, cd, True) for cd in (0.2, 0.3)]   # round 5g
    jobs = [(sp, e, m, st, sd, src) for sp in specs for e, m in s5b.HOUSES for st in (1.0, 1.2, 1.4, 1.8, 2.4) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    with Pool(6, initializer=_init, initargs=(w, krs)) as p: res = p.map(job, jobs, chunksize=2)
    name = sys.argv[sys.argv.index("--name") + 1] if "--name" in sys.argv else "round5f_runs.json"
    json.dump(res, open(os.path.join(out, name), "w")); print("done", len(res))
