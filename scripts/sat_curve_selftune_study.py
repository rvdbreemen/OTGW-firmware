"""TASK-1519 study: hourly-with-guards vs daily heating-curve self-tune, Monte Carlo on validated houses and real weather.

Controller: thermo-nova at the pin (7beaaf5): heating_curve.py:28-39 (curve, base 27.2), pid.py (kp = c*curve/3,
ki = kp/8400, kd = 0.07*8400*kp, integral zeroed outside DEADBAND 0.1, clamp +-curve), PID step every 60 s (entry 42).
Tuner signal: the curve integral, mean of (setpoint - curve) over clean minutes (Sergeant D entry 163).
Clean = SAT heating (outside < 15 C), no hot-water draw, sun below ~12 deg approximated as shortwave < 50 W/m2
(entry 160), no tuner step in that hour.

Plant: one thermal mass. Q_rad = K*(Tmean - Troom)^1.3, Tmean = flow - 5; loss H*(Troom - Tout); solar = A*shortwave;
household gains (cooking 17:30-18:30 1.2 kW daily, 1-3 random 0.3-0.9 kW bursts of 30-120 min, a party night per
fortnight 1.5 kW 19-24 h); two 10-minute hot-water draws a day (no CH heat). Boiler flow follows the setpoint with a
5 min lag, max 70 C, continuous modulation (no PWM, no minimum modulation).
Validation (scripts/sat_curve_selftune_study.py --validate): a field capture (4 h) fixes the ratios K/H ~ 1 and
tau = C/H ~ 110 h with RMSE 0.033 C on the fit half and 0.069 C on the held-out half; the absolute scale is not
identifiable from 4 h, but the tuner only depends on the ratios. The study spans tau 40/110 h and K/H 0.6/1.0/1.6.
Weather: Open-Meteo archive, 52.1N 6.2E, 2025-11-01..2026-02-28, hourly temperature_2m and shortwave_radiation
(file weather_2025_26.json next to this script or given with --weather).
"""
import json, math, os, random, statistics, sys, itertools
from multiprocessing import Pool

TARGET = 20.0; BASE = 27.2; DT = 60

def curve(c, to):
    return round(BASE + (c / 4) * (4 * (TARGET - 20) + 0.03 * (to - 20) ** 2 - 0.4 * (to - 20)), 1)

def best_coeff(kr, to):
    if to >= TARGET - 0.5: return None
    need = ((TARGET - to) / kr) ** (1 / 1.3) + 5 + TARGET          # K/H = kr
    return min((abs(curve(c / 10, to) - need), c / 10) for c in range(1, 51))[1]

def load_weather(path):
    h = json.load(open(path))["hourly"]
    T, S = h["temperature_2m"], h["shortwave_radiation"]
    tm, sm = [], []
    for i in range(len(T) - 1):
        for k in range(60):
            a = k / 60; tm.append(T[i] * (1 - a) + T[i + 1] * a); sm.append(S[i] * (1 - a) + S[i + 1] * a)
    return tm, sm

# ------------------------------------------------------------------ strategies
class Hourly:
    """Robert: a decision every hour on a sliding window of the last W clean hours; a step needs N consecutive
    hourly evaluations beyond THR with the same sign; step STEP; at most CAP total change per calendar day."""
    def __init__(self, W, N, THR, STEP, CAP=0.2):
        self.W, self.N, self.THR, self.STEP, self.CAP = W, N, THR, STEP, CAP
        self.name = f"H w{W}h n{N} thr{THR} st{STEP}"
        self.hist = []; self.agree = 0; self.sign = 0; self.day = -1; self.day_delta = 0.0
    def hour(self, c, day, mean_corr, clean):
        if day != self.day: self.day, self.day_delta = day, 0.0
        if clean: self.hist = (self.hist + [mean_corr])[-self.W:]
        if not clean or len(self.hist) < self.W: return c
        m = statistics.fmean(self.hist)
        if abs(m) <= self.THR: self.agree = 0; return c
        s = 1 if m > 0 else -1
        self.agree = self.agree + 1 if s == self.sign else 1; self.sign = s
        if self.agree >= self.N and self.day_delta + self.STEP <= self.CAP + 1e-9:
            self.agree = 0; self.hist = []; self.day_delta += self.STEP
            return round(min(5.0, max(0.1, c + s * self.STEP)), 2)
        return c
    def end_of_day(self, c, day): return c

