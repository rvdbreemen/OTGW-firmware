#!/usr/bin/env python3
"""TASK-1412.10: SAT PID start-up, held derivative, saturation, deadband, Ki = 0 and persistence against pid.py.

Verification task. The code under test is the REAL SATpid.ino (satPidReset / satPidRestoreState / satPidRestoreLast /
satPidGetPersist* / satPidNoteRoom / satPidStep / satPidOutput) compiled on the host with the real safeTimers.h
(build of tests/test_sat_pid_cadence.py). The C driver prints, per PID step, the HELD integral, raw derivative, error,
the room-changed stamp and the gains; per output call P, I, D, the output and the gains.

The ORACLE is thermo-nova's PID, transcribed line for line below from custom_components/sat/pid.py at the pin 7beaaf5
(update :159-171, _update_integral :185-197, _update_derivative :199-232, clamp_to_range helpers.py:148-150, outputs
:101-132). It is the reference, not a copy of the firmware. It is fed what the firmware saw (the room value, the
room-changed stamp). Recorded, deliberate deviations from pid.py are modelled as parameters of the oracle, each with
its entry: gains and every term rounded to 3 decimals instead of 6 / 1 (entry 95, Q28a), and D = 0 on the first output
after a boot, reset or restore (entry 95, Q39b). Nothing else differs; any other difference is a test failure.

Room values are synthetic (no field recording); float32 on the firmware side, so the oracle is fed the float32 value.

Run: python tests/test_sat_pid_semantics.py
"""
import math
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pid_cadence as cad  # noqa: E402

MAIN = r"""
static void printStep(const char* tag) {
  printf("%s I=%.9g rd=%.9g err=%.9g changed=%u last=%.9g ki=%.9g kp=%.9g kd=%.9g\n", tag, _pid_integral, _pid_rawDerivative,
         state.sat.fError, (unsigned)_pid_roomTempChangedMs, _pid_lastRoomTemp, state.sat.fKi, state.sat.fKp, state.sat.fKd);
}
int main() {
  char c[8];
  while (scanf("%7s", c) == 1) {
    if (!strcmp(c, "R")) { satPidReset(); continue; }
    if (!strcmp(c, "RS")) { float i, d; scanf("%f %f", &i, &d); satPidRestoreState(i, d); continue; }
    if (!strcmp(c, "RL")) { int eh, th; float e, t; unsigned ms; scanf("%d %f %d %f %u", &eh, &e, &th, &t, &ms); satPidRestoreLast(eh, e, th, t, ms); continue; }
    if (!strcmp(c, "SOLAR")) { int on; scanf("%d", &on); state.sat.bSolarGainActive = on; settings.sat.bSolarFreezeIntegral = on; continue; }
    if (!strcmp(c, "S")) { float room, target, curve; unsigned ms; scanf("%f %f %f %u", &room, &target, &curve, &ms);
      g_ms = ms; satPidNoteRoom(room, ms); satPidStep(room, target, curve, ms); printStep("S"); continue; }
    if (!strcmp(c, "O")) { float curve; scanf("%f", &curve); float out = satPidOutput(curve);
      printf("O out=%.9g P=%.9g I=%.9g D=%.9g kp=%.9g kd=%.9g\n", out, state.sat.fPidP, state.sat.fPidI, state.sat.fPidD, state.sat.fKp, state.sat.fKd); continue; }
    if (!strcmp(c, "G")) { float i, d, rd, le; bool eh, th; float lt; uint32_t ms;
      satPidGetPersistState(&i, &d, &rd, &le); satPidGetPersistLast(&eh, &th, &lt, &ms);
      printf("G I=%.9g rd=%.9g lastErr=%.9g errHeld=%d tempHeld=%d lastTemp=%.9g ms=%u\n", i, rd, le, (int)eh, (int)th, lt, (unsigned)ms); continue; }
  }
  return 0;
}
"""


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def r3(x):
    return math.floor(abs(x) * 1000.0 + 0.5) / 1000.0 * (1 if x >= 0 else -1)   # roundf: half away from zero


# ---- thermo-nova pid.py at 7beaaf5, transcribed ----------------------------------------------------------------------
DEADBAND = 0.1                 # const.py:16
PID_UPDATE_INTERVAL = 60       # pid.py:20
DERIVATIVE_RAW_CAP = 5.0       # pid.py:19


