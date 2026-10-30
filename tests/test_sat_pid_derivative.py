#!/usr/bin/env python3
"""TASK-1418: the SAT PID derivative matches thermo-nova pid.py (Sergeant D entry 77, Q27: "match thermo-nova").

Two details of pid.py _update_derivative() (pid.py:199-232) at the pin:
  - "no change" is temperature_delta == 0.0 exactly (pid.py:211);
  - the previous temperature is overwritten on every update (pid.py:171), also when the derivative step is
    skipped because 60 s or less passed since the last derivative update (pid.py:215-218).
The firmware kept a separate reference that moved only with the derivative timestamp (TASK-1195) and tested
fabsf(delta) < 0.001.

The code under test is the REAL satPidStep() / satPidNoteRoom() from SATpid.ino and the REAL room EMA
satRoomFiltered() from SATcontrol.ino, compiled on the host (build of tests/test_sat_pid_cadence.py). The C side
prints, per PID step, the room value it used, the room-changed stamp and the raw derivative. The ORACLE is
thermo-nova's _update_derivative() transcribed line for line below (with pid.py's own constants): it is the
reference, not a copy of the firmware. It is fed exactly the values the firmware saw.

Room series: synthetic, not a field recording. A sensor that rises 0.1 C every 10 minutes (and one case with a
falling sensor), through the 30 s control tick and the room EMA, with the PID stepping every 60 s and the error
outside the 0.1 deadband (target 22.0). A step-wise series without the EMA is included as the case where TASK-1195
said both rules agree.

Old-vs-fix: OTGW_SATPID_INO / OTGW_SAT_INO point at origin/dev copies; there the derivative differs from pid.py.
The test also prints the mean |raw derivative| of old and new for the bench comparison of AC#2 (host only).

Run: python tests/test_sat_pid_derivative.py
"""
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pid_cadence as cad  # noqa: E402

MAIN = r"""
int main(int argc, char** argv) {
  // stdin: one line per 30 s control tick: "<t_s> <raw sensor C> <use_ema 0|1>"
  unsigned t; float raw; int ema;
  while (scanf("%u %f %d", &t, &raw, &ema) == 3) {
    g_ms = t * 1000u;
    float room = ema ? satRoomFiltered(raw) : raw;
    satPidNoteRoom(room, g_ms);                       // the control tick stamps every tick (TASK-1195)
    if (t % 60 == 0) {
      satPidStep(room, 22.0f, 40.0f, g_ms);
      printf("S %u %.9g %u %.9g\n", t, room, (unsigned)_pid_roomTempChangedMs, _pid_rawDerivative);
    }
  }
  return 0;
}
"""

# ---- thermo-nova pid.py constants and _update_derivative(), transcribed (pid.py:19-20, :199-232; const.py:16) ----
DEADBAND = 0.1
PID_UPDATE_INTERVAL = 60
DERIVATIVE_RAW_CAP = 5.0


