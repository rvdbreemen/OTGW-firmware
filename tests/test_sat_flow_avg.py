#!/usr/bin/env python3
"""TASK-1990, Sergeant D entry 364 ("A, 2 minutes window"): during PWM ON the modulation suppression setpoint is the 2 minute
average of the boiler flow minus fModSupOffset, not the latest flow (thermo-nova heating_control.py:435-441 takes the latest).

Nothing is re-implemented. The host program holds the real SATflowAvg.h and the real satSuppressionFlowTemp() and
satPwmSuppressionSetpoint(), cut out of SATcontrol.ino. Stubs: a stub clock (millis), the latest fresh flow (satGetFlowTemp),
the flame-on time (satCycleGetFlameOnStartMs) and the settings the two functions read. The samples go in through the real
satFlowAvgNote(), as the processOT() hook does (OTGW-Core.ino, checked at source level in FeedHook below).

Old-vs-fix: OTGW_SAT_INO points at a copy without satSuppressionFlowTemp() (git show <old>:src/OTGW-firmware/SATcontrol.ino);
the program then calls the old satPwmSuppressionSetpoint(), which takes the latest flow, and the average cases fail.

Run: python tests/test_sat_flow_avg.py
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
#include "@HDR@"
#define SAT_MIN_SETPOINT 10.0f
static uint32_t g_now = 0, g_flameOn = 0; static float g_latest = 0;
static uint32_t millis() { return g_now; }
static float satGetFlowTemp() { return g_latest; }
static uint32_t satCycleGetFlameOnStartMs() { return g_flameOn; }
struct SatSettings { float fModSupOffset = 1.0f; bool bSimulation = false; }; struct Settings { SatSettings sat; }; static Settings settings;
struct OTBusS { SatFlowAvg flowAvg; }; struct State { OTBusS otBus; }; static State state;
"""

# stdin: lines "n <ms> <flow>" (note a sample), "f <ms>" (flame-on), "q <ms> <latest>" (query: prints the CS),
# "s <0|1>" (SAT simulation off/on)
MAIN = r"""
int main() {
  char op; unsigned long ms; float v;
  while (scanf(" %c %lu", &op, &ms) == 2) {
    g_now = (uint32_t)ms;
    if (op == 'n') { scanf("%f", &v); g_latest = v; satFlowAvgNote(state.otBus.flowAvg, g_now, v); }
    else if (op == 'f') { g_flameOn = (uint32_t)ms; }
    else if (op == 'q') { scanf("%f", &v); g_latest = v; printf("%.3f\n", satPwmSuppressionSetpoint(90.0f)); }
    else if (op == 's') { settings.sat.bSimulation = (ms != 0); }
  }
  return 0;
}
"""