def clamp_to_range(value, limit):          # helpers.py:148-150
    return max(-limit, min(value, limit))


class PidPy:
    def __init__(self, coeff=1.6, divisor=3):
        self.coeff, self.divisor = coeff, divisor
        self.integral = 0.0
        self.raw_derivative = 0.0
        self.last_error = None
        self.last_temperature = None
        self.last_derivative_updated = None

    # gains: pid.py:61-93 rounds to 6; the firmware rounds to 3 (entry 95, Q28a), so the oracle takes 3
    def kp(self, curve): return r3(self.coeff * curve / self.divisor)
    def ki(self, curve): return r3(self.kp(curve) / 8400)
    def kd(self, curve): return r3(0.07 * 8400 * self.kp(curve))

    def update(self, current, error, last_changed, curve, freeze_integral=False):      # pid.py:159-171
        self._update_derivative(current, error, last_changed)
        if not freeze_integral:
            self._update_integral(error, curve)
        self.last_error = error
        self.last_temperature = current

    def _update_integral(self, error, curve):                                          # pid.py:185-192
        if abs(error) > DEADBAND:
            self.integral = 0.0
            return
        self.integral += self.ki(curve) * error * PID_UPDATE_INTERVAL
        self.integral = clamp_to_range(self.integral, curve)

    def _update_derivative(self, current, error, last_changed):                        # pid.py:199-232
        if self.last_temperature is None or self.last_derivative_updated is None:
            self.last_derivative_updated = last_changed
            return
        if abs(error) <= DEADBAND:
            self.last_derivative_updated = last_changed
            return
        delta = current - self.last_temperature
        if delta == 0.0:
            self.last_derivative_updated = last_changed
            return
        delta_time = last_changed - self.last_derivative_updated
        if delta_time <= PID_UPDATE_INTERVAL:
            return
        derivative = -delta / delta_time
        if abs(derivative) >= DERIVATIVE_RAW_CAP:
            self.raw_derivative = max(-DERIVATIVE_RAW_CAP, min(derivative, DERIVATIVE_RAW_CAP))
            self.last_derivative_updated = last_changed
            return
        alpha = delta_time / (PID_UPDATE_INTERVAL + delta_time)
        filtered = alpha * derivative + (1 - alpha) * self.raw_derivative
        self.raw_derivative = clamp_to_range(filtered, DERIVATIVE_RAW_CAP)
        self.last_derivative_updated = last_changed

    def terms(self, curve, d_zero=False):                                              # pid.py:101-132, 3 decimals (entry 95)
        p = r3(self.kp(curve) * self.last_error)
        i = r3(self.integral)
        d = 0.0 if d_zero else r3(self.kd(curve) * self.raw_derivative)
        return p, i, d, r3(curve + p + i + d)


def num(tok):
    return float(tok.split("=")[1])


def parse(line):
    head, *rest = line.split()
    return head, {kv.split("=")[0]: float(kv.split("=")[1]) for kv in rest}


class Base(unittest.TestCase):
    _shared = {}

    @classmethod
    def setUpClass(cls):
        if "exe" in Base._shared:
            cls.exe = Base._shared["exe"]
            return
        cls.dir = tempfile.TemporaryDirectory()
        Base._shared["dir"] = cls.dir
        d = Path(cls.dir.name)
        sat = cad.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = cad.PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = pid[pid.index("// --- PID Constants ---"):]
        text = cad.adapt(sat, cad.PRELUDE.replace("@TIMERS@", cad.TIMERS).replace("@ZONE@", cad.zone_source(sat))
                         .replace("@PID@", pid).replace("@GATHER@", cad.GATHER_STUB)
                         .replace("@SAVE@", cad.save_source(sat)) + MAIN)
        cpp = d / "sem.cpp"
        cpp.write_text(text, encoding="utf-8")
        cls.exe = Base._shared["exe"] = cad.compile_cpp(cpp, d / "sem.exe")

    def run_script(self, lines):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        return [parse(l) for l in out.splitlines()]

    def follow(self, script, curve_of=lambda s: 40.0, oracle=None, start_from_restore=None):
        """Run a script of 'S room target curve ms' lines (others pass through) and compare every step with pid.py."""
        rows = self.run_script(script)
        pid = oracle or PidPy()
        steps = [l.split() for l in script if l.startswith("S ")]
        got = [r for r in rows if r[0] == "S"]
        self.assertEqual(len(steps), len(got))
        for (s, room, target, curve, ms), (_, g) in zip(steps, got):
            current = f32(float(room))
            error = r3(float(target) - current)               # TemperatureState.error is rounded (temperature/state.py)
            pid.update(current, error, g["changed"] / 1000.0, float(curve))
            self.assertAlmostEqual(g["I"], pid.integral, delta=max(1e-4, 1e-3 * abs(pid.integral)), msg="integral at %s" % ms)
            self.assertAlmostEqual(g["rd"], pid.raw_derivative, delta=max(1e-7, 1e-3 * abs(pid.raw_derivative)), msg="raw derivative at %s" % ms)
            self.assertAlmostEqual(g["err"], error, places=5)
        return rows, pid


