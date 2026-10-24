#!/usr/bin/env python3
"""TASK-1347: the SAT wizard must not overwrite the heating-curve coefficient.

Sergeant D, entry 24: default 2.0 (thermo-nova const.py:182), stored values stay.
Source checks on the real files (data/v2.js, SATtypes.h, settingsHelp.h); they do not
run the wizard in a browser. OTGW_V2_JS / OTGW_SATTYPES_H point at other copies for
old-vs-fix runs.

Run: python tests/test_sat_wizard_coefficient.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
V2 = Path(os.environ.get("OTGW_V2_JS", FW / "data" / "v2.js")).read_text(encoding="utf-8", errors="replace")
TYPES = Path(os.environ.get("OTGW_SATTYPES_H", FW / "SATtypes.h")).read_text(encoding="utf-8", errors="replace")


def wizard_commit() -> str:
    i = V2.find("function commit(then)")
    assert i >= 0, "wizard commit() not found"
    return V2[i:V2.find(".then(function () { then(true); }", i)]


class WizardCoefficient(unittest.TestCase):
    def test_no_per_system_slope_presets(self):
        m = re.search(r"var SAT_SYS = \{[^\n]*\};", V2)
        self.assertIsNotNone(m)
        self.assertNotIn("slope", m.group(0), "SAT_SYS must not carry slope presets (1.4/0.6/0.8)")

    def test_automatic_tuning_does_not_write_coefficient(self):
        body = wizard_commit()
        self.assertNotRegex(body, r"\[\s*\[?'satsystem'[^\n]*'satcoefficient'",
                            "satcoefficient must not be in the unconditional write list")
        self.assertRegex(body, r"if \(st\.tuning === 'manual'\) kv\.push\(\['satcoefficient'",
                         "satcoefficient is written only for Manual tuning")

    def test_manual_slider_starts_at_stored_value(self):
        self.assertRegex(V2, r"slope: \(setData\.satcoefficient", "the slider starts at the stored coefficient")

    def test_firmware_default_is_2(self):
        m = re.search(r"float\s+fHeatingCurveCoeff\s*=\s*([0-9.]+)f", TYPES)
        self.assertIsNotNone(m)
        self.assertEqual(float(m.group(1)), 2.0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
