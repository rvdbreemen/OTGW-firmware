#!/usr/bin/env python3
"""TASK-1440: the SAT multi-zone aggregation follows thermo-nova's area.py as it is (entry 116).

thermo-nova at 7beaaf5 (custom_components/sat):
  climate.py:369-394  requested_setpoint: ECO returns the primary PID only (:377-379); otherwise
                      setpoint = max(primary, areas output) (:385-386), then min(setpoint, overshoot cap)
                      (:388-389).
  area.py:166-175     requires_heat: valve >= 10 %, else pid.output > COLD_SETPOINT.
  area.py:264-301     _PIDs.output: the counted areas' outputs sorted, baseline = outputs[int(N * 0.75)]
                      (clamped to N-1), chosen = min(highest, baseline + headroom 5.0), round 1;
                      no counted area -> MINIMUM_SETPOINT 10 (:291-292).
  area.py:303-333     overshoot_cap: a counted area with error < -0.3 (OVERSHOOT_MARGIN, :33) gives
                      COLD_SETPOINT + 10 - 4 * degrees over (:322, :32, :34); the minimum wins, floored at
                      COLD_SETPOINT (:331), round 1.
ADR-199 (Robert, 2026-10-24): every COLD_SETPOINT 22 in area.py is the per-system cold setpoint
(28.2 radiators, 21.0 underfloor), satGetColdSetpoint().

The code under test is NOT re-implemented. From src/OTGW-firmware/SATcontrol.ino the test cuts the
real control-loop segment from "// --- Calculate heating curve ---" up to the deadband restore (the
whole zone block), satZoneUsable(), satGetColdSetpoint(), the SAT_* constants and, when present,
satAggregateZoneSetpoint(); from SATpid.ino the real satPidError(). Only the edges are stubbed: the
main PID output, the zone PID outputs (injected per zone; an unusable zone returns 10 as the real
satZonePidOutput() does), the heating curve and the diagnostics publisher. Built with a host C++
compiler; without one the tests are skipped, they do not pass silently.

OTGW_SAT_INO points at another copy (e.g. `git show <rev>:src/OTGW-firmware/SATcontrol.ino`) for
old-vs-fix runs; the helper tests then fail on a revision without satAggregateZoneSetpoint().

Run: python tests/test_sat_zone_aggregation.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import _fn, compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
PID_INO = REPO / "src" / "OTGW-firmware" / "SATpid.ino"

RAD, FLOOR = 1, 2           # SAT_HSYS_RADIATORS, SAT_HSYS_UNDERFLOOR (SATtypes.h:42-43)
COLD = {RAD: 28.2, FLOOR: 21.0}

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
#define SAT_HSYS_UNDERFLOOR 2
#define SAT_MAX_ZONES 4
struct SatSettings { float fHeatingCurveCoeff = 1.6f; int iZoneCount = 1; int iZoneTimeoutS = 900;
                     float fDeadband = 0.1f; float fZoneAggregationHeadroom = 5.0f; int iHeatingMode = 0; };
struct Settings { SatSettings sat; };
struct SatState { float fHeatingCurveValue = 0.0f; };
struct State { SatState sat; };
struct SATZoneState { float fRoomTemp = 0, fSetpoint = 0, fPidOutput = 0, fPidIntegral = 0, fPrevError = 0;
                      uint32_t iLastUpdateMs = 0; bool bRoomValid = false, bSpValid = false, bOff = false; };
static Settings settings; static State state; static SATZoneState satZones[SAT_MAX_ZONES];
static uint32_t millis() { return 1000; }
static uint8_t g_hsys = 1;
static uint8_t satGetEffectiveHeatingSystem() { return g_hsys; }
static float g_primary = 0.0f;
static float g_zoneOut[SAT_MAX_ZONES];
static float satCalcHeatingCurve(float, float) { return 40.0f; }   // the curve only feeds the stubbed PIDs
static float satPidOutput(float) { return g_primary; }
static float satPidCurveOnly(float) { return g_primary; }
static const bool roomValid = true;
static void satPublishZoneDiagnostics() { }
"""