class Daily:
    """Sergeant D: one decision per day on the clean hours of the last D days (>= 6 per day); beyond THR -> step;
    with UNDO, a step whose next day is worse (|mean| grew) is undone (entry 164)."""
    def __init__(self, D, THR, STEP, UNDO=True):
        self.D, self.THR, self.STEP, self.UNDO = D, THR, STEP, UNDO
        self.name = f"D {D}d thr{THR} st{STEP}{' undo' if UNDO else ''}"
        self.today = []; self.days = []; self.prev = None; self.last = 0.0
    def hour(self, c, day, mean_corr, clean):
        if clean: self.today.append(mean_corr)
        return c
    def end_of_day(self, c, day):
        if len(self.today) >= 6: self.days = (self.days + [statistics.fmean(self.today)])[-self.D:]
        self.today = []
        if len(self.days) < self.D: return c
        m = statistics.fmean(self.days)
        if self.UNDO and self.last and self.prev is not None and abs(m) > abs(self.prev):
            c = round(c - self.last, 2); self.last = 0.0; self.prev = m; self.days = []; return c
        self.prev = m; self.last = 0.0
        if abs(m) > self.THR:
            self.last = self.STEP if m > 0 else -self.STEP; self.days = []
            return round(min(5.0, max(0.1, c + self.last)), 2)
        return c

class NoTune:
    name = "none"
    def hour(self, c, *a): return c
    def end_of_day(self, c, day): return c

def make(spec):
    kind = spec[0]
    if kind == "none": return NoTune()
    if kind == "H": return Hourly(*spec[1:])
    return Daily(*spec[1:])