class StartUp(Base):
    """pid.py:201-203 (no history: the derivative only stamps its time) and :185-192 (the integral still runs)."""

    def test_first_update_inside_deadband_has_no_derivative_and_one_integral_step(self):
        rows, pid = self.follow(["R", "S 20.95 21 40 1000", "O 40"])
        step = rows[0][1]
        self.assertEqual(step["rd"], 0.0)
        self.assertAlmostEqual(step["I"], 0.003 * 0.05 * 60, places=5)        # ki(curve 40) = round3(21.333/8400) = 0.003
        self.assertAlmostEqual(step["I"], pid.integral, places=5)

    def test_first_update_outside_deadband_zeroes_the_integral_and_has_no_derivative(self):
        rows, pid = self.follow(["R", "S 19 21 40 1000", "O 40"])
        step = rows[0][1]
        self.assertEqual((step["I"], step["rd"]), (0.0, 0.0))
        out = rows[1][1]
        p, i, d, total = pid.terms(40.0)
        self.assertAlmostEqual(out["P"], p, places=3)
        self.assertAlmostEqual(out["out"], total, places=3)

    def test_first_output_after_a_restore_has_d_zero_then_d(self):
        # Entry 95 (Q39b): "D = 0" on the first output after boot/reset/restore. pid.py:109-114 has no such rule
        # (recorded deviation); the second output carries kd * restored raw derivative as pid.py does.
        rows = self.run_script(["R", "RS 0.5 0.02", "S 20.95 21 40 1000", "O 40", "O 40"])
        first, second = rows[1][1], rows[2][1]
        self.assertEqual(first["D"], 0.0)
        self.assertAlmostEqual(second["D"], r3(first["kd"] * 0.02), places=3)
        self.assertGreater(second["D"], 100.0, "premise: the restored raw derivative gives a large D")
        self.assertAlmostEqual(rows[0][1]["rd"], 0.02, places=6, msg="the start-up step must not drop the restored raw derivative")

    def test_restored_integral_survives_the_startup_step_inside_the_deadband_and_is_reset_outside(self):
        inside = self.run_script(["R", "RS 0.5 0", "S 20.95 21 40 1000"])[0][1]
        self.assertAlmostEqual(inside["I"], 0.5 + 0.003 * 0.05 * 60, places=5)     # pid.py:191 adds to the restored value
        outside = self.run_script(["R", "RS 0.5 0.02", "S 19 21 40 1000"])[0][1]
        self.assertEqual(outside["I"], 0.0)                                         # pid.py:187-189
        self.assertAlmostEqual(outside["rd"], 0.02, places=6)                       # the derivative step is skipped, raw value held


