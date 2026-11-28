#!/usr/bin/env python3
"""TASK-2005 AC2: replay of indigo's Vitodens capture of 2026-11-16 16:17-16:41 through the real satCycleCheckOvershootSwitch().

Source: OTGW-field-data/indigo_light/20261116-144953 (2.0.0-beta.350+9011c3c), hours/20261116-16.zip, telnet.log, the
SATTICK lines at a 30 s control interval (fields pid = requested PID setpoint, flow). On beta.350 (1.5 C / 30 s, TASK-1888)
SAT left continuous for PWM three times inside one burn (flame=1 throughout), each time 30 s after the first tick with
flow - pid >= 1.5:

  segment 1, continuous 16:19:58 .. switch 16:30:28 (16:29:58 and 16:30:28: flow 42.4, pid 40.87, +1.53)
  segment 2, continuous 16:32:28 .. switch 16:35:28 (16:34:58 +1.62, 16:35:28 +2.42)
  segment 3, continuous 16:37:28 .. switch 16:39:28 (16:38:58 +2.02, 16:39:28 +2.82)

Each segment below is the recorded (pid, flow) per tick from its first continuous tick up to and including the tick on
which beta.350 switched. The default is now 3.0 C for 60 s (ADR-305/306 as thermo-nova; entry 407 and Q-407a had 2.0): none of the three
segments reaches the switch by that tick. After that tick the capture shows PWM behaviour, so the replay stops there.

The code under test is the real SATcycles.ino (cut by test_sat_overshoot_sustain.extract); the switch margin is the real
default from SATtypes.h, not the harness default. Old-vs-fix: OTGW_SATCYCLES_INO and OTGW_SATTYPES_H at the code before
TASK-2005 switch on exactly the recorded ticks (16:30:28, 16:35:28, 16:39:28): every case goes red.
Run: python tests/test_sat_vitodens_replay_2005.py
"""
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_overshoot_sustain as base  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

TYPES = Path(os.environ.get("OTGW_SATTYPES_H", base.FW / "SATtypes.h"))

# (time, pid, flow) per SATTICK line, first continuous tick .. the tick beta.350 switched on
SEG1 = [("16:19:58", 38.09, 26.1), ("16:20:28", 38.09, 27.0), ("16:20:58", 37.77, 28.8), ("16:21:28", 37.77, 31.0),
        ("16:21:58", 38.18, 34.0), ("16:22:28", 38.18, 36.6), ("16:22:58", 39.38, 38.7), ("16:23:28", 39.38, 39.9),
        ("16:23:58", 41.26, 40.7), ("16:24:28", 41.26, 41.1), ("16:24:58", 41.29, 41.5), ("16:25:28", 41.29, 41.9),
        ("16:25:58", 41.06, 41.9), ("16:26:28", 41.18, 41.9), ("16:26:58", 41.42, 42.4), ("16:27:28", 41.42, 42.4),
        ("16:27:58", 41.70, 42.4), ("16:28:28", 41.70, 41.9), ("16:28:58", 41.86, 42.4), ("16:29:28", 41.86, 42.4),
        ("16:29:58", 40.87, 42.4), ("16:30:28", 40.87, 42.4)]
SEG2 = [("16:32:28", 41.41, 35.2), ("16:32:58", 40.49, 37.0), ("16:33:28", 40.49, 38.7), ("16:33:58", 40.63, 39.9),
        ("16:34:28", 40.63, 40.7), ("16:34:58", 39.48, 41.1), ("16:35:28", 39.48, 41.9)]
SEG3 = [("16:37:28", 39.01, 37.1), ("16:37:58", 38.93, 37.9), ("16:38:28", 38.93, 39.1), ("16:38:58", 37.88, 39.9),
        ("16:39:28", 37.88, 40.7)]


def default_margin():
    m = re.search(r"^\s*float\s+fPwmSwitchMargin\s*=\s*([0-9.]+)f\s*;", TYPES.read_text(encoding="utf-8"), re.M)
    assert m, "fPwmSwitchMargin default not found in SATtypes.h"
    return m.group(1)


class VitodensReplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = base.CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        stubs = base.STUBS
        # the harness stub carries a margin literal; the replay uses the real default of SATtypes.h whatever the stub says (ADR-305: 3.0)
        stubs, n = re.subn(r"float fPwmSwitchMargin = [0-9.]+f;", "float fPwmSwitchMargin = %sf;" % default_margin(), stubs)
        assert n == 1, "test_sat_overshoot_sustain stub changed, patch target not found"
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "r.cpp"
        flag, body = base.extract(src)
        cpp.write_text(flag + stubs + body + base.MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "r.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    run_ticks = base.SustainedSwitches.run_ticks
    switch_at = base.SustainedSwitches.switch_at

    def replay(self, seg):
        r = self.run_ticks(["30000:g%g,%g" % (pid, flow) for _, pid, flow in seg])
        return int(r["t"]), r["mode"]

    def test_segment_1_stays_continuous_through_16_30_28(self):
        self.assertEqual(self.replay(SEG1), (-1, "1"), "beta.350 switched on 16:30:28 (+1.53 for 30 s)")

    def test_segment_2_stays_continuous_through_16_35_28(self):
        self.assertEqual(self.replay(SEG2), (-1, "1"), "beta.350 switched on 16:35:28 (+1.62, +2.42)")

    def test_segment_3_stays_continuous_through_16_39_28(self):
        self.assertEqual(self.replay(SEG3), (-1, "1"), "beta.350 switched on 16:39:28 (+2.02, +2.82)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
