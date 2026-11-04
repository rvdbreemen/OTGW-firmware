#!/usr/bin/env python3
"""TASK-1360, TASK-1356, TASK-1361 (entries 257, 258, 259): host test of the SAT display changes.

The code under test is NOT re-implemented. The test cuts the real satCalcHeatingCurve() and the real
satUpdateBoilerStatus() (with its constants and tracking statics) out of src/OTGW-firmware/SATcontrol.ino,
plus satLastCycleExpired() from SATtypes.h, wraps them in stubs and compiles them with a host C++ compiler.
Without a compiler the tests are skipped.

OTGW_SAT_INO points at another copy (e.g. `git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for old-vs-fix runs.

  1360: curve = round(base + coeff/4 * curve, 1) as thermo-nova heating_curve.py:39
  1356: stall threshold uses the last cycle only while younger than 6 h, compares with >=
        (device/status.py:157-168, cycles/history.py:48-57)
  1361: overshoot_cooling follows the flame-off latch (device/__init__.py:204-207, device/status.py:121, :201-209)

Not reachable from here: the MQTT / REST / telnet publishers of eBoilerStatus (they read the field directly).

Run: python tests/test_sat_status_display.py
"""
import math
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
TYPES_H = FW / "SATtypes.h"

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <iostream>
#include <sstream>
#define PSTR(x) x
static const float SAT_HC_REF_TEMP = 20.0f;
@ENUMS@
struct SatSettings { float fHeatingCurveCoeff = 1.6f; float fOvershootMargin = 2.0f; };
struct Settings { SatSettings sat; };
struct SatState { SATBoilerStatus eBoilerStatus = SAT_BS_OFF; bool bBoilerStatusKnown = false; bool bDhwActive = false;
                  bool bModulationReliable = false; float fFinalSetpoint = 0; float fLastCycleDuration = 0;
                  uint32_t iLastCycleEndMs = 0; SATCycleClass eLastCycleClass = SAT_CYCLE_NONE; };