class HeldDerivative(Base):
    """The raw derivative is held between updates and inside the deadband (pid.py:205-207); D = kd(curve) * held value."""

    def test_output_between_steps_holds_error_integral_and_raw_derivative_and_d_follows_the_curve(self):
        script = ["R", "S 19 22 40 0", "S 19.5 22 40 120000", "O 40", "O 30", "O 40", "O 50"]
        rows, pid = self.follow(script)
        self.assertLess(rows[1][1]["rd"], 0.0)
        self.assertNotEqual(rows[1][1]["rd"], 0.0)
        outs = [r[1] for r in rows if r[0] == "O"][1:]   # the first output after a reset has D = 0 (entry 95, Q39b)
        for out, curve in zip(outs, (30.0, 40.0, 50.0)):
            p, i, d, total = pid.terms(curve)
            self.assertAlmostEqual(out["P"], p, delta=2e-3)
            self.assertAlmostEqual(out["I"], i, delta=2e-3)
            self.assertAlmostEqual(out["D"], d, delta=max(2e-3, 2e-3 * abs(d)))
            self.assertAlmostEqual(out["out"], total, delta=max(5e-3, 2e-3 * abs(total)))
        self.assertEqual(len({round(o["D"], 3) for o in outs}), 3, "D must follow kd(curve), not stay a frozen number (entries 124, 136)")

    def test_inside_the_deadband_the_raw_derivative_is_frozen_while_the_integral_runs(self):
        script = ["R", "S 19 22 40 0", "S 19.5 22 40 120000"]
        t = 120000
        for k in range(12):                                   # a drifting room inside the deadband (error 0.04 .. 0.09)
            t += 120000
            script.append("S %.3f 22 40 %d" % (21.91 + 0.005 * k, t))
        rows, pid = self.follow(script)
        steps = [r[1] for r in rows if r[0] == "S"]
        frozen = steps[1]["rd"]
        self.assertNotEqual(frozen, 0.0)
        self.assertEqual({s["rd"] for s in steps[2:]}, {frozen})
        self.assertGreater(steps[-1]["I"], steps[2]["I"], "the integral keeps accumulating inside the deadband")

    def test_change_within_60_s_is_skipped_but_moves_the_reference_and_exactly_60_s_is_skipped(self):
        # pid.py:215-218 (delta_time <= 60 skips), :171 (the previous temperature is overwritten on every update); entry 77
        script = ["R", "S 19 22 40 0", "S 19.5 22 40 60000", "S 19.6 22 40 121000"]
        rows, pid = self.follow(script)
        steps = [r[1] for r in rows if r[0] == "S"]
        self.assertEqual(steps[1]["rd"], 0.0, "60 s exactly is skipped")
        self.assertNotEqual(steps[2]["rd"], 0.0)
        self.assertAlmostEqual(steps[2]["rd"], -(19.6 - 19.5) / 121 * (121 / 181), delta=1e-5)   # delta from the PREVIOUS step, dt from the timer

    def test_61_s_moves_the_derivative(self):
        rows, _ = self.follow(["R", "S 19 22 40 0", "S 19.5 22 40 61000"])
        self.assertNotEqual(rows[1][1]["rd"], 0.0)

    def test_unchanged_temperature_leaves_the_derivative_alone(self):
        rows, _ = self.follow(["R", "S 19 22 40 0", "S 19.5 22 40 120000", "S 19.5 22 40 240000", "S 19.5 22 40 360000"])
        steps = [r[1] for r in rows if r[0] == "S"]
        self.assertEqual({steps[1]["rd"], steps[2]["rd"], steps[3]["rd"]}, {steps[1]["rd"]})