# ------------------------------------------------------------------ one run
def run(args):
    spec, tau_h, kr, area, c_off, seed, weather = args[:7]
    trace = [] if len(args) > 7 and args[7] else None
    tm, sm = WEATHER[weather]
    minutes = len(tm)
    H = 150.0; K = kr * H; C = tau_h * 3600 * H
    rnd = random.Random(seed)
    gains = [0.0] * minutes
    for d in range(minutes // 1440):
        for m in range(d * 1440 + 1050, d * 1440 + 1110): gains[m] += 1200.0
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + int(rnd.uniform(8, 22) * 60); w = rnd.uniform(300, 900)
            for m in range(st, min(minutes, st + int(rnd.uniform(30, 120)))): gains[m] += w
        if d % 14 == 9:
            for m in range(d * 1440 + 1140, d * 1440 + 1440): gains[m] += 1500.0
    season_mean = statistics.fmean(t for t in tm if t < 15)
    c_best_season = best_coeff(kr, season_mean)
    c = round(min(5.0, max(0.1, c_best_season + c_off)), 2)
    st = make(spec)
    T = TARGET; flow = 30.0
    integ = 0.0; last_room = None; rd = 0.0
    corr_acc = 0.0; corr_n = 0; clean_hour = True; stepped = False
    sq = 0.0; nsq = 0; over_min = 0; under_min = 0
    writes = 0; flips = 0; last_dir = 0; wrong = 0; conv_day = None
    for m in range(minutes):
        day, h = m // 1440, (m % 1440) / 60
        tout, sw = tm[m], sm[m]
        heating = tout < 15
        # thermo-nova PID, every 60 s
        cv = curve(c, tout); kp = c * cv / 3; ki = kp / 8400; kd = 588.0 * kp
        err = TARGET - T
        if last_room is not None:
            d_ = T - last_room; rd = 0.0 if d_ == 0 else -d_ / DT
        last_room = T
        if abs(err) > 0.1: integ = 0.0
        else: integ = max(-cv, min(cv, integ + ki * err * DT))
        sp = round(cv + kp * err + integ + kd * rd, 1) if heating else 10.0
        draw = (420 <= m % 1440 < 430) or (1140 <= m % 1440 < 1150)
        tgt = min(sp, 70.0) if not draw else T
        flow += (tgt - flow) * DT / 300.0
        q = K * max(flow - 5.0 - T, 0.0) ** 1.3 if not draw else 0.0
        T += (q - H * (T - tout) + area * sw + gains[m]) * DT / C
        if heating:
            corr_acc += sp - cv; corr_n += 1
            if day >= 14:
                sq += err * err; nsq += 1
                if T > TARGET + 0.5: over_min += 1
                if T < TARGET - 0.5: under_min += 1
        clean_hour &= heating and not draw and sw < 50.0
        if trace is not None and m % 60 == 0:
            trace.append((m // 60, round(tout, 2), round(sw, 1), round(T, 3), sp, cv, c, gains[m]))
        if m % 60 == 59:
            mc = corr_acc / corr_n if corr_n else 0.0
            new_c = st.hour(c, day, mc, clean_hour and not stepped and corr_n > 0)
            corr_acc = 0.0; corr_n = 0; clean_hour = True; stepped = False
            if new_c != c:
                b = best_coeff(kr, statistics.fmean(tm[max(0, m - 1439):m + 1]))
                if b is not None and abs(new_c - b) > abs(c - b) + 1e-9: wrong += 1
                dr = 1 if new_c > c else -1
                if last_dir and dr != last_dir: flips += 1
                last_dir = dr; writes += 1; c = new_c; integ = 0.0; rd = 0.0; stepped = True
        if m % 1440 == 1439:
            new_c = st.end_of_day(c, day)
            if new_c != c:
                b = best_coeff(kr, statistics.fmean(tm[m - 1439:m + 1]))
                if b is not None and abs(new_c - b) > abs(c - b) + 1e-9: wrong += 1
                dr = 1 if new_c > c else -1
                if last_dir and dr != last_dir: flips += 1
                last_dir = dr; writes += 1; c = new_c; integ = 0.0; rd = 0.0
            if conv_day is None and abs(c - c_best_season) <= 0.1: conv_day = day
    days_scored = max(1, nsq / 1440)
    if trace is not None:
        return {"spec": st.name, "trace": trace}
    return {"spec": st.name, "tau": tau_h, "kr": kr, "area": area, "c_off": c_off, "seed": seed,
            "rms": math.sqrt(sq / nsq) if nsq else None, "over_min_per_day": over_min / days_scored,
            "under_min_per_day": under_min / days_scored, "writes": writes, "flips": flips, "wrong": wrong,
            "conv_day": conv_day, "c_end": c, "c_best_season": c_best_season}

WEATHER = {}
def _init(path):
    WEATHER["w"] = load_weather(path)

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    wpath = sys.argv[sys.argv.index("--weather") + 1] if "--weather" in sys.argv else os.path.join(here, "weather_2025_26.json")
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else "study_results.json"
    specs = [("none",)]
    for W, N, THR in itertools.product((1, 3, 6, 12, 24), (1, 3, 6), (1.0, 2.0, 3.0)):
        specs.append(("H", W, N, THR, 0.1))
    for D, THR, UNDO in itertools.product((1, 2, 3), (1.0, 2.0, 3.0), (True, False)):
        specs.append(("D", D, THR, 0.1, UNDO))
    houses = [(tau, kr, area) for tau in (40, 110, 150) for kr in (0.6, 1.0, 1.6) for area in (0.5, 2.5)]
    offsets = (-1.0, -0.5, 0.0, 0.5, 1.0); seeds = (1, 2)
    jobs = [(s, tau, kr, area, off, seed, "w") for s in specs for (tau, kr, area) in houses
            for off in offsets for seed in seeds]
    print(f"{len(specs)} strategies x {len(houses)} houses x {len(offsets)} start curves x {len(seeds)} seeds = {len(jobs)} runs", flush=True)
    with Pool(6, initializer=_init, initargs=(wpath,)) as p:
        res = p.map(run, jobs, chunksize=4)
    json.dump(res, open(out, "w"))
    by = {}
    for r in res: by.setdefault(r["spec"], []).append(r)
    print(f"{'strategy':28s} {'rms':>6} {'over/d':>7} {'under/d':>8} {'writes':>7} {'flips':>6} {'wrong':>6} {'conv%':>6} {'conv_d':>7}")
    rows = []
    for k, rs in by.items():
        conv = [r["conv_day"] for r in rs if r["conv_day"] is not None]
        rows.append((statistics.fmean(r["rms"] for r in rs), k, statistics.fmean(r["over_min_per_day"] for r in rs),
                     statistics.fmean(r["under_min_per_day"] for r in rs), statistics.fmean(r["writes"] for r in rs),
                     statistics.fmean(r["flips"] for r in rs), statistics.fmean(r["wrong"] for r in rs),
                     100 * len(conv) / len(rs), statistics.fmean(conv) if conv else float("nan")))
    for r in sorted(rows):
        print(f"{r[1]:28s} {r[0]:6.3f} {r[2]:7.1f} {r[3]:8.1f} {r[4]:7.1f} {r[5]:6.1f} {r[6]:6.1f} {r[7]:6.0f} {r[8]:7.1f}")