struct State { SatState sat; };
struct OTState { float RelModLevel = 0; };
static Settings settings; static State state; static OTState OTcurrentSystemState;
static uint32_t g_now = 0; static bool g_flame = false; static float g_flow = 0; static float g_base = 27.2f;
static uint32_t millis() { return g_now; }
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
static float satGetBaseOffset() { return g_base; }
static float satGetColdSetpoint() { return 28.2f; }
@TYPES@
@CURVE@
@STATUS@
int main(int argc, char** argv) {
  std::string mode = argv[1];
  if (mode == "curve") {
    // curve <coeff> <base> <target> <outside>
    settings.sat.fHeatingCurveCoeff = (float)atof(argv[2]); g_base = (float)atof(argv[3]);
    printf("%.6f\n", satCalcHeatingCurve((float)atof(argv[4]), (float)atof(argv[5])));
    return 0;
  }
  // status: one tick per stdin line: nowMs flame dhw flow setpoint lastDur lastEndMs cycleClass
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    unsigned long now, end; int flame, dhw, cls; float flow, sp, dur;
    is >> now >> flame >> dhw >> flow >> sp >> dur >> end >> cls;
    g_now = (uint32_t)now; g_flame = flame != 0; g_flow = flow;
    state.sat.bDhwActive = dhw != 0; state.sat.fFinalSetpoint = sp; state.sat.fLastCycleDuration = dur;
    state.sat.iLastCycleEndMs = (uint32_t)end; state.sat.eLastCycleClass = (SATCycleClass)cls;
    satUpdateBoilerStatus();
    printf("%d\n", (int)state.sat.eBoilerStatus);
  }
  return 0;
}
"""

ST = {"off": 0, "idle": 1, "stalled": 7, "anti_cycling": 8, "waiting": 10, "overshoot": 11,
      "post_cycle": 12, "cooling": 14}


def cut(src, start_re, end_re, what):
    m = re.search(start_re + r".*?(?=" + end_re + r")", src, re.S)
    if not m:
        raise AssertionError("%s not found in %s" % (what, SAT_INO))
    return m.group(0)


class StatusDisplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ino = SAT_INO.read_text(encoding="utf-8")
        th = TYPES_H.read_text(encoding="utf-8")
        enums = cut(th, r"enum SATCycleClass[^\n]*\{", r"\n\};", "SATCycleClass enum") + "\n};\n"
        enums += cut(th, r"enum SATBoilerStatus[^\n]*\{", r"\n\};", "SATBoilerStatus enum") + "\n};\n"
        types = cut(th, r"// --- satLastCycleExpired begin", r"// --- satLastCycleExpired end", "satLastCycleExpired")
        types += "\ntypedef SatState SATRuntimeSection;\n"
        types += cut(th, r"static inline SATCycleClass satReportedLastCycleClass", r"\n\}\r?\n", "satReportedLastCycleClass") + "\n}\n"
        curve = cut(ino, r"static float satCalcHeatingCurve\(", r"\n\}\r?\n", "satCalcHeatingCurve") + "\n}\n"
        status = (cut(ino, r"// --- Boiler Status Tracking ---", r"// --- Previous flame state", "bs statics") +
                  cut(ino, r"// SAT Python status.py timing constants", r"// String labels for boiler status", "status fn"))
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(STUBS.replace("@ENUMS@", enums).replace("@TYPES@", types)
                       .replace("@CURVE@", curve).replace("@STATUS@", status))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_exe(self, args, stdin=""):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return subprocess.run([str(self.exe)] + [str(a) for a in args], input=stdin,
                              capture_output=True, text=True, check=True).stdout.split()

    # ---- TASK-1360 -------------------------------------------------------
    def test_curve_rounded_to_one_decimal(self):
        for coeff, base in ((1.6, 27.2), (2.3, 27.2), (0.9, 20.0)):
            for target in (19.0, 20.5, 21.3):
                for outside in (-7.0, 0.0, 3.7, 12.4, 18.2):
                    diff = outside - 20.0
                    raw = base + (coeff / 4.0) * (4.0 * (target - 20.0) + 0.03 * diff * diff - 0.4 * diff)
                    if abs(raw * 10 - math.floor(raw * 10) - 0.5) < 1e-3:
                        continue   # exact .x5 tie: float32 vs Python float64 may differ by 0.1 C, not asserted
                    got = float(self.run_exe(["curve", coeff, base, target, outside])[0])
                    self.assertAlmostEqual(got, math.floor(raw * 10 + 0.5) / 10, places=4,
                                           msg="coeff=%s base=%s target=%s outside=%s raw=%s" % (coeff, base, target, outside, raw))

    def test_curve_known_value_is_rounded(self):
        # 27.2 + 0.4 * (4*1.3 + 0.03*7.3^2 + 0.4*7.3) = 27.2 + 0.4*(5.2+1.5987+2.92) = 31.0875 -> 31.1
        got = float(self.run_exe(["curve", 1.6, 27.2, 21.3, 12.7])[0])
        self.assertAlmostEqual(got, 31.1, places=4)

    # ---- TASK-1356 -------------------------------------------------------
    def status_seq(self, ticks):
        lines = "\n".join(" ".join(str(x) for x in t) for t in ticks) + "\n"
        return [int(x) for x in self.run_exe(["status"], lines)]

    def test_stall_ignores_last_cycle_older_than_6h(self):
        H = 3600 * 1000
        # last cycle lasted 1000 s (threshold 3 x = 3000 s). Fresh cycle: 700 s after flame-off is still waiting_flame.
        t_off = 100000
        fresh = self.status_seq([(t_off - 1000, 1, 0, 40, 40, 1000, t_off - 1000, 1), (t_off, 0, 0, 40, 40, 1000, t_off, 1),
                                 (t_off + 700000, 0, 0, 30, 40, 1000, t_off, 1)])
        self.assertEqual(fresh[-1], ST["waiting"])
        # The same cycle ended 7 h before the flame-off tick: forgotten, floor 600 s -> 700 s off is stalled
        far = 7 * H
        old = self.status_seq([(far - 1000, 1, 0, 40, 40, 1000, 1000, 1), (far, 0, 0, 40, 40, 1000, 1000, 1),
                               (far + 700000, 0, 0, 30, 40, 1000, 1000, 1)])
        self.assertEqual(old[-1], ST["stalled"])

    def test_stall_compares_with_ge(self):
        # off at t=2000, no cycle (floor 600 s): exactly 600 s later is stalled, 1 ms earlier waiting
        pre = [(1000, 1, 0, 40, 40, 0, 0, 0), (2000, 0, 0, 40, 40, 0, 0, 0)]
        at = self.status_seq(pre + [(2000 + 600000, 0, 0, 30, 40, 0, 0, 0)])[-1]
        before = self.status_seq(pre + [(2000 + 599999, 0, 0, 30, 40, 0, 0, 0)])[-1]
        self.assertEqual(before, ST["waiting"])
        self.assertEqual(at, ST["stalled"])

    # ---- TASK-1361 -------------------------------------------------------
    def test_pwm_off_phase_cs10_is_not_overshoot_cooling(self):
        # flame off, CS 10 (below the cold setpoint), flow 45: the old live test flow > CS + 2 said overshoot_cooling
        seq = self.status_seq([(1000, 1, 0, 45, 45, 0, 0, 0), (2000, 0, 0, 45, 10, 0, 0, 0),
                               (3000, 0, 0, 44, 10, 0, 0, 0), (200000, 0, 0, 40, 10, 0, 0, 0)])
        self.assertNotIn(ST["overshoot"], seq)

    def test_flame_off_overshoot_latches_and_clears(self):
        # setpoint 40, flow 43.5 >= 40 + 3 at the tick before flame-off -> latched
        seq = self.status_seq([(1000, 1, 0, 43.5, 40, 0, 0, 0),
                               (2000, 0, 0, 43.0, 40, 0, 0, 0),   # edge tick: flow above setpoint -> overshoot
                               (3000, 0, 0, 41.0, 40, 0, 0, 0),   # still above setpoint
                               (4000, 0, 0, 39.0, 40, 0, 0, 0),   # below setpoint: not overshoot_cooling any more
                               (5000, 0, 0, 41.0, 40, 0, 0, 0),   # latch stays, flow above again -> overshoot (thermo-nova)
                               (6000, 1, 0, 41.0, 40, 0, 0, 0),   # flame on clears the latch
                               (7000, 0, 0, 41.0, 40, 0, 0, 0)])  # off again, previous flow 41 < 43: no latch
        self.assertEqual(seq[1], ST["overshoot"])
        self.assertEqual(seq[2], ST["overshoot"])
        self.assertNotEqual(seq[3], ST["overshoot"])
        self.assertEqual(seq[4], ST["overshoot"])
        self.assertNotEqual(seq[6], ST["overshoot"])

    def test_small_overshoot_is_not_latched(self):
        # flow 42.9 < setpoint 40 + 3: no latch
        seq = self.status_seq([(1000, 1, 0, 42.9, 40, 0, 0, 0), (2000, 0, 0, 42.0, 40, 0, 0, 0),
                               (3000, 0, 0, 41.5, 40, 0, 0, 0)])
        self.assertNotIn(ST["overshoot"], seq)

    def test_setpoint_below_cold_setpoint_skips_latch(self):
        # setpoint 28.1 < 28.2, flow 40 would be >= +3, skipped
        seq = self.status_seq([(1000, 1, 0, 40, 28.1, 0, 0, 0), (2000, 0, 0, 40, 28.1, 0, 0, 0),
                               (3000, 0, 0, 39, 28.1, 0, 0, 0)])
        self.assertNotIn(ST["overshoot"], seq)
        # setpoint exactly at the cold setpoint (strict <) latches
        seq = self.status_seq([(1000, 1, 0, 40, 28.2, 0, 0, 0), (2000, 0, 0, 40, 28.2, 0, 0, 0)])
        self.assertEqual(seq[-1], ST["overshoot"])


if __name__ == "__main__":
    unittest.main()