class Saturation(Base):
    """clamp_to_range (helpers.py:148-150) at +-curve for the integral (pid.py:192) and +-5 for the derivative (:222-230)."""

    def test_integral_saturates_at_plus_and_minus_curve_and_is_reset_outside_the_deadband(self):
        script = ["R"]
        t = 0
        for room in ("20.9", "21.1"):                          # error +0.1 then -0.1: exactly on the deadband edge, inside
            for _ in range(5000):
                script.append("S %s 21 40 %d" % (room, t))
                t += 60000
            script.append("MARK")
        script.append("S 19 21 40 %d" % t)                     # outside: reset
        out = subprocess.run([str(self.exe)], input="\n".join(l for l in script if l != "MARK") + "\n", capture_output=True, text=True, check=True).stdout
        steps = [parse(l)[1] for l in out.splitlines()]
        self.assertEqual(len(steps), 10001)
        self.assertAlmostEqual(steps[4999]["I"], 40.0, places=3)                 # +curve
        self.assertAlmostEqual(steps[4999]["I"], 40.0, places=3)
        self.assertAlmostEqual(max(s["I"] for s in steps), 40.0, places=3, msg="never above +curve")
        self.assertAlmostEqual(steps[9999]["I"], -40.0, places=3)                # -curve
        self.assertAlmostEqual(min(s["I"] for s in steps), -40.0, places=3, msg="never below -curve")
        self.assertEqual(steps[10000]["I"], 0.0)
        # the same series through the oracle
        pid = PidPy()
        for line in script[1:]:
            if not line.startswith("S "):
                continue
            _, room, target, curve, ms = line.split()
            cur = f32(float(room))
            pid.update(cur, r3(float(target) - cur), int(ms) / 1000.0, float(curve))
        self.assertAlmostEqual(steps[-1]["I"], pid.integral, places=3)

    def test_integral_clamp_follows_a_falling_curve(self):
        # the clamp uses the CURRENT curve (pid.py:192): a restored 30 with a curve of 20 becomes 20 on the next inside-deadband step
        rows = self.run_script(["R", "RS 30 0", "S 20.95 21 20 0"])
        self.assertAlmostEqual(rows[0][1]["I"], 20.0, places=3)

    def test_raw_derivative_cap_is_5_without_filter_and_restored_overshoot_is_clamped(self):
        rows, pid = self.follow(["R", "S 20 22 40 0", "S 420 22 40 61000"])         # 400 C in 61 s: -6.56 /s, capped
        self.assertEqual(rows[1][1]["rd"], -5.0)
        restored = PidPy()
        restored.raw_derivative = 12.0
        rows2, pid2 = self.follow(["R", "RS 0 12", "S 19 22 40 0", "S 19.5 22 40 61000"], oracle=restored)   # filtered 5.95 -> clamp
        self.assertEqual(rows2[1][1]["rd"], 5.0)

    def test_output_is_not_clamped_by_the_pid(self):
        # pid.py:126-132 has no output limit; the setpoint limits live in the control layer, not in satPidOutput()
        rows = self.run_script(["R", "S 0 21 40 0", "O 40"])
        out = rows[1][1]
        self.assertGreater(out["out"], 400.0)
        self.assertAlmostEqual(out["out"], r3(40 + r3(r3(1.6 * 40 / 3) * 21.0)), delta=2e-3)

    def test_solar_freeze_skips_the_whole_integral_update_including_the_reset(self):
        # climate.py:499-502 -> pid.py:167-168: freeze_integral skips _update_integral, the derivative still runs
        rows = self.run_script(["R", "RS 5 0", "SOLAR 1", "S 19 21 40 0", "S 19.5 21 40 120000"])
        steps = [r[1] for r in rows if r[0] == "S"]
        self.assertEqual({s["I"] for s in steps}, {5.0})
        self.assertNotEqual(steps[1]["rd"], 0.0)


class Deadband(Base):
    """abs(error) > 0.1 resets the integral (pid.py:187), abs(error) <= 0.1 freezes the derivative (:205); error rounded to 3."""

    def test_boundary_values(self):
        cases = [("20.9", True), ("21.1", True), ("20.899", False), ("21.101", False), ("21.0", True)]
        for room, inside in cases:
            with self.subTest(room=room):
                rows = self.run_script(["R", "RS 1 0", "S %s 21 40 0" % room])
                i = rows[0][1]["I"]
                if inside:
                    self.assertAlmostEqual(i, 1.0 + 0.003 * r3(21.0 - f32(float(room))) * 60, places=5)   # pid.py:191
                else:
                    self.assertEqual(i, 0.0)                                                              # pid.py:187-189

    def test_float_error_of_exactly_0_1_is_inside(self):
        # 21.0f - 20.9f = 0.1000004 in float32; satPidError rounds to 3 decimals as TemperatureState.error does
        rows = self.run_script(["R", "S 20.9 21 40 0"])
        self.assertAlmostEqual(rows[0][1]["err"], 0.1, places=6)
        rows = self.run_script(["R", "RS 1 0", "S 20.9 21 40 0"])
        self.assertAlmostEqual(rows[0][1]["I"], 1.0 + 0.003 * 0.1 * 60, places=5)

    def test_derivative_boundary(self):
        # error 0.1 (inside) freezes, error 0.101 (outside) moves, same temperature step
        inside = self.run_script(["R", "S 21.5 21 40 0", "S 20.9 21 40 120000"])[1][1]
        outside = self.run_script(["R", "S 21.5 21 40 0", "S 20.899 21 40 120000"])[1][1]
        self.assertEqual(inside["rd"], 0.0)
        self.assertNotEqual(outside["rd"], 0.0)


