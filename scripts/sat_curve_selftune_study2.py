"""TASK-1519 study round 2: proportional steps (entry 167: at most 0.2 per change, 3 decimals), fast (within a day)
vs slow (3 days) convergence, a comfort fast lane, and gas use. Same validated plant, PID and real weather as
sat_curve_selftune_study.py (see its docstring for the model and its limits).

Proportional step: the curve moves raw(Tout)/4 C of flow per unit of coefficient (heating_curve.py:28-39), so a
measured mean correction X (setpoint minus curve, C of flow) maps to dc = GAIN * X / (raw/4), clamped to +-0.2 and
rounded to 3 decimals. GAIN < 1 damps the step.
New scores: minutes per day outside +-0.3 C, minutes above setpoint + 0.3 (overshoot), and heat delivered over the
scored days relative to the same house with the best season coefficient and no tuner (gas %, 100 = as good).
Run: python scripts/sat_curve_selftune_study2.py --weather <weather json> [--out file]
"""
import itertools, json, math, os, random, statistics, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sat_curve_selftune_study as base
from sat_curve_selftune_study import curve, best_coeff, TARGET, DT

def raw4(to):
    return (0.03 * (to - 20) ** 2 - 0.4 * (to - 20)) / 4      # d(curve)/d(coefficient) at target 20

def prop_step(mean_corr, to, gain):
    dc = gain * mean_corr / max(raw4(to), 0.5)
    return max(-0.2, min(0.2, dc))

class PropWindow:
    """Decide every hour on a sliding window of the last W clean hours (W=24*D for days); N consecutive evaluations
    beyond THR (C flow) and the same sign; proportional step; at most one change per MINGAP hours.
    FAST: if the last FAST_H clean hours are all beyond FAST_THR, step at once (comfort fast lane)."""
    def __init__(self, W, N, THR, GAIN, MINGAP, FAST_H=0, FAST_THR=99.0, label=""):
        self.W, self.N, self.THR, self.GAIN, self.MINGAP = W, N, THR, GAIN, MINGAP
        self.FAST_H, self.FAST_THR = FAST_H, FAST_THR
        self.name = label or f"P w{W}h n{N} thr{THR} g{GAIN} gap{MINGAP}h" + (f" fast{FAST_H}h>{FAST_THR}" if FAST_H else "")
        self.hist = []; self.touts = []; self.agree = 0; self.sign = 0; self.since = 10**6
    def hour(self, c, day, mean_corr, clean, tout=5.0):
        self.since += 1
        if clean:
            self.hist = (self.hist + [mean_corr])[-max(self.W, self.FAST_H):]
            self.touts = (self.touts + [tout])[-max(self.W, self.FAST_H):]
        if not clean: return c
        if self.FAST_H and len(self.hist) >= self.FAST_H and self.since >= 2:
            last = self.hist[-self.FAST_H:]
            if all(x > self.FAST_THR for x in last) or all(x < -self.FAST_THR for x in last):
                m = statistics.fmean(last); dc = prop_step(m, statistics.fmean(self.touts[-self.FAST_H:]), self.GAIN)
                self.hist = []; self.touts = []; self.agree = 0; self.since = 0
                return round(min(5.0, max(0.1, c + dc)), 3)
        if len(self.hist) < self.W or self.since < self.MINGAP: return c
        win = self.hist[-self.W:]; m = statistics.fmean(win)
        if abs(m) <= self.THR: self.agree = 0; return c
        s = 1 if m > 0 else -1
        self.agree = self.agree + 1 if s == self.sign else 1; self.sign = s
        if self.agree < self.N: return c
        dc = prop_step(m, statistics.fmean(self.touts[-self.W:]), self.GAIN)
        self.hist = []; self.touts = []; self.agree = 0; self.since = 0
        return round(min(5.0, max(0.1, c + dc)), 3)
    def end_of_day(self, c, day): return c

class NoTune(base.NoTune):
    def hour(self, c, *a, **k): return c

def make(spec):
    if spec[0] == "none": return NoTune()
    return PropWindow(*spec[1:])

