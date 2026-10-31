"""TASK-1519 round 5c: does excluding minimum-output hours hide the cliff, and does an asymmetric threshold (Q-1519e B) help?

Model = round 5b (radiators calibrated per house, PWM hours excluded). Clean-rule variants: PWM only; PWM + minimum
output (both sides, round 5b); PWM + minimum output only while the room is above target ("above"). Thresholds:
symmetric 1.0 C, and asymmetric raise at 0.5 / lower at 1.0 C (Q-1519e option B). Starts 1.0-2.4 on houses balanced
at 1.8, plus houses balanced at 1.6 and 2.4 (entry 166 range) started at 1.8: does the rule find an optimum that is
not where it starts? Daily rule, 2 days, step 0.1, each run paired against no tuner.
Run: python scripts/sat_curve_selftune_study5c.py --weather <json> --outdir <dir>
"""
import json, os, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study3 as s3
import sat_curve_selftune_study4 as s4
import sat_curve_selftune_study5b as s5b

class AsymDaily(base.Daily):
    def __init__(self, D, UP, DN, STEP):
        super().__init__(D, DN, STEP, False); self.UP = UP; self.name = f"D {D}d up{UP} dn{DN} st{STEP}"
    def end_of_day(self, c, day):
        if len(self.today) >= 6: self.days = (self.days + [statistics.fmean(self.today)])[-self.D:]
        self.today = []
        if len(self.days) < self.D: return c
        m = statistics.fmean(self.days)
        if m > self.UP or m < -self.THR:
            self.days = []; return round(min(5.0, max(0.1, c + (self.STEP if m > 0 else -self.STEP))), 2)
        return c

def make5c(spec):
    return base.NoTune() if spec[0] == "none" else AsymDaily(*spec[1:])

def _init(w, krs):
    s5b._init(w, krs); s5b.KRS = krs; s4.make = make5c

def job(a):
    spec, atmin, era, mass, bal, start, seed, src = a; s4.EXCLUDE_ATMIN = atmin; kr = s5b.KRS[f"{era}|{mass}|{bal}"]
    r = s4.run((spec, era, mass, kr, round(start - s5b._cb(era, mass, kr), 3), 30, seed, False, src))
    r.update(start=start, atmin=str(atmin), balance=bal); return r

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]; out = sys.argv[sys.argv.index("--outdir") + 1]
    base._init(w); season = statistics.fmean(t for t in base.WEATHER["w"][0] if t < 15)
    cases = [(e, m, 1.8, st) for e, m in s5b.HOUSES for st in (1.0, 1.2, 1.4, 1.8, 2.4)]
    cases += [(e, "heavy", bal, 1.8) for e in ("3_1983-92", "4_1992-2012", "6_BENG2021") for bal in (1.6, 2.4)]
    krs = {f"{e}|{m}|{bal}": s5b.kr_for(e, m, season, bal) for e, m, bal, _ in cases}
    print("krs", krs, flush=True)
    specs = [(("none",), False)] + [(("A", 2, up, 1.0, 0.1), a) for up in (1.0, 0.5) for a in (False, True, "above")]
    jobs = [(sp, a, e, m, bal, st, sd, src) for sp, a in specs for e, m, bal, st in cases for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    with Pool(6, initializer=_init, initargs=(w, krs)) as p: res = p.map(job, jobs, chunksize=2)
    json.dump({"krs": krs, "res": res}, open(os.path.join(out, "round5c_runs.json"), "w")); print("done", len(res))