class PidPyDerivative:
    def __init__(self):
        self._last_temperature = None
        self._last_derivative_updated = None
        self._raw_derivative = 0.0

    def update(self, current, error, last_changed):          # pid.py:159-171, the derivative part
        self._update_derivative(current, error, last_changed)
        self._last_temperature = current                       # pid.py:171

    def _update_derivative(self, current, error, last_changed):
        if self._last_temperature is None or self._last_derivative_updated is None:
            self._last_derivative_updated = last_changed
            return
        if abs(error) <= DEADBAND:
            self._last_derivative_updated = last_changed
            return
        temperature_delta = current - self._last_temperature
        if temperature_delta == 0.0:
            self._last_derivative_updated = last_changed
            return
        delta_time = last_changed - self._last_derivative_updated
        if delta_time <= PID_UPDATE_INTERVAL:
            return
        derivative = -temperature_delta / delta_time
        if abs(derivative) >= DERIVATIVE_RAW_CAP:
            self._raw_derivative = max(-DERIVATIVE_RAW_CAP, min(derivative, DERIVATIVE_RAW_CAP))
            self._last_derivative_updated = last_changed
            return
        alpha = delta_time / (PID_UPDATE_INTERVAL + delta_time)
        filtered = alpha * derivative + (1 - alpha) * self._raw_derivative
        self._raw_derivative = max(-DERIVATIVE_RAW_CAP, min(filtered, DERIVATIVE_RAW_CAP))
        self._last_derivative_updated = last_changed


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def series(kind, hours=3):
    """30 s ticks: (t, raw sensor, ema flag)."""
    out = []
    for t in range(0, hours * 3600 + 1, 30):
        if kind == "rise":
            raw = 20.0 + 0.1 * (t // 600)
        elif kind == "fall":
            raw = 21.5 - 0.1 * (t // 600)
        else:  # "steps": step-wise sensor, no EMA
            raw = 20.0 + 0.1 * (t // 600)
        out.append((t, raw, 0 if kind == "steps" else 1))
    return out


def build_cpp(sat: str, pid: str) -> str:
    pid = pid[pid.index("// --- PID Constants ---"):]
    ema = cad._fn(sat, r"static float satRoomFiltered\(")
    return cad.adapt(sat, cad.PRELUDE.replace("@TIMERS@", cad.TIMERS).replace("@ZONE@", cad.zone_source(sat))
                     .replace("@PID@", pid + "\n" + ema).replace("@GATHER@", cad.GATHER_STUB)
                     .replace("@SAVE@", cad.save_source(sat)) + MAIN)


class Derivative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        sat = cad.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = cad.PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cpp = d / "deriv.cpp"
        cpp.write_text(build_cpp(sat, pid), encoding="utf-8")
        cls.exe = cad.compile_cpp(cpp, d / "deriv.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_series(self, kind):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        stdin = "".join(f"{t} {raw:.6f} {ema}\n" for t, raw, ema in series(kind))
        out = subprocess.run([str(self.exe)], input=stdin, capture_output=True, text=True, check=True).stdout
        steps = []
        for line in out.splitlines():
            _, t, room, changed, rd = line.split()
            steps.append((int(t), float(room), int(changed), float(rd)))
        oracle = PidPyDerivative()
        rows = []
        for t, room, changed, rd in steps:
            current = f32(room)
            error = round(22.0 - current, 3)                       # TemperatureState.error is rounded (temperature/state.py)
            oracle.update(current, error, changed / 1000.0)
            rows.append((t, rd, oracle._raw_derivative))
        return rows

    def assert_matches(self, rows):
        bad = [(t, fw, py) for t, fw, py in rows if abs(fw - py) > max(1e-9, 1e-3 * abs(py))]
        self.assertEqual(bad[:5], [], f"{len(bad)} of {len(rows)} steps differ from pid.py (t, firmware, pid.py)")
        self.assertTrue(any(py != 0.0 for _, _, py in rows), "premise: pid.py's derivative moves in this series")

    def test_rising_room_through_the_ema_matches_pid_py(self):
        rows = self.run_series("rise")
        self.assert_matches(rows)
        mean = sum(abs(fw) for _, fw, _ in rows) / len(rows)
        print(f"\n  [rise] mean |raw derivative| = {mean:.3e} C/s over {len(rows)} steps")

    def test_falling_room_through_the_ema_matches_pid_py(self):
        self.assert_matches(self.run_series("fall"))

    def test_step_wise_sensor_without_ema_matches_pid_py(self):
        self.assert_matches(self.run_series("steps"))


class SourceGuards(unittest.TestCase):
    def test_no_separate_reference_and_exact_zero_test(self):
        pid = cad.PID_INO.read_text(encoding="utf-8")
        self.assertNotIn("_pid_derivRefTemp", pid)
        self.assertIn("float tempDelta = roomTemp - _pid_lastRoomTemp;", pid)
        self.assertIn("if (tempDelta == 0.0f) {", pid)
        self.assertNotIn("fabsf(tempDelta) < 0.001f", pid)


if __name__ == "__main__":
    unittest.main(verbosity=2)
