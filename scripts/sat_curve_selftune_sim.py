"""Closed-loop simulation: heating-curve self-tune options for SAT (TASK-1519).

Controller = thermo-nova at the pinned commit 7beaaf5 (read 2026-10-31):
  heating_curve.py:28-39  curve = base + coeff/4 * (4*(T-20) + 0.03*(To-20)^2 - 0.4*(To-20)), base 27.2 (radiators)
  pid.py kp = coeff*curve/3 (radiators), ki = kp/8400, kd = 0.07*8400*kp
  pid.py _update_integral: |error| > DEADBAND(0.1) -> integral = 0, else += ki*error*60, clamped to +-curve
  output = curve + P + I + D, PID step every 60 s (entry 42).
Plant (our assumption, NOT measured): one thermal mass, radiator output K*(Tmean-Troom)^1.3,
losses H*(Troom-Tout), solar gain on sunny afternoons, boiler flow follows the setpoint with a
5-minute lag, flow capped at 70 C, no PWM / no minimum modulation (continuous only).
The 'true' coefficient is the one where the curve alone holds 20.0 C at -5 C outside.
"""
import math, random, statistics, sys

DT = 60.0                    # s, also the PID interval
BASE = 27.2
TARGET = 20.0

def curve_raw(t, to):
    return 4 * (t - 20) + 0.03 * (to - 20) ** 2 - 0.4 * (to - 20)

def curve(c, t, to):
    return round(BASE + (c / 4) * curve_raw(t, to), 1)

class House:
    C = 12e6        # J/K   (about a terraced house incl. contents; assumption)
    H = 180.0       # W/K   heat loss coefficient (assumption)
    def __init__(self, k_rad):
        self.k = k_rad; self.t = TARGET; self.flow = 30.0
    def step(self, setpoint, tout, solar_w, ch_ok):
        tgt = min(setpoint, 70.0) if ch_ok else self.t       # DHW draw: no CH heat
        self.flow += (tgt - self.flow) * DT / 300.0
        dtr = max(self.flow - 5.0 - self.t, 0.0)             # mean radiator temp ~ flow - 5
        q = self.k * dtr ** 1.3 if ch_ok else 0.0
        self.t += (q - self.H * (self.t - tout) + solar_w) * DT / self.C
        return q

def k_for_true(c_true):
    """Radiator constant so that the curve alone balances 20 C at -5 C outside."""
    tout = -5.0
    flow = curve(c_true, TARGET, tout)
    return House.H * (TARGET - tout) / ((flow - 5.0 - TARGET) ** 1.3)

class Pid:
    def __init__(self):
        self.i = 0.0; self.last_t = None; self.rd = 0.0
    def reset(self):
        self.i = 0.0; self.rd = 0.0
    def step(self, c, room, to):
        cv = curve(c, TARGET, to)
        kp = c * cv / 3; ki = kp / 8400; kd = 0.07 * 8400 * kp
        err = TARGET - room
        if self.last_t is not None:
            d = room - self.last_t
            self.rd = 0.0 if d == 0 else -d / DT
        self.last_t = room
        if abs(err) > 0.1:
            self.i = 0.0
        else:
            self.i = max(-cv, min(cv, self.i + ki * err * DT))
        p = kp * err
        return round(cv + p + self.i + kd * self.rd, 1), err, p

def weather(minute, day, cold, sunny_days):
    h = (minute / 60.0) % 24
    tout = cold + 4.0 * math.sin((h - 9) / 24 * 2 * math.pi)       # warmest ~15:00
    solar = 0.0
    if day in sunny_days and 10 <= h <= 16:
        solar = 1800.0 * math.sin((h - 10) / 6 * math.pi)            # W through the windows
    return tout, solar

# ---------------- tuning strategies (called once per minute) ----------------
class NoTune:
    name = "none"
    def __init__(self): self.writes = 0
    def tick(self, c, m, ctx): return c

class A_Integral:
    """Proposal A as sent: integral kept one sign for the whole hour -> 0.1 step."""
    name = "A integral sign"
    def __init__(self): self.writes = 0; self.signs = []; self.ok = True
    def tick(self, c, m, ctx):
        self.signs.append(0 if ctx["i"] == 0 else (1 if ctx["i"] > 0 else -1)); self.ok &= ctx["stable"]
        if m % 60 == 59:
            s = set(self.signs); stable = self.ok; self.signs = []; self.ok = True
            if stable and s in ({1}, {-1}):
                self.writes += 1; return round(min(5.0, max(0.1, c + 0.1 * s.pop())), 1)
        return c

