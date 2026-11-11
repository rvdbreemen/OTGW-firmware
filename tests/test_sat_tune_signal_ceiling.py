#!/usr/bin/env python3
"""TASK-1815 / Sergeant D entry 317: the self-tune signal is clamped at the effective ceiling.

The tuner's signal is clamp(pidRaw, SAT_MIN_SETPOINT, ceiling) - curve (ADR-205 decision 1, kept by
ADR-228). Entry 317 (Q-1796d B) names the ceiling: the one SAT already applies to the setpoint it sends,
satEffectiveMaxSetpoint() (boiler MaxTSet MsgID 57 / SH=, user max, hard ceiling), not the SAT max
setpoint alone (satGetMaxSetpoint()). Field case: a boiler capped at 48 C with SAT max 65 C counted a
request up to 65 C that the boiler could not deliver.

This is a SOURCE test of src/OTGW-firmware/SATcontrol.ino (comments removed): the clamp sits in the
control tick next to a lot of device state, so the test resolves which function fills the variable the
upper clamp uses, in the same function body. OTGW_SATCONTROL points at another copy (old-vs-fix runs).
The bench proof (a boiler MaxTSet below the user max shows the clamped signal in SATTUNEH) is separate.

Run: python tests/test_sat_tune_signal_ceiling.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC_PATH = Path(os.environ.get("OTGW_SATCONTROL", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


SRC = strip_comments(SRC_PATH.read_text(encoding="utf-8").replace("\r\n", "\n"))


def enclosing_function(src, pos):
    """The text from the last top-level function start before pos up to pos."""
    starts = [m.start() for m in re.finditer(r"\n(?:static\s+)?(?:void|bool|float|int|uint\w*|unsigned\s+long)\s+\w+\s*\([^;{]*\)\s*\{", src)
              if m.start() < pos]
    assert starts, "no enclosing function found"
    return src[starts[-1]:pos]


class SignalCeiling(unittest.TestCase):
    def setUp(self):
        m = re.search(r"float\s+sigPid\s*=\s*pidRaw\s*;(.*?)ti\.signal\s*=\s*sigPid\s*-\s*curveValue\s*;", SRC, re.S)
        self.assertIsNotNone(m, "tuner signal block (sigPid = pidRaw ... ti.signal = sigPid - curveValue) not found")
        self.block, self.pos = m.group(1), m.start()

    def test_lower_clamp_is_min_setpoint(self):
        self.assertRegex(self.block, r"if\s*\(\s*sigPid\s*<\s*SAT_MIN_SETPOINT\s*\)\s*sigPid\s*=\s*SAT_MIN_SETPOINT\s*;")

    def test_upper_clamp_is_the_effective_ceiling(self):
        m = re.search(r"if\s*\(\s*sigPid\s*>\s*(\w+)\s*\)\s*sigPid\s*=\s*(\w+)\s*;", self.block)
        self.assertIsNotNone(m, "upper clamp of sigPid not found")
        self.assertEqual(m.group(1), m.group(2), "the clamp compares and assigns different values")
        var = m.group(1)
        body = enclosing_function(SRC, self.pos)
        defs = re.findall(rf"\bfloat\s+{var}\s*=\s*(\w+)\s*\(\s*\)\s*;", body)
        self.assertEqual(len(defs), 1, f"{var} must be defined once from a function call in the control tick: {defs}")
        self.assertEqual(defs[0], "satEffectiveMaxSetpoint",
                         f"the tuner signal is clamped at {var} = {defs[0]}(), entry 317 wants satEffectiveMaxSetpoint()")
        self.assertIsNone(re.search(rf"\b{var}\s*(?:[-+*/]?=)(?!=)", body[body.index(f"{var} = {defs[0]}") + 1:]),
                          f"{var} is reassigned before the clamp")

    def test_effective_ceiling_includes_the_boiler_maximum(self):
        m = re.search(r"static\s+float\s+satEffectiveMaxSetpoint\s*\(\s*\)\s*\{(.*?)\n\}", SRC, re.S)
        self.assertIsNotNone(m)
        self.assertIn("OTcurrentSystemState.MaxTSet", m.group(1), "the effective ceiling must start from the boiler MaxTSet")


if __name__ == "__main__":
    unittest.main(verbosity=2)
