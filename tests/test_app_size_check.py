#!/usr/bin/env python3
"""TASK-1993 (TASK-1955 AC2, option A): every esp32 build logs the app image size against the ota_0 slot, warns above
95 % and fails above 98 %.

The test runs the real scripts/check_app_size.py on fake images of a chosen size (no firmware build needed) and checks
at source level that build.py runs it on the collected .ino.bin with the ota_0 size from partitions_otgw_esp32.csv.
Before TASK-1993 the script and the call do not exist and every case fails.

Run: python tests/test_app_size_check.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = REPO / "scripts" / "check_app_size.py"
BUILD_PY = REPO / "build.py"
SLOT = 2293760   # ota_0 in partitions_otgw_esp32.csv (0x230000)


class AppSizeCheck(unittest.TestCase):
    def run_check(self, percent):
        self.assertTrue(SCRIPT.exists(), "scripts/check_app_size.py is missing")
        size = int(SLOT * percent / 100)
        with tempfile.TemporaryDirectory() as d:
            img = Path(d) / "fake.ino.bin"
            with open(img, "wb") as f:
                f.truncate(size)
            r = subprocess.run([sys.executable, str(SCRIPT), str(img), str(SLOT)], capture_output=True, text=True)
        return size, r.returncode, r.stdout + r.stderr

    def test_94_percent_passes_and_logs_the_numbers(self):
        size, rc, out = self.run_check(94)
        self.assertEqual(rc, 0, out)
        self.assertIn("%d B of %d B" % (size, SLOT), out)
        self.assertIn("free %d B" % (SLOT - size), out)
        self.assertNotIn("WARN", out)
        self.assertNotIn("FAIL", out)

    def test_96_percent_warns_and_continues(self):
        _, rc, out = self.run_check(96)
        self.assertEqual(rc, 0, out)
        self.assertIn("WARN", out)

    def test_99_percent_fails(self):
        _, rc, out = self.run_check(99)
        self.assertEqual(rc, 1, out)
        self.assertIn("FAIL", out)

    def test_exactly_98_percent_still_passes_with_a_warning(self):
        # "above 98 %" fails; 98.0 % itself warns
        _, rc, out = self.run_check(98)
        self.assertEqual(rc, 0, out)
        self.assertIn("WARN", out)


class BuildPyCallsTheCheck(unittest.TestCase):
    def test_build_py_runs_the_check_on_the_app_image_with_the_ota0_slot(self):
        src = BUILD_PY.read_text(encoding="utf-8")
        self.assertIn('"check_app_size.py"', src)
        call = src[src.index('"check_app_size.py"') - 1500:src.index('"check_app_size.py"') + 600]
        self.assertIn("fw_dest", call, "the check runs on the collected .ino.bin")
        self.assertRegex(call, r"read_partition_table\(project_dir\)\.get\(\"app0\"", "slot size from the CSV (app0 = ota_0)")
        self.assertRegex(call, r"sys\.exit\(1\)", "a FAIL stops the build")


if __name__ == "__main__":
    unittest.main(verbosity=2)
