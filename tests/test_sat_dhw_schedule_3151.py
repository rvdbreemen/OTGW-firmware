#!/usr/bin/env python3
"""TASK-3151.4 (ADR-305 decision 8, thermo-nova comparison D4): a PWM phase change that falls due during a hot-water draw is taken during
the draw, as thermo-nova, whose heating_control.update() calls pwm.update() on every pass, hot water included (heating_control.py:202-215,
pwm.py:117-214; its only hot-water checks are the enable/exit guards and the low-duty flame test, pwm.py:248).

The control tick (satControlLoopRun) cannot be compiled on the host, so the case is a source-level check on the real SATcontrol.ino:
(1) the DHW branch of the control tick runs satPwmSchedule() for the PWM mode, with the request of the last control tick and only while
    that tick had heating demand, before it returns;
(2) satPwmSchedule() itself sends nothing (no CS, CH, MM or queued command), so the call changes the PWM phase only;
(3) the DHW branch still sends nothing but the MM maximum for the draw (entry 220): no CS/CH send in it.
The behaviour of satPwmSchedule() (phase machine, anchors) is covered by test_sat_hp_pwm.py and the PWM tests that compile it.
Red on the code before this change (the DHW branch returns before the schedule), green after.

OTGW_SAT_INO points at another copy of SATcontrol.ino (old-vs-fix).
Run: python tests/test_sat_dhw_schedule_3151.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))


def body(src, signature):
    i = src.index(signature)
    j = src.index("{", i)
    depth, k = 0, j
    while True:
        c = src[k]
        depth += c == "{"
        depth -= c == "}"
        k += 1
        if depth == 0:
            return src[i:k]


class DhwSchedule(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        loop = cls.src[cls.src.index("static void satControlLoopRun()"):]   # no brace matching: the loop holds braces inside strings
        a = loop.index("if (state.sat.bDhwActive) {")
        b = loop.index("    return;\n  }", a)
        cls.branch = loop[a:b]

    def test_the_draw_branch_runs_the_schedule_for_pwm(self):
        self.assertRegex(self.branch,
                         r"if \(state\.sat\.eControlMode == SAT_MODE_PWM && _sat_requestedSetpoint > satColdThreshold\(\)\) "
                         r"satPwmSchedule\(_sat_requestedSetpoint\);")

    def test_the_schedule_runs_before_the_branch_sends_anything(self):
        call = self.branch.index("satPwmSchedule(_sat_requestedSetpoint)")
        for sender in ("satSendMMDuringDhw", "satPublishMQTT"):
            self.assertLess(call, self.branch.index(sender), sender + " comes after the schedule")

    def test_the_schedule_sends_nothing(self):
        sched = body(self.src, "static void satPwmSchedule(float pidOutput)")
        for forbidden in ("satSendCS", "satSendCH", "satSendMM", "addCommandToQueue", "satEnqueue", "satSendRelease"):
            self.assertNotIn(forbidden, sched)

    def test_the_draw_branch_sends_no_cs_or_ch(self):
        for forbidden in ("satSendCS(", "satSendCH(", "satEnqueueIfChangedCS", "satApplyPWM("):
            self.assertNotIn(forbidden, self.branch)


if __name__ == "__main__":
    unittest.main(verbosity=2)
