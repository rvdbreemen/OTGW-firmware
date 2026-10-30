#!/usr/bin/env python3
"""TASK-1416: the SAT PID integral is bounded by the heating curve only, as thermo-nova (Sergeant D entry 75, Q25).

thermo-nova at the pin: _update_integral() adds ki * error * PID_UPDATE_INTERVAL and clamps with
clamp_to_range(integral, heating_curve.value) (pid.py:185-192, helpers.py:148-150); update() has no minimum
interval (pid.py:159-171). Areas run the same PID (area.py:208). The firmware had an extra absolute cap of +-20 on
the primary PID (SATpid.ino) and on the zone PIDs (SATcontrol.ino); this test fails on that cap.

Nothing is re-implemented: the harness reuses the build of tests/test_sat_pid_cadence.py (PRELUDE, the zone cut
and the save cut), which compiles the real SATpid.ino and the real zone PID functions from SATcontrol.ino, and
drives satPidStep() / satZonePidAdvance() directly, one call per simulated 60 s.

Old-vs-fix: OTGW_SATPID_INO / OTGW_SAT_INO point at copies of origin/dev (`git show origin/dev:...`); there both
integrals stop at 20.000 (red). The 10 s gate case is green on origin/dev as well: TASK-1368 (8b2f5f7f9) already
removed SAT_PID_SAMPLE_TIME_LIMIT, so it documents the predecessor's fix and guards it, it is not this task's red.

Run: python tests/test_sat_pid_integral_cap.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pid_cadence as cad  # noqa: E402

MAIN = r"""
int main(int argc, char** argv) {
  const char* sc = argv[1];
  const int steps = atoi(argv[2]);
  g_ms = 1000;
  float i = 0, d = 0, rd = 0, e = 0;
  if (!strcmp(sc, "primary")) {           // room 20.95, target 21.0 (error 0.05, inside the 0.1 deadband), curve 40
    for (int n = 0; n < steps; n++) { satPidStep(20.95f, 21.0f, 40.0f, g_ms); g_ms += 60000; }
    satPidGetPersistState(&i, &d, &rd, &e);
    printf("I=%.6f ki=%.9f err=%.6f\n", i, state.sat.fKi, e);
    return 0;
  }
  if (!strcmp(sc, "close")) {             // the old 10 s gate: two steps 5 s apart both move the integral
    satPidStep(20.95f, 21.0f, 40.0f, g_ms); satPidGetPersistState(&i, &d, &rd, &e); const float a = i;
    g_ms += 5000; satPidStep(20.95f, 21.0f, 40.0f, g_ms); satPidGetPersistState(&i, &d, &rd, &e);
    printf("A=%.6f B=%.6f ki=%.9f\n", a, i, state.sat.fKi);
    return 0;
  }
  if (!strcmp(sc, "zone")) {              // zone 1: room 20.95, setpoint 21.0, outside 5 C (stub)
    settings.sat.iZoneCount = 1;
    SATZoneState& z = satZones[0];
    z.fSetpoint = 21.0f; z.fRoomTemp = 20.95f; z.bRoomValid = true; z.bSpValid = true; z.bOff = false;
    for (int n = 0; n < steps; n++) { z.iLastUpdateMs = g_ms; satZonePidAdvance(0, 5.0f); g_ms += 60000; }
    printf("ZI=%.6f curve=%.6f\n", z.fPidIntegral, satCalcHeatingCurve(21.0f, 5.0f));
    return 0;
  }
  return 1;
}
"""


def build_cpp(sat: str, pid: str) -> str:
    pid = pid[pid.index("// --- PID Constants ---"):]
    return cad.adapt(sat, cad.PRELUDE.replace("@TIMERS@", cad.TIMERS).replace("@ZONE@", cad.zone_source(sat)).replace("@PID@", pid)
            .replace("@GATHER@", cad.GATHER_STUB).replace("@SAVE@", cad.save_source(sat)) + MAIN)


def kv(line):
    return {k: float(v) for k, v in (p.split("=", 1) for p in line.split())}


class IntegralCap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        sat = cad.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = cad.PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cpp = d / "cap.cpp"
        cpp.write_text(build_cpp(sat, pid), encoding="utf-8")
        cls.exe = cad.compile_cpp(cpp, d / "cap.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc, steps=0):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc, str(steps)], capture_output=True, text=True, check=True).stdout
        return kv(out.strip())

    def test_primary_integral_passes_20_up_to_the_curve(self):
        # 3000 steps of ki * 0.05 * 60 each: thermo-nova's integral passes 20 and stops at the curve value 40
        # (pid.py:191-192). Red on origin/dev: 20.000.
        r = self.run_sc("primary", 3000)
        expect = min(3000 * r["ki"] * r["err"] * 60.0, 40.0)
        self.assertGreater(expect, 20.0, "premise: thermo-nova's integral passes 20 in this run")
        self.assertAlmostEqual(r["I"], expect, delta=0.05)

    def test_primary_integral_is_still_clamped_to_the_curve(self):
        r = self.run_sc("primary", 10000)
        self.assertAlmostEqual(r["I"], 40.0, places=4)

    def test_zone_integral_passes_20_up_to_the_curve(self):
        # Zones run the same PID (area.py:208); entry 75 read for zones (our reading, as entry 42). Red on origin/dev.
        r = self.run_sc("zone", 10000)
        self.assertGreater(r["curve"], 20.0, "premise: the zone curve is above 20")
        self.assertAlmostEqual(r["ZI"], r["curve"], places=3)

    def test_two_steps_five_seconds_apart_are_both_processed(self):
        # AC#2: no 10 s minimum between updates (pid.py:159-171). Already true on origin/dev (TASK-1368).
        r = self.run_sc("close")
        step = r["ki"] * 0.05 * 60.0
        self.assertAlmostEqual(r["A"], step, delta=1e-4)
        self.assertAlmostEqual(r["B"], 2 * step, delta=1e-4)


class SourceGuards(unittest.TestCase):
    def test_no_absolute_cap_left(self):
        pid = cad.PID_INO.read_text(encoding="utf-8")
        sat = cad.SAT_INO.read_text(encoding="utf-8")
        self.assertNotIn("SAT_PID_INTEGRAL_ABS_MAX", pid)
        self.assertNotIn("SAT_PID_SAMPLE_TIME_LIMIT", pid)
        self.assertNotIn("z.fPidIntegral >  20.0f", sat)


if __name__ == "__main__":
    unittest.main(verbosity=2)
