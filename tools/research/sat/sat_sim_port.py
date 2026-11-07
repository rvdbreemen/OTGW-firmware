"""Python port of the SAT decision logic used by the heat-pump simulation library (TASK-1527, round 21).

Each function mirrors one firmware function; tests/test_sat_sim_port_equivalence.py drives the same input sequences
through this port and through the compiled firmware functions and fails on any difference. Keep the two in step:
when the firmware changes, that test goes red until the port follows.

Ported (firmware source):
- satCalcHeatingCurve (SATcontrol.ino)                      -> curve()
- SATpid.ino: satPidError, satPidRound3, _pidCalculateGains, _pidUpdateIntegral, _pidUpdateDerivative,
  satPidNoteRoom, satPidStep, satPidOutput, satPidService    -> class Pid
- satApplyPWM's 5-range mapper (SATcontrol.ino), periods and minimum ON for heat pumps (entries 197/198)
                                                             -> pwm_times()
- satChEnableDecide (SATchEnable.h)                          -> ch_enable()
Not ported here (the simulation models them in its own loop, see sat_hp_library_study21.py): the PWM entry switch and
exits (SATcycles.ino), the ON-phase setpoint overrides, the room sensor filter, presets, solar gain.

Floats: the firmware computes in float32; this port uses numpy float32 for the rounding steps so that the 3-decimal
roundings land on the same side.
"""
import math
import numpy as np

f32 = np.float32
HC_REF = f32(20.0)
DEADBAND = f32(0.1)
PID_INTERVAL = f32(60.0)
DERIV_CAP = f32(5.0)
AGGRESSION = f32(8400.0)


def round3(v):
    """satPidRound3: roundf(v * 1000) / 1000 in float32; roundf rounds half away from zero."""
    x = float(f32(v) * f32(1000.0))
    r = math.copysign(math.floor(abs(x) + 0.5), x)
    return f32(f32(r) / f32(1000.0))


def pid_error(target, room):
    return round3(f32(target) - f32(room))


def curve(coeff, target, outside, base_offset):
    """satCalcHeatingCurve: base + coeff/4 * (4*(target-20) + 0.03*(out-20)^2 - 0.4*(out-20)), rounded to 0.1 (entry 257)."""
    diff = f32(outside) - HC_REF
    cv = f32(4.0) * (f32(target) - HC_REF) + f32(0.03) * diff * diff - f32(0.4) * diff
    v = f32(base_offset) + (f32(coeff) / f32(4.0)) * cv
    x = float(v * f32(10.0))   # roundf(v * 10) / 10, half away from zero (entry 257, TASK-1360)
    return f32(math.copysign(math.floor(abs(x) + 0.5), x)) / f32(10.0)