ZONE_OUT = r"""
static float satZonePidOutput(uint8_t idx, float) {   // stub: injected output; unusable zone -> 10 as the real one
  if (!satZoneUsable(satZones[idx], millis())) return SAT_MIN_SETPOINT;
  satZones[idx].fPidOutput = g_zoneOut[idx];
  return g_zoneOut[idx];
}
"""

TICK_MAIN = r"""
int main(int argc, char** argv) {   // eco hsys primary zoneCount [out target room usable]...
  if (argc < 5) return 2;
  settings.sat.iHeatingMode = atoi(argv[1]);
  g_hsys = (uint8_t)atoi(argv[2]);
  g_primary = (float)atof(argv[3]);
  settings.sat.iZoneCount = atoi(argv[4]);
  for (int i = 5, z = 0; i + 3 < argc + 0 && z < SAT_MAX_ZONES; i += 4, z++) {
    g_zoneOut[z] = (float)atof(argv[i]);
    satZones[z].fSetpoint = (float)atof(argv[i + 1]);
    satZones[z].fRoomTemp = (float)atof(argv[i + 2]);
    bool usable = atoi(argv[i + 3]) != 0;
    satZones[z].bRoomValid = satZones[z].bSpValid = usable;
    satZones[z].iLastUpdateMs = 1000;
  }
  printf("pid=%.3f\n", tick(20.0f, 5.0f));
  return 0;
}
"""

HELPER_MAIN = r"""
int main(int argc, char** argv) {   // primary n coldSp headroom eco [out err usable]...
  if (argc < 6) return 2;
  float primary = (float)atof(argv[1]);
  int n = atoi(argv[2]);
  float coldSp = (float)atof(argv[3]), headroom = (float)atof(argv[4]);
  bool eco = atoi(argv[5]) != 0;
  float outs[16], errs[16]; bool use[16];
  for (int i = 0; i < n && i < 16; i++) {
    outs[i] = (float)atof(argv[6 + 3 * i]); errs[i] = (float)atof(argv[7 + 3 * i]);
    use[i] = atoi(argv[8 + 3 * i]) != 0;
  }
  printf("pid=%.3f\n", satAggregateZoneSetpoint(primary, outs, errs, use, (uint8_t)n, coldSp, headroom, eco));
  return 0;
}
"""


SOLAR_PROGRAM = r"""
#include <cstdio>
#include <cstdlib>
struct SatSettings { bool bSolarGainEnable = true; float fSolarSetpointOffset = 3.0f; int iHeatingMode = 0; };
struct Settings { SatSettings sat; };
struct SatState { bool bSolarGainActive = true; };
struct State { SatState sat; };
static Settings settings; static State state;
static float solar(float pidOutput) {
%s
  return pidOutput;
}
int main(int argc, char** argv) {   // eco primary
  settings.sat.iHeatingMode = atoi(argv[1]);
  printf("pid=%%.3f\n", solar((float)atof(argv[2])));
  return 0;
}
"""


def solar_program(src):
    a = src.index("  // --- Solar gain compensation on the requested setpoint")
    b = src.index("  // --- Clamp to valid range ---", a)
    return SOLAR_PROGRAM % src[a:b]


def _line(src, rx):
    m = re.search(rx, src, re.M)
    if not m:
        raise AssertionError(f"{rx} not found in {SAT_INO}")
    return m.group(0)


def _common(src):
    consts = "\n".join(_line(src, rf"^static const float {n}\s*=.*$")
                       for n in ("SAT_MIN_SETPOINT", "SAT_COLD_SETPOINT_RAD", "SAT_COLD_SETPOINT_FLOOR"))
    pid_err = _fn(PID_INO.read_text(encoding="utf-8"), r"float satPidError\(")
    cold = _fn(src, r"static float satGetColdSetpoint\(")
    usable = _fn(src, r"static bool satZoneUsable\(")
    return consts + "\n" + pid_err + "\n" + cold + "\n" + usable + "\n"


def _helper(src):
    if "satAggregateZoneSetpoint(" not in src:
        return None
    return _fn(src, r"static float satAggregateZoneSetpoint\(")


