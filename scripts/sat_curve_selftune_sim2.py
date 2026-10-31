"""TASK-1519 round 2: hourly decisions with guards (Robert) vs daily decisions (Sergeant D, entries 162/164),
both on the separate curve integral (entry 163: setpoint minus curve), with household noise.

Reuses the plant and the thermo-nova curve/PID of sat_curve_selftune_sim.py (same assumptions and limits).
Noise model (assumption): daily cooking 17:30-18:30 +1200 W; 1-3 random activity bursts per day
(300-900 W, 30-120 min, 08-23 h); a party on day 9, 19-24 h, +1500 W. Sun on sunny days 10-16 h
(an hour with sun does not count, entry 160 'sun below 12 degrees' approximated as 'no solar gain').
Metrics: room error RMS over the last 7 days, coefficient writes, and wrong-way steps (a step that moves the
coefficient away from the best coefficient for that day's mean outside temperature).
Run: python scripts/sat_curve_selftune_sim2.py
"""
import math, random, statistics, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sat_curve_selftune_sim import House, Pid, curve, k_for_true, TARGET, DT

def best_coeff(k, tout):
    need = (House.H * (TARGET - tout) / k) ** (1 / 1.3) + 5 + TARGET
    return min((abs(curve(c / 10, TARGET, tout) - need), c / 10) for c in range(1, 51))[1]

class Hourly:
    """Robert: decide every hour. Guard: 3 clean hours in a row with mean correction beyond 1.5 C, same sign;
    step 0.1; at most 0.2 total change per calendar day (entry 165 as a daily cap)."""
    name = "H hourly + guards"
    def __init__(self): self.run = []; self.day_delta = 0.0; self.day = -1; self.writes = 0
    def hour(self, c, day, mean_corr, clean):
        if day != self.day: self.day, self.day_delta = day, 0.0
        if not clean or abs(mean_corr) <= 1.5: self.run = []; return c
        s = 1 if mean_corr > 0 else -1
        self.run = [x for x in self.run if x == s] + [s]
        if len(self.run) >= 3 and self.day_delta + 0.1 <= 0.2 + 1e-9:
            self.run = []; self.day_delta += 0.1; self.writes += 1
            return round(min(5.0, max(0.1, c + 0.1 * s)), 1)
        return c
    def end_of_day(self, c, day): return c

class Daily:
    """Sergeant D: decide once a day on the clean hours (>= 6); mean correction beyond 1.5 C -> 0.1 step;
    if the day after a step is worse (|mean| grew), the step is undone (entry 164 'compare previous to new')."""
    name = "D daily + compare"
    def __init__(self): self.hours = []; self.writes = 0; self.prev = None; self.last_step = 0.0
    def hour(self, c, day, mean_corr, clean):
        if clean: self.hours.append(mean_corr)
        return c
    def end_of_day(self, c, day):
        hrs, self.hours = self.hours, []
        if len(hrs) < 6: return c
        m = statistics.fmean(hrs)
        if self.last_step and self.prev is not None and abs(m) > abs(self.prev):
            c = round(c - self.last_step, 1); self.writes += 1; self.last_step = 0.0; self.prev = m; return c
        self.prev = m; self.last_step = 0.0
        if abs(m) > 1.5:
            self.last_step = 0.1 if m > 0 else -0.1; self.writes += 1
            return round(min(5.0, max(0.1, c + self.last_step)), 1)
        return c

class NoTune:
    name = "none"; writes = 0
    def hour(self, c, *a): return c
    def end_of_day(self, c, day): return c

def run(strategy, c0, c_true=1.6, days=28, cold=2.0, sunny=(), seed=1):
    rnd = random.Random(seed)
    k = k_for_true(c_true); house = House(k); pid = Pid(); c = c0
    events = []                                           # (start_min, end_min, watts)
    for d in range(days):
        events.append((d * 1440 + 17.5 * 60, d * 1440 + 18.5 * 60, 1200.0))
        for _ in range(rnd.randint(1, 3)):
            st = d * 1440 + rnd.uniform(8, 22) * 60
            events.append((st, st + rnd.uniform(30, 120), rnd.uniform(300, 900)))
    events.append((9 * 1440 + 19 * 60, 9 * 1440 + 24 * 60, 1500.0))
    errs, wrong, hour_corr, hour_clean, day_touts = [], 0, [], True, []
    for m in range(days * 1440):
        day, h = m // 1440, (m / 60) % 24
        tout = cold + 4.0 * math.sin((h - 9) / 24 * 2 * math.pi)
        solar = 1800.0 * math.sin((h - 10) / 6 * math.pi) if (day in sunny and 10 <= h <= 16) else 0.0
        gains = sum(w for s, e, w in events if s <= m < e)
        draw = (7 <= h < 7.17) or (19 <= h < 19.17)
        sp, err, _ = pid.step(c, house.t, tout)
        house.step(sp, tout, solar + gains, ch_ok=not draw)
        hour_corr.append(sp - curve(c, TARGET, tout)); hour_clean &= (not draw) and solar == 0
        day_touts.append(tout)
        if day >= days - 7: errs.append(err)
        if m % 60 == 59:
            new_c = strategy.hour(c, day, statistics.fmean(hour_corr), hour_clean)
            hour_corr, hour_clean = [], True
            if new_c != c:
                b = best_coeff(k, statistics.fmean(day_touts[-1440:]))
                if abs(new_c - b) > abs(c - b) + 1e-9: wrong += 1
                c = new_c; pid.reset()
        if m % 1440 == 1439:
            new_c = strategy.end_of_day(c, day)
            if new_c != c:
                b = best_coeff(k, statistics.fmean(day_touts[-1440:]))
                if abs(new_c - b) > abs(c - b) + 1e-9: wrong += 1
                c = new_c; pid.reset()
            day_touts = []
    return {"c_end": c, "rms": math.sqrt(statistics.fmean(e * e for e in errs)), "writes": strategy.writes, "wrong": wrong}

if __name__ == "__main__":
    scen = [("curve too low (0.8)", 0.8, ()), ("curve too high (2.6)", 2.6, ()),
            ("correct curve (1.6), sunny days", 1.6, (3, 4, 5, 10, 11, 12, 17, 18, 24, 25))]
    print("best coefficient at the +2 C operating point:", best_coeff(k_for_true(1.6), 2.0))
    for label, c0, sunny in scen:
        for S in (NoTune, Hourly, Daily):
            rs = [run(S(), c0, sunny=sunny, seed=s) for s in (1, 2, 3)]
            print(f"{label:34s} {S.name:20s} c_end={[r['c_end'] for r in rs]} "
                  f"rms7d={statistics.fmean(r['rms'] for r in rs):.3f} writes={statistics.fmean(r['writes'] for r in rs):.1f} "
                  f"wrong-way={statistics.fmean(r['wrong'] for r in rs):.1f}")
