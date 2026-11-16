#!/usr/bin/env python3
"""TASK-1990, Sergeant D entry 383 (2026-11-16, confirms entry 375, replaces our reading of entry 364): no 2 minute flow
average anywhere. During PWM ON the modulation suppression setpoint is the LATEST boiler flow minus fModSupOffset, as
thermo-nova (heating_control.py:435-441 at 7beaaf5); the 180 s flame-on rule (entry 275) covers the light-off.

Nothing is re-implemented. The host program holds the real satPwmSuppressionSetpoint() cut out of SATcontrol.ino. Stubs: a stub
clock (millis), the latest fresh flow (satGetFlowTemp), the flame-on time and the settings the function reads.

Old-vs-fix: OTGW_SAT_INO points at the SATcontrol.ino of the 2 minute average (beta.330..beta.341) and OTGW_FLOWAVG_H at its
SATflowAvg.h. The program then also holds the old satSuppressionFlowTemp(), every flow reading goes into the old ring through
the real satFlowAvgNote(), and the cases where the average differs from the latest flow fail.

Run: python tests/test_sat_suppression_latest_flow.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
FLOWAVG_H = os.environ.get("OTGW_FLOWAVG_H", "")
CORE_INO = FW / "OTGW-Core.ino"
OFFSET = 1.0   # fModSupOffset default (entry 347)


def func(src, name):
    m = re.search(r"^static float " + name + r"\(.*?\n\{.*?\n\}\n", src, re.S | re.M)
    if not m:
        raise AssertionError(name + " not found")
    return m.group(0)


STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
@OLD_INCLUDE@
#define SAT_MIN_SETPOINT 10.0f
static uint32_t g_now = 0, g_flameOn = 0; static float g_latest = 0;
static uint32_t millis() { return g_now; }
static float satGetFlowTemp() { return g_latest; }
static uint32_t satCycleGetFlameOnStartMs() { return g_flameOn; }
struct SatSettings { float fModSupOffset = 1.0f; bool bSimulation = false; }; struct Settings { SatSettings sat; }; static Settings settings;
@OLD_STATE@
"""

# stdin: lines "n <ms> <flow>" (a flow reading), "f <ms>" (flame-on), "q <ms> <latest>" (query: prints the CS)
MAIN = r"""
int main() {
  char op; unsigned long ms; float v;
  while (scanf(" %c %lu", &op, &ms) == 2) {
    g_now = (uint32_t)ms;
    if (op == 'n') { scanf("%f", &v); g_latest = v; NOTE(v); }
    else if (op == 'f') { g_flameOn = (uint32_t)ms; }
    else if (op == 'q') { scanf("%f", &v); g_latest = v; printf("%.3f\n", satPwmSuppressionSetpoint(90.0f)); }
  }
  return 0;
}
"""


class SuppressionFollowsLatestFlow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        old = "satSuppressionFlowTemp(" in src
        if old and not FLOWAVG_H:
            raise AssertionError("old code under test needs OTGW_FLOWAVG_H (its SATflowAvg.h)")
        body = (func(src, "satSuppressionFlowTemp") if old else "") + func(src, "satPwmSuppressionSetpoint")
        stubs = STUBS.replace("@OLD_INCLUDE@", '#include "%s"' % Path(FLOWAVG_H).as_posix() if old else "")
        stubs = stubs.replace("@OLD_STATE@", "struct OTBusS { SatFlowAvg flowAvg; }; struct State { OTBusS otBus; }; static State state;\n"
                              "#define NOTE(v) satFlowAvgNote(state.otBus.flowAvg, g_now, (v))" if old else "#define NOTE(v) ((void)(v))")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "sup.cpp"
        cpp.write_text(stubs + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "sup.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_script(self, lines):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        return [float(x) for x in out.split()]

    def test_a_flow_step_after_two_minutes_of_burning_gives_the_latest_flow(self):
        # flame on at 1 s; flow 40 for 60 s, then 50 for 60 s, one reading every 2 s; at 121 s the CS follows the latest 50
        # (the 2 minute average of beta.330..341 gave 45)
        lines = ["f 1000"] + ["n %d 40" % t for t in range(2000, 62000, 2000)] + ["n %d 50" % t for t in range(62000, 122000, 2000)]
        cs, = self.run_script(lines + ["q 121000 50"])
        self.assertAlmostEqual(cs, 50.0 - OFFSET, places=2)

    def test_a_falling_flow_is_followed_at_once(self):
        # 3 minutes at 55, then the flow drops to 45: the CS drops with it, not with a window still full of 55
        lines = ["f 1000"] + ["n %d 55" % t for t in range(2000, 182000, 2000)] + ["n %d 45" % t for t in range(182000, 192000, 2000)]
        cs, = self.run_script(lines + ["q 191000 45"])
        self.assertAlmostEqual(cs, 45.0 - OFFSET, places=2)

    def test_start_up_follows_the_latest_flow(self):
        lines = ["f 1000"] + ["n %d %d" % (1000 + 2000 * i, 35 + 2 * i) for i in range(11)]
        cs, = self.run_script(lines + ["q 21500 55"])
        self.assertAlmostEqual(cs, 55.0 - OFFSET, places=2)

    def test_no_fresh_flow_gives_the_minimum(self):
        cs, = self.run_script(["f 500", "n 1000 45", "q 2000 0"])
        self.assertAlmostEqual(cs, 10.0, places=2)


class NoAverageLeft(unittest.TestCase):
    """Source level: the 2 minute ring and its processOT() hook are gone (entry 383)."""

    def test_no_flow_average_in_the_firmware(self):
        self.assertFalse((FW / "SATflowAvg.h").exists())
        for name in ("SATcontrol.ino", "OTGW-Core.ino", "OTBustypes.h"):
            text = (FW / name).read_text(encoding="utf-8")
            for token in ("satFlowAvg", "SatFlowAvg", "flowAvg", "satSuppressionFlowTemp"):
                self.assertNotIn(token, text, "%s still names %s" % (name, token))
        self.assertIn("float cs = satGetFlowTemp() - settings.sat.fModSupOffset;", (FW / "SATcontrol.ino").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