class Pid:
    """SATpid.ino state machine, driven like satControlLoopRun(): service(t) every pass, note_room + output every tick."""

    def __init__(self, coeff, underfloor):
        self.coeff = f32(coeff); self.divisor = f32(4.0) if underfloor else f32(3.0)
        self.reset(0)
        self.room_seen_val = float("nan"); self.room_changed_ms = 0
        self.timer_next_ms = None

    def reset(self, now_ms):
        self.last_error = f32(0.0); self.integral = f32(0.0); self.raw_d = f32(0.0)
        self.last_room = f32(0.0); self.last_deriv_ms = now_ms; self.initialized = False
        self.startup_pending = True; self.first_output_pending = True; self.room_seen = False
        self.kp = self.ki = self.kd = f32(0.0)

    def _gains(self, cv):
        self.kp = round3(self.coeff * f32(cv) / self.divisor)
        self.ki = round3(self.kp / AGGRESSION)
        self.kd = round3(f32(0.063) * AGGRESSION * self.kp)   # SAT_PID_KD_FACTOR, entry 279 (pid.py:93 has 0.07)

    def note_room(self, room, now_ms):
        room = f32(room)
        same = (not math.isnan(self.room_seen_val)) and abs(room - f32(self.room_seen_val)) < f32(0.001)
        if not same:   # firmware: !(fabsf(room - seen) < 0.001), true while seen is NAN
            self.room_seen_val = float(room); self.room_changed_ms = now_ms

    def _integral(self, error, cv):
        if abs(error) > DEADBAND:
            self.integral = f32(0.0); return
        self.integral = f32(self.integral + self.ki * error * PID_INTERVAL)
        if self.integral < -f32(cv): self.integral = -f32(cv)
        if self.integral > f32(cv): self.integral = f32(cv)

    def _derivative(self, room, error):
        changed = self.room_changed_ms
        if abs(error) <= DEADBAND:
            self.last_deriv_ms = changed; return
        delta = f32(room) - self.last_room
        if delta == f32(0.0):
            self.last_deriv_ms = changed; return
        dt = f32((changed - self.last_deriv_ms) & 0xFFFFFFFF) / f32(1000.0)
        if dt <= PID_INTERVAL: return
        raw = -delta / dt
        if abs(raw) >= DERIV_CAP:
            self.raw_d = f32(max(-DERIV_CAP, min(DERIV_CAP, raw))); self.last_deriv_ms = changed; return
        alpha = dt / (PID_INTERVAL + dt)
        filt = alpha * raw + (f32(1.0) - alpha) * self.raw_d
        self.raw_d = f32(max(-DERIV_CAP, min(DERIV_CAP, filt))); self.last_deriv_ms = changed

    def step(self, room, target, cv, now_ms):
        error = pid_error(target, room)
        self.note_room(room, now_ms); self.room_seen = True
        if not self.initialized:
            self.last_error = error; self.last_room = f32(room); self.last_deriv_ms = self.room_changed_ms; self.initialized = True
        self._gains(cv); self._integral(error, cv); self._derivative(room, error)
        self.last_error = error; self.last_room = f32(room)

    def output(self, cv):
        self._gains(cv)
        P = I = D = f32(0.0)
        if not math.isnan(float(self.last_error)):
            P = round3(self.kp * self.last_error); I = round3(self.integral)
            if not self.first_output_pending: D = round3(self.kd * self.raw_d)
            self.first_output_pending = False
        return round3(f32(cv) + P + I + D)

    def service(self, now_ms, room, target, cv):
        """satPidService with a valid room temperature: a step at start-up, then every 60 s (SKIP_MISSED_TICKS)."""
        startup = self.startup_pending
        due = startup or (self.timer_next_ms is not None and now_ms >= self.timer_next_ms)
        if not due and self.room_seen: return
        if startup:
            self.startup_pending = False; self.timer_next_ms = now_ms + 60000
        elif due:
            while self.timer_next_ms <= now_ms: self.timer_next_ms += 60000
        self.step(room, target, cv, now_ms)


def pwm_times(duty, period_s, min_on_s, flame_on):
    """satApplyPWM 5-range mapper: returns (on_s, off_s, continuous)."""
    upper = float(period_s); mn = float(min_on_s); mx = 2.0 * upper
    dl = f32(mn * 1000.0) / f32(upper * 1000.0); du = f32(1.0) - dl; dmin = dl / f32(2.0); dmax = f32(1.0) - dmin   # float32 as the firmware
    duty = f32(duty)
    if duty >= dmax: return mx, 0.0, True
    if duty < dmin: return (mn, mx - mn, False) if flame_on else (0.0, mx, False)
    if duty <= dl: return mn, min(mn * (1.0 - duty) / duty, mx - mn), False
    if duty <= du:
        on = duty * upper; return max(on, mn), upper - on, False
    on = mn / (1.0 - duty) - mn
    return max(on, mn), mn, False


def ch_enable(requested, cold, pwm_drives, pwm_on):
    """satChEnableDecide."""
    if requested <= cold: return 0
    if pwm_drives and not pwm_on: return 0
    return 1