def run(args):
    spec, tau_h, kr, area, c_off, seed = args[:6]
    trace = [] if len(args) > 6 and args[6] else None
    tm, sm = base.WEATHER["w"]; minutes = len(tm)
    H = 150.0; K = kr * H; C = tau_h * 3600 * H
    rnd = random.Random(seed); gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    cbest = best_coeff(kr, statistics.fmean(t for t in tm if t < 15))
    c = round(min(5.0, max(0.1, cbest + c_off)), 3)
    st = make(spec)
    T = TARGET; flow = 30.0; integ = 0.0; last = None; rd = 0.0
    corr_acc = 0.0; corr_n = 0; clean = True; stepped = False; tout_acc = 0.0
    sq = 0.0; n = 0; out_band = 0; over = 0; heat = 0.0
    writes = flips = wrong = 0; last_dir = 0; conv_day = None; maxdc = 0.0
    for m in range(minutes):
        day = m // 1440; tout, sw = tm[m], sm[m]; heating = tout < 15
        cv = curve(c, tout); kp = c * cv / 3; ki = kp / 8400; kd = 588.0 * kp
        err = TARGET - T
        if last is not None:
            d_ = T - last; rd = 0.0 if d_ == 0 else -d_ / DT
        last = T
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = round(cv + kp * err + integ + kd * rd, 1) if heating else 10.0
        draw = (420 <= m % 1440 < 430) or (1140 <= m % 1440 < 1150)
        flow += ((min(sp, 70.0) if not draw else T) - flow) * DT / 300.0
        q = K * max(flow - 5.0 - T, 0.0) ** 1.3 if not draw else 0.0
        T += (q - H * (T - tout) + area * sw + gains[m]) * DT / C
        if heating:
            corr_acc += sp - cv; corr_n += 1; tout_acc += tout
            if day >= 14:
                sq += err * err; n += 1; heat += q * DT
                if abs(T - TARGET) > 0.3: out_band += 1
                if T > TARGET + 0.3: over += 1
        clean &= heating and not draw and sw < 50.0
        if trace is not None and m % 60 == 0:
            trace.append((m // 60, round(tout, 2), round(sw, 1), round(T, 3), sp, cv, c, gains[m]))
        if m % 60 == 59:
            mc = corr_acc / corr_n if corr_n else 0.0; mt = tout_acc / corr_n if corr_n else tout
            new_c = st.hour(c, day, mc, clean and not stepped and corr_n > 0, mt)
            corr_acc = 0.0; corr_n = 0; tout_acc = 0.0; clean = True; stepped = False
            if new_c != c:
                b = best_coeff(kr, statistics.fmean(tm[max(0, m - 1439):m + 1]))
                if b is not None and abs(new_c - b) > abs(c - b) + 1e-9: wrong += 1
                dr = 1 if new_c > c else -1
                if last_dir and dr != last_dir: flips += 1
                maxdc = max(maxdc, abs(new_c - c)); last_dir = dr; writes += 1; c = new_c; integ = 0.0; rd = 0.0; stepped = True
        if m % 1440 == 1439 and conv_day is None and abs(c - cbest) <= 0.1: conv_day = day
    days = max(1.0, n / 1440)
    if trace is not None:
        return {"spec": st.name, "trace": trace}
    return {"spec": st.name, "tau": tau_h, "kr": kr, "area": area, "c_off": c_off, "seed": seed,
            "rms": math.sqrt(sq / n), "out_band_min_d": out_band / days, "over_min_d": over / days, "heat_J": heat,
            "writes": writes, "flips": flips, "wrong": wrong, "conv_day": conv_day, "c_end": c, "cbest": cbest, "maxdc": maxdc}

if __name__ == "__main__":
    wpath = sys.argv[sys.argv.index("--weather") + 1]
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "study2_results.json"
    specs = [("none",)]
    for W, N, THR, GAIN in itertools.product((3, 6), (1, 3), (1.0, 2.0), (0.3, 0.6)):          # within a day
        specs.append(("P", W, N, THR, GAIN, 12))
    for W, THR, GAIN in itertools.product((24, 72), (1.0, 2.0), (0.3, 0.6, 1.0)):              # 1 day / 3 days
        specs.append(("P", W, 1, THR, GAIN, W))
    for W, GAIN in itertools.product((24, 72), (0.3, 0.6)):                                     # slow + fast lane
        specs.append(("P", W, 1, 1.0, GAIN, W, 4, 4.0))
    houses = [(tau, kr, area) for tau in (40, 110, 150) for kr in (0.75, 1.0, 1.2) for area in (0.5, 2.5)]
    offsets = (-1.0, -0.5, 0.0, 0.5, 1.0); seeds = (1, 2)
    jobs = [(s, *h, off, sd) for s in specs for h in houses for off in offsets for sd in seeds]
    print(f"{len(specs)} strategies x {len(houses)} houses (coefficient 1.5-2.4) x {len(offsets)} start curves x {len(seeds)} seeds = {len(jobs)} runs", flush=True)
    with Pool(6, initializer=base._init, initargs=(wpath,)) as p:
        res = p.map(run, jobs, chunksize=4)
    ref = {(r["tau"], r["kr"], r["area"], r["seed"]): r["heat_J"] for r in res if r["spec"] == "none" and r["c_off"] == 0.0}
    for r in res: r["gas_pct"] = 100.0 * r["heat_J"] / ref[(r["tau"], r["kr"], r["area"], r["seed"])]
    json.dump(res, open(out, "w"))
    by = {}
    for r in res: by.setdefault(r["spec"], []).append(r)
    print(f"{'strategy':44s} {'rms':>6} {'out±.3':>7} {'over':>6} {'gas%':>6} {'writes':>6} {'flips':>5} {'wrong':>5} {'conv%':>5} {'conv_d':>6}")
    rows = []
    for k, rs in by.items():
        cv = [r["conv_day"] for r in rs if r["conv_day"] is not None]
        rows.append((statistics.fmean(r["out_band_min_d"] for r in rs), k, statistics.fmean(r["rms"] for r in rs),
                     statistics.fmean(r["over_min_d"] for r in rs), statistics.fmean(r["gas_pct"] for r in rs),
                     statistics.fmean(r["writes"] for r in rs), statistics.fmean(r["flips"] for r in rs),
                     statistics.fmean(r["wrong"] for r in rs), 100 * len(cv) / len(rs), statistics.fmean(cv) if cv else float("nan")))
    for r in sorted(rows):
        print(f"{r[1]:44s} {r[2]:6.3f} {r[0]:7.1f} {r[3]:6.1f} {r[4]:6.1f} {r[5]:6.1f} {r[6]:5.1f} {r[7]:5.1f} {r[8]:5.0f} {r[9]:6.1f}")
