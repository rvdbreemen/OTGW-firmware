#!/usr/bin/env python3
"""TASK-1359: the heating-curve coefficient range is 0.1-5.0 everywhere (ADR-198 open question 1).

The setter clamps SATcoefficient to 0.1-5.0 (settingStuff.ino). Before the fix the REST settings
metadata announced 0-5 as integers (addNum took int bounds, so a fractional bound could not be
sent) and the SAT setup wizard slider offered 0.2-2.4.

Old-vs-fix: OTGW_SRC_DIR points at a directory with older copies of restAPI.ino, settingStuff.ino
and data/v2.js.

Run: python tests/test_sat_coefficient_range.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC = Path(os.environ.get("OTGW_SRC_DIR", REPO / "src" / "OTGW-firmware"))


def read(name):
    return (SRC / name).read_text(encoding="utf-8")


class CoefficientRange(unittest.TestCase):
    def test_rest_metadata_announces_0_1_to_5_0(self):
        rest = read("restAPI.ino")
        self.assertRegex(rest, r'addNum\(F\("satcoefficient"\), tmpBuf, "f", 0\.1f?, 5(\.0)?f?\)')
        sig = re.search(r"auto addNum = \[&\]\([^)]*\)", rest).group(0)
        self.assertRegex(sig, r"float minValue, float maxValue", "addNum bounds must carry decimals")

    def test_wizard_slider_spans_the_setting_range(self):
        v2 = read("data/v2.js")
        self.assertIn('id="satOnbSlope" type="range" class="satonb-slope" min="0.1" max="5.0" step="0.1"', v2)
        self.assertIn("((v - 0.1) / (5.0 - 0.1))", v2)

    def test_setter_clamp_unchanged(self):
        s = read("settingStuff.ino")
        i = s.index('PSTR("SATcoefficient")')
        self.assertRegex(s[i:i + 400], r"0\.1f?,\s*5\.0f?")


if __name__ == "__main__":
    unittest.main(verbosity=2)