class FlowAverage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.has_avg = "satSuppressionFlowTemp(" in src
        body = (func(src, "satSuppressionFlowTemp") if cls.has_avg else "") + func(src, "satPwmSuppressionSetpoint")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "avg.cpp"
        cpp.write_text(STUBS.replace("@HDR@", (FW / "SATflowAvg.h").as_posix()) + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "avg.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_script(self, lines):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        return [float(x) for x in out.split()]

    def test_a_step_inside_the_window_gives_the_mean_not_the_last_value(self):
        # flame on at 1 s; flow 40 for 60 s, then 50 for 60 s, one reading every 2 s; query at 121 s with the latest 50
        lines = ["f 1000"] + ["n %d 40" % t for t in range(2000, 62000, 2000)] + ["n %d 50" % t for t in range(62000, 122000, 2000)]
        cs, = self.run_script(lines + ["q 121000 50"])
        self.assertAlmostEqual(cs, 45.0 - OFFSET, places=2, msg="average 40/50 = 45, minus the offset")

    def test_readings_older_than_two_minutes_drop_out(self):
        lines = ["f 1000"] + ["n %d 40" % t for t in range(2000, 62000, 2000)] + ["n %d 50" % t for t in range(62000, 300000, 2000)]
        cs, = self.run_script(lines + ["q 299000 50"])
        self.assertAlmostEqual(cs, 50.0 - OFFSET, places=2, msg="only the last 120 s, all 50")

    def test_readings_before_the_flame_on_are_left_out(self):
        # OFF phase: standing water reads 63 (indigo_light 2026-11-15); flame on at 100 s; ON readings 42
        lines = ["n %d 63" % t for t in range(2000, 100000, 2000)] + ["f 100000"] + ["n %d 42" % t for t in range(100000, 222000, 2000)]
        cs, = self.run_script(lines + ["q 221000 42"])
        self.assertAlmostEqual(cs, 42.0 - OFFSET, places=2, msg="no OFF-phase reading in the ON average")

    def test_one_reading_per_two_seconds_at_most(self):
        # after 2 minutes of burning: 40 every 2 s, then a 100 at 122.0 s and a 0 at 122.5 s; the 0 is inside the 2 s spacing and is
        # not stored. Window at 123 s: 59 x 40 (4..120 s) + 100 -> mean 41.0 (with the 0 stored it would be 40.33)
        lines = ["f 1000"] + ["n %d 40" % t for t in range(2000, 122000, 2000)] + ["n 122000 100", "n 122500 0"]
        cs, = self.run_script(lines + ["q 123000 1"])
        self.assertAlmostEqual(cs, 41.0 - OFFSET, places=2)

    def test_no_fresh_flow_means_no_average(self):
        # the latest flow is not fresh (0): the average is not used either, as before (the minimum setpoint)
        cs, = self.run_script(["f 500", "n 1000 45", "q 2000 0"])
        self.assertAlmostEqual(cs, 10.0, places=2)

    def test_first_reading_after_ignition(self):
        cs, = self.run_script(["f 500", "n 1000 37", "q 1100 37"])
        self.assertAlmostEqual(cs, 37.0 - OFFSET, places=2)

    def test_start_up_follows_the_latest_flow(self):
        # entries 151, 336: in the first 2 minutes after the flame-on the CS follows the rising flow (35 -> 55 in 20 s), not a
        # window that is still filling (that average would be ~45, 10 K below the flow)
        lines = ["f 1000"] + ["n %d %d" % (1000 + 2000 * i, 35 + 2 * i) for i in range(11)]
        cs, = self.run_script(lines + ["q 21500 55"])
        self.assertAlmostEqual(cs, 55.0 - OFFSET, places=2)

    def test_the_average_starts_once_the_burner_ran_two_minutes(self):
        lines = ["f 1000"] + ["n %d 40" % t for t in range(2000, 62000, 2000)] + ["n %d 50" % t for t in range(62000, 122000, 2000)]
        before, after = self.run_script(lines + ["q 120000 50", "q 121000 50"])
        self.assertAlmostEqual(before, 50.0 - OFFSET, places=2, msg="119 s after the flame-on: still the latest flow")
        self.assertAlmostEqual(after, 45.0 - OFFSET, places=2, msg="120 s after the flame-on: the 2 minute average")

    def test_sat_simulation_uses_the_latest_flow(self):
        lines = ["s 1", "f 1000"] + ["n %d 40" % t for t in range(2000, 62000, 2000)] + ["n %d 50" % t for t in range(62000, 122000, 2000)]
        cs, = self.run_script(lines + ["q 121000 50"])
        self.assertAlmostEqual(cs, 50.0 - OFFSET, places=2, msg="the ring holds the real boiler, the simulation its own flow")


class FeedHook(unittest.TestCase):
    """Source level: processOT() notes only a valid boiler MsgID 25 sample, and not while the boiler reports hot water."""

    def test_hook_feeds_the_average_from_the_flow_feed_outside_hot_water(self):
        core = CORE_INO.read_text(encoding="utf-8")
        a = core.index("// TASK-1990 (entry 364): the same valid boiler flow sample")
        hook = core[a:a + 900]
        self.assertIn("OTdata.id == OT_Tboiler && OTdata.masterslave == 1 && !OTdata.skipthis", hook)
        self.assertIn("== SatFeedEvent::Valid", hook)
        self.assertIn("(OTcurrentSystemState.SlaveStatus & 0x0C) == 0x08", hook, "flame on (bit 3) and no hot water (bit 2)")
        self.assertIn("satFlowAvgNote(state.otBus.flowAvg, millis(), OTdata.f88())", hook)


if __name__ == "__main__":
    unittest.main(verbosity=2)