class KiZero(Base):
    """A curve so low that Ki rounds to 0: the integral adds nothing, the clamp still applies (entry 95 Q28b; pid.py:191-192)."""

    def test_ki_zero_keeps_a_value_inside_the_clamp_and_clamps_one_outside(self):
        rows = self.run_script(["R", "RS 3 0", "S 20.95 21 5 0"])
        self.assertEqual(rows[0][1]["ki"], 0.0)
        self.assertAlmostEqual(rows[0][1]["I"], 3.0, places=6)
        rows = self.run_script(["R", "RS 50 0", "S 20.95 21 5 0"])
        self.assertAlmostEqual(rows[0][1]["I"], 5.0, places=6)
        rows = self.run_script(["R", "RS -50 0", "S 21.05 21 5 0"])
        self.assertAlmostEqual(rows[0][1]["I"], -5.0, places=6)

    def test_ki_zero_matches_the_oracle(self):
        oracle = PidPy()
        oracle.integral = 50.0
        rows = self.run_script(["R", "RS 50 0", "S 20.95 21 5 0"])
        oracle.last_temperature = None
        oracle.update(f32(20.95), 0.05, 0.0, 5.0)
        self.assertEqual(oracle.ki(5.0), 0.0)
        self.assertAlmostEqual(rows[0][1]["I"], oracle.integral, places=6)


class Persistence(Base):
    """pid.py:141-157 restores five values with no age check; a restored state makes the first live step a normal step."""

    def test_round_trip_resumes_exactly_where_an_uninterrupted_controller_would(self):
        pre = ["R", "S 19 22 40 0", "S 19.5 22 40 120000", "S 21.93 22 40 240000", "S 21.95 22 40 360000"]
        post = "S 21.96 22 40 480000"
        # uninterrupted
        rows, pid = self.follow(pre + [post])
        want = [r[1] for r in rows if r[0] == "S"][-1]
        # interrupted: persist, reboot (reset), restore all five values, continue
        rows = self.run_script(pre + ["G"])
        g = rows[-1][1]
        self.assertEqual((g["errHeld"], g["tempHeld"]), (1.0, 1.0))
        resumed = self.run_script(["R", "RS %.9g %.9g" % (g["I"], g["rd"]),
                                   "RL 1 %.9g 1 %.9g %d" % (g["lastErr"], g["lastTemp"], int(g["ms"])),
                                   "S 21.96 22 40 480000"])
        got = resumed[0][1]
        self.assertAlmostEqual(got["I"], want["I"], places=5)
        self.assertAlmostEqual(got["rd"], want["rd"], places=7)
        self.assertNotEqual(want["rd"], 0.0, "premise: a non-trivial raw derivative is carried over")

    def test_without_the_last_temperature_the_first_step_is_a_cold_derivative_step(self):
        # restoring only integral and raw derivative (an old file) is a start-up step: the derivative is held, not moved
        rows = self.run_script(["R", "RS 0.5 0.02", "S 19.5 22 40 480000"])
        self.assertAlmostEqual(rows[0][1]["rd"], 0.02, places=6)

    def test_restore_of_the_last_values_lands_after_a_live_step_is_ignored(self):
        rows = self.run_script(["R", "S 19 22 40 0", "G", "RL 1 7 1 33 5", "G"])
        before, after = rows[1][1], rows[2][1]
        self.assertEqual(before, after, "a late restore must not overwrite newer live values (satPidRestoreLast)")

    def test_no_age_check_a_derivative_stamp_hours_old_still_computes_the_slope(self):
        # the stamp is just a time: 3 hours later the first live step uses it (pid.py:147-155, no staleness test)
        rows = self.run_script(["R", "RS 0 0", "RL 1 3 1 19 1000", "S 19.5 22 40 %d" % (1000 + 3 * 3600 * 1000)])
        self.assertNotEqual(rows[0][1]["rd"], 0.0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