class A2_MeanError:
    """Variant A2: hourly mean room error under stable conditions beyond +-0.2 C -> 0.1 step."""
    name = "A2 hourly mean error"
    def __init__(self): self.writes = 0; self.errs = []; self.ok = True
    def tick(self, c, m, ctx):
        self.errs.append(ctx["err"]); self.ok &= ctx["stable"]
        if m % 60 == 59:
            e = statistics.fmean(self.errs); stable = self.ok; self.errs = []; self.ok = True
            if stable and abs(e) > 0.2:
                self.writes += 1; return round(min(5.0, max(0.1, c + (0.1 if e > 0 else -0.1))), 1)
        return c

class A3_MeanCorrection:
    """Variant A3: hourly mean of the PID correction (setpoint minus curve, in C flow) under stable
    conditions; beyond +-1.5 C -> one 0.1 step, at most one step per 3 hours (anti-hunting)."""
    name = "A3 hourly PID correction"
    def __init__(self): self.writes = 0; self.corr = []; self.ok = True; self.cool = 0
    def tick(self, c, m, ctx):
        self.corr.append(ctx["corr"]); self.ok &= ctx["stable"]
        if m % 60 == 59:
            k = statistics.fmean(self.corr); stable = self.ok; self.corr = []; self.ok = True
            if self.cool > 0: self.cool -= 1; return c
            if stable and abs(k) > 1.5:
                self.writes += 1; self.cool = 2
                return round(min(5.0, max(0.1, c + (0.1 if k > 0 else -0.1))), 1)
        return c

class B_DailyHcr:
    """Option B: daily median of the room error; 3 days in a row beyond 0.5 C -> 0.1 step (firmware HCR rule)."""
    name = "B daily recommendation"
    def __init__(self): self.writes = 0; self.day = []; self.medians = []
    def tick(self, c, m, ctx):
        if ctx["heating"]: self.day.append(ctx["err"])
        if m % 1440 == 1439:
            if len(self.day) >= 2: self.medians.append(statistics.median(self.day))
            self.day = []
            last = self.medians[-3:]
            if len(last) == 3 and (all(x > 0.5 for x in last) or all(x < -0.5 for x in last)):
                self.writes += 1; self.medians = []
                return round(min(5.0, max(0.1, c + (0.1 if last[0] > 0 else -0.1))), 1)
        return c

def run(strategy, c_true, c0, days=21, cold=2.0, sunny=(), dhw=True, seed=1):
    random.seed(seed)
    house = House(k_for_true(c_true)); pid = Pid(); c = c0
    sp = curve(c, TARGET, cold); errs_late = []; t_conv = None; trace = []
    for m in range(days * 1440):
        day = m // 1440; h = (m / 60) % 24
        tout, solar = weather(m, day, cold, sunny)
        draw = dhw and ((7 <= h < 7.17) or (19 <= h < 19.17))     # two 10-minute hot-water draws a day
        sp, err, p = pid.step(c, house.t, tout)
        house.step(sp, tout, solar, ch_ok=not draw)
        stable = (not draw) and solar == 0 and tout < 15
        new_c = strategy.tick(c, m, {"i": pid.i, "err": err, "p": p, "corr": sp - curve(c, TARGET, tout), "stable": stable, "heating": tout < 15})
        if new_c != c:
            c = new_c; pid.reset()
        if t_conv is None and abs(c - c_true) <= 0.1 and strategy.name != "none":
            t_conv = m / 1440
        if day >= days - 7: errs_late.append(err)
        if m % 60 == 0: trace.append((round(m / 1440, 3), c, round(house.t, 2)))
    rms = math.sqrt(statistics.fmean(e * e for e in errs_late))
    return {"strategy": strategy.name, "c_end": c, "days_to_within_0.1": t_conv, "rms_last7d": round(rms, 3),
            "max_abs_err_last7d": round(max(abs(e) for e in errs_late), 2), "flash_writes": strategy.writes, "trace": trace}

if __name__ == "__main__":
    import json
    C_TRUE = 1.6
    scenarios = [("curve too low (0.8)", 0.8, ()), ("curve too high (2.6)", 2.6, ()),
                 ("correct curve (1.6), sunny days", 1.6, (3, 4, 5, 10, 11, 12, 17, 18))]
    out = []
    for label, c0, sunny in scenarios:
        for S in (NoTune, A_Integral, A2_MeanError, A3_MeanCorrection, B_DailyHcr):
            r = run(S(), C_TRUE, c0, sunny=sunny)
            r["scenario"] = label; out.append(r)
            print(f"{label:34s} {r['strategy']:24s} c_end={r['c_end']:<4} conv_days={r['days_to_within_0.1']!s:6s} "
                  f"rms7d={r['rms_last7d']:<6} max7d={r['max_abs_err_last7d']:<5} writes={r['flash_writes']}")
    json.dump(out, open(sys.argv[1] if len(sys.argv) > 1 else "results.json", "w"))