def tick_program(src):
    a = src.index("  // --- Calculate heating curve ---")
    b = src.index("  // Task #21: Restore original deadband", a)
    helper = _helper(src) or ""
    return (STUBS + _common(src) + helper + "\n" + ZONE_OUT +
            "static float tick(float effectiveTarget, float outsideTemp) {\n"
            "  float roomTemp = effectiveTarget - 0.5f; (void)roomTemp;\n"
            "  bool thermalDeadbandWidened = false; float savedDeadband = 0; (void)savedDeadband;\n"
            + src[a:b] + "\n  return pidOutput;\n}\n" + TICK_MAIN)


class ZoneAggregation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        (d / "tick.cpp").write_text(tick_program(cls.src), encoding="utf-8")
        cls.tick_exe = compile_cpp(d / "tick.cpp", d / "tick.exe")
        (d / "solar.cpp").write_text(solar_program(cls.src), encoding="utf-8")
        cls.solar_exe = compile_cpp(d / "solar.cpp", d / "solar.exe")
        helper = _helper(cls.src)
        cls.helper_exe = None
        if helper:
            (d / "helper.cpp").write_text(STUBS + _common(cls.src) + helper + HELPER_MAIN, encoding="utf-8")
            cls.helper_exe = compile_cpp(d / "helper.cpp", d / "helper.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def _run(self, exe, args):
        out = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        m = re.match(r"pid=(\S+)", out)
        self.assertIsNotNone(m, out)
        return float(m.group(1))

    def tick(self, primary, zones, eco=0, hsys=RAD, count=None):
        """zones: list of (output, target, room, usable). Returns the requested setpoint after the zone block."""
        if not self.tick_exe:
            self.skipTest("no host C++ compiler")
        args = [eco, hsys, primary, count if count is not None else max(1, len(zones))]
        for z in zones:
            args += list(z)
        return self._run(self.tick_exe, args)

    def helper(self, primary, zones, cold=28.2, headroom=5.0, eco=0):
        """zones: list of (output, error, usable)."""
        self.assertIsNotNone(_helper(self.src), "satAggregateZoneSetpoint() is missing from SATcontrol.ino")
        if not self.helper_exe:
            self.skipTest("no host C++ compiler")
        args = [primary, len(zones), cold, headroom, eco]
        for z in zones:
            args += list(z)
        return self._run(self.helper_exe, args)

    # --- through the real control-loop segment -----------------------------------------------
    def test_four_zones_pick_the_highest_output(self):
        # area.py:295: int(4 * 0.75) = 3, the highest of four; no overshoot, headroom not binding
        got = self.tick(35.0, [(40, 20, 20, 1), (42, 20, 20, 1), (44, 20, 20, 1), (46, 20, 20, 1)], count=4)
        self.assertAlmostEqual(got, 46.0, places=2)

    def test_zones_never_lower_the_primary(self):
        # climate.py:385-386: max(primary, areas); two zones below the primary leave it alone
        got = self.tick(50.0, [(30, 20, 20, 1), (32, 20, 20, 1)], count=2)
        self.assertAlmostEqual(got, 50.0, places=2)

    def test_one_zone_counts(self):
        # aggregation also with a single area (climate.py:383-386), also at zone count 1
        got = self.tick(35.0, [(45, 20, 20, 1)], count=1)
        self.assertAlmostEqual(got, 45.0, places=2)

    def test_eco_ignores_the_zones(self):
        # climate.py:377-379: ECO follows the primary PID only
        got = self.tick(35.0, [(45, 20, 20, 1), (47, 20, 20, 1)], eco=1, count=2)
        self.assertAlmostEqual(got, 35.0, places=2)

    def test_overshoot_ceiling(self):
        # area.py:315-322 with the radiator cold setpoint (ADR-199): 28.2 + 10 - 4 * 1.0 = 34.2,
        # applied after the max (climate.py:388-389); the cap may lower the setpoint below the zone output
        got = self.tick(35.0, [(45, 20.0, 21.0, 1)], count=1)
        self.assertAlmostEqual(got, 34.2, places=2)

    def test_overshoot_ceiling_floored_at_the_cold_setpoint(self):
        # area.py:331: 28.2 + 10 - 4 * 4.0 = 22.2 -> floored at 28.2
        got = self.tick(35.0, [(45, 20.0, 24.0, 1)], count=1)
        self.assertAlmostEqual(got, 28.2, places=2)

    def test_overshoot_only_beyond_the_margin(self):
        # area.py:315: error >= -0.3 is no overshoot; 0.3 over gives no cap, 0.31 over does
        self.assertAlmostEqual(self.tick(35.0, [(45, 20.0, 20.3, 1)], count=1), 45.0, places=2)
        self.assertAlmostEqual(self.tick(35.0, [(45, 20.0, 20.31, 1)], count=1), 37.0, places=2)

    def test_underfloor_uses_its_cold_setpoint(self):
        # ADR-199: underfloor cold 21.0; 21 + 10 - 4 * 1.0 = 27.0, and an output of 25 (> 21) counts
        self.assertAlmostEqual(self.tick(20.0, [(25, 20.0, 20.0, 1)], hsys=FLOOR, count=1), 25.0, places=2)
        self.assertAlmostEqual(self.tick(30.0, [(35, 20.0, 21.0, 1)], hsys=FLOOR, count=1), 27.0, places=2)

    def test_zone_at_or_below_cold_does_not_count(self):
        # area.py:175: output > COLD_SETPOINT; an overshooting zone at 28.0 (radiators) neither raises nor caps
        got = self.tick(35.0, [(28.0, 20.0, 22.0, 1)], count=1)
        self.assertAlmostEqual(got, 35.0, places=2)

    def test_unusable_zone_does_not_count(self):
        # area.py:279, :309: an area without a usable PID is skipped
        got = self.tick(35.0, [(45, 20.0, 20.0, 0)], count=1)
        self.assertAlmostEqual(got, 35.0, places=2)

    # --- solar gain in ECO (entry 118, Q58 A) ---------------------------------------------------
    def test_eco_applies_no_solar_offset(self):
        # entry 118: as thermo-nova, ECO returns before the solar step (climate.py:377-379 vs :391-392)
        if not self.solar_exe:
            self.skipTest("no host C++ compiler")
        self.assertAlmostEqual(self._run(self.solar_exe, [1, 40.0]), 40.0, places=3)

    def test_comfort_applies_the_solar_offset(self):
        # climate.py:391-392: outside ECO the active solar gain lowers the requested setpoint by the offset (3.0)
        if not self.solar_exe:
            self.skipTest("no host C++ compiler")
        self.assertAlmostEqual(self._run(self.solar_exe, [0, 40.0]), 37.0, places=3)

    # --- the helper directly ------------------------------------------------------------------
    def test_headroom_caps_above_the_baseline(self):
        # area.py:295-299 with 8 counted areas: baseline = outputs[6] = 40, highest 50 -> min(50, 40 + 5) = 45.
        # With the firmware's at most 4 zones the baseline is always the highest, so this path is unreachable
        # on the device; the helper still follows thermo-nova.
        zones = [(30 + i, 0.0, 1) for i in range(7)] + [(50, 0.0, 1)]   # 30..36 and 50
        zones[5] = (40, 0.0, 1)                                          # sorted 30,31,32,33,34,36,40,50: [6] = 40
        self.assertAlmostEqual(self.helper(20.0, zones), 45.0, places=2)

    def test_output_is_rounded_to_one_decimal(self):
        # area.py:301 round(chosen, 1)
        self.assertAlmostEqual(self.helper(20.0, [(45.04, 0.0, 1)]), 45.0, places=3)

    def test_no_counted_zone_leaves_the_primary(self):
        # area.py:291-292 returns MINIMUM_SETPOINT; max(primary, 10) is the primary
        self.assertAlmostEqual(self.helper(35.0, [(25, -1.0, 1)]), 35.0, places=2)

    # --- source checks ------------------------------------------------------------------------
    def test_tick_calls_the_helper_with_the_cold_setpoint(self):
        a = self.src.index("  // --- Calculate heating curve ---")
        b = self.src.index("  // Task #21: Restore original deadband", a)
        seg = self.src[a:b]
        self.assertIn("satAggregateZoneSetpoint(", seg)
        self.assertIn("satGetColdSetpoint()", seg)
        self.assertIn("satZonePidOutput(z, outsideTemp)", seg)
        self.assertNotIn("ceilf(0.75f", seg)


if __name__ == "__main__":
    unittest.main(verbosity=2)
