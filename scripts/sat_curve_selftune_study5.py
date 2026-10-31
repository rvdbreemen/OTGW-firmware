"""TASK-1519 round 5: our firmware's settings, then a search for the optimum.
Model = round 4 (two-node house by era, boiler/heat-pump minimum output, thermo-nova PWM rules; boiler cycling not
validated). Firmware settings (src/OTGW-firmware): overshoot margin 2.0 (fOvershootMargin, TASK-1268) vs thermo-nova 3.0;
max flow 62 for gas (radiators, entry 152) and 40 for heat pumps (entry 155); 3 cycles/h; minimum setpoint 10.
Tuner: daily rule on the curve integral (entry 163) over the last D days of clean hours, clean = SAT heating, no hot
water, sun < 50 W/m2 (entry 160), no step in that hour and no SAT PWM (round 4 finding); step STEP when |mean| > THR;
at most 0.2 per change (entry 167); BAND: optional total band around the start coefficient.
Houses: eras 1983-92 .. BENG heavy + 1992-2012 and BENG light (pre-1975 left out: the PID itself oscillates, round 3).
Run: python scripts/sat_curve_selftune_study5.py --weather <json> [--out file]
"""
import itertools, json, os, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
import sat_curve_selftune_study4 as s4

class BandDaily(base.Daily):
    def __init__(self, D, THR, STEP, BAND):
        super().__init__(D, THR, STEP, False); self.BAND = BAND; self.c0 = None
        self.name = f"D {D}d thr{THR} st{STEP}" + (f" band{BAND}" if BAND else "")
    def end_of_day(self, c, day):
        if self.c0 is None: self.c0 = c
        n = super().end_of_day(c, day)
        if self.BAND: n = round(min(self.c0 + self.BAND, max(self.c0 - self.BAND, n)), 3)
        return n

def make5(spec):
    return base.NoTune() if spec[0] == "none" else BandDaily(*spec[1:])

def _init(w, margin):
    base._init(w); s4.OVERSHOOT_MARGIN = margin; s4.MAX_SP_GAS = 62.0; s4.MAX_SP_HP = 40.0; s4.EXCLUDE_PWM = True
    s4.make = make5

def job(a):
    r = s4.run(a); r["margin"] = s4.OVERSHOOT_MARGIN; return r

if __name__ == "__main__":
    w = sys.argv[sys.argv.index("--weather") + 1]
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "study5_results.json"
    specs = [("none",)] + [("B", D, THR, STEP, BAND) for D, THR, STEP, BAND in itertools.product((2, 3, 4), (0.5, 1.0, 1.5), (0.05, 0.1), (0.0, 0.4))]
    houses = [(e, "heavy", 1.0) for e in ("3_1983-92", "4_1992-2012", "5_2015", "6_BENG2021")] + [("4_1992-2012", "light", 1.0), ("6_BENG2021", "light", 1.0)]
    jobs = [(sp, *h, off, 30, sd, False, src) for sp in specs for h in houses for off in (-0.5, 0.0, 0.5) for sd in (1, 2) for src in ("gas", "hp30", "hp60")]
    res = []
    for margin in (2.0, 3.0):
        print(f"margin {margin}: {len(jobs)} runs", flush=True)
        with Pool(6, initializer=_init, initargs=(w, margin)) as p: res += p.map(job, jobs, chunksize=4)
    key = lambda r: (r["margin"], r["source"], r["era"], r["mass"], r["seed"])
    ref = {key(r): r["fuel_J"] for r in res if r["spec"] == "none" and r["c_off"] == 0.0}
    for r in res: r["gas_pct"] = 100.0 * r["fuel_J"] / ref[key(r)]
    json.dump(res, open(out, "w")); print("done", len(res))
