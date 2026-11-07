#!/usr/bin/env python3
"""TASK-1388: flash_otgw --migrate checks settings.ini per key, not byte for byte.

A migrate across a version jump reported '[ERROR] settings.ini on the unit differs from the saved copy'
although every setting was kept: the new firmware reads the old file and writes it back in its own format
(new keys, defaults, number precision). Now every saved key must come back with the same value (numbers
compared as numbers), new keys are allowed, and a real loss names the keys (never the values).

The test runs the real scripts through their --compare-settings entry: flash_otgw.bat via cmd and
flash_otgw.sh via bash. Synthetic settings only.

Old-vs-fix: OTGW_FLASH_DIR points at a directory with older copies of both scripts (they have no
--compare-settings entry, so the old byte compare is reproduced by the test's own fc/cmp check below).

Run: python tests/test_flash_migrate_settings_compare.py
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DIR = Path(os.environ.get("OTGW_FLASH_DIR", REPO))

SAVED = """{
  "hostname": "OTGW",
  "httppasswd": "secret-old",
  "MQTTbroker": "192.168.1.10",
  "MQTTbrokerPort": 1883,
  "SATcoefficient": 0.35,
  "SATenabled": true
}
"""
# Same settings rewritten by a newer firmware: other order, more precision, a new key.
AFTER_SAME = """{
  "hostname": "OTGW",
  "httppasswd": "secret-old",
  "SATenabled": true,
  "SATcoefficient": 0.349999994,
  "MQTTbroker": "192.168.1.10",
  "MQTTbrokerPort": 1883,
  "SATcurveselftune": true
}
"""
# TASK-1684: the saved file comes from a firmware that still had the OT-Thing controller settings; the new
# firmware does not write them back. A retired key may be missing, a real key may not.
SAVED_RETIRED = SAVED.replace('  "SATenabled": true\n', '  "SATenabled": true,\n  "OTDchmode": 1,\n  "OTDflowtemp": 45.0,\n  "OTDhysteresisenable": false\n')
AFTER_LOST = AFTER_SAME.replace('"MQTTbroker": "192.168.1.10",\n', "").replace('"httppasswd": "secret-old"', '"httppasswd": "secret-new"')


def bash():
    for c in (shutil.which("bash"), r"C:\Program Files\Git\bin\bash.exe"):
        if c and Path(c).exists():
            return c
    return None


class SettingsCompare(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="cmpset-"))
        (self.tmp / "saved.ini").write_text(SAVED)
        (self.tmp / "same.ini").write_text(AFTER_SAME)
        (self.tmp / "lost.ini").write_text(AFTER_LOST)
        (self.tmp / "saved_retired.ini").write_text(SAVED_RETIRED)
        (self.tmp / "lost_retired.ini").write_text(AFTER_LOST)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_bat(self, after, saved="saved.ini"):
        r = subprocess.run(["cmd", "/c", str(DIR / "flash_otgw.bat"), "--compare-settings",
                            str(self.tmp / saved), str(self.tmp / after)], capture_output=True, text=True)
        return r.returncode, r.stdout + r.stderr

    def run_sh(self, after, saved="saved.ini"):
        r = subprocess.run([bash(), str(DIR / "flash_otgw.sh"), "--compare-settings",
                            (self.tmp / saved).as_posix(), (self.tmp / after).as_posix()],
                           capture_output=True, text=True)
        return r.returncode, r.stdout + r.stderr

    def check_same_ok(self, rc, out):
        self.assertEqual(rc, 0, out)
        self.assertIn("[OK] Settings restored and verified", out)

    def check_loss_named(self, rc, out):
        self.assertEqual(rc, 1, out)
        self.assertIn("MQTTbroker (missing)", out)
        self.assertIn("httppasswd", out)
        self.assertNotIn("secret", out, "a value was printed")

    @unittest.skipUnless(os.name == "nt", "cmd only on Windows")
    def test_bat_version_jump_with_all_settings_kept_is_ok(self):
        self.check_same_ok(*self.run_bat("same.ini"))

    @unittest.skipUnless(os.name == "nt", "cmd only on Windows")
    def test_bat_real_loss_names_the_keys(self):
        self.check_loss_named(*self.run_bat("lost.ini"))

    @unittest.skipUnless(bash(), "no bash")
    def test_sh_version_jump_with_all_settings_kept_is_ok(self):
        self.check_same_ok(*self.run_sh("same.ini"))

    @unittest.skipUnless(bash(), "no bash")
    def test_sh_real_loss_names_the_keys(self):
        self.check_loss_named(*self.run_sh("lost.ini"))

    @unittest.skipUnless(os.name == "nt", "cmd only on Windows")
    def test_bat_retired_keys_may_be_missing(self):
        self.check_same_ok(*self.run_bat("same.ini", "saved_retired.ini"))

    @unittest.skipUnless(os.name == "nt", "cmd only on Windows")
    def test_bat_retired_keys_do_not_hide_a_real_loss(self):
        rc, out = self.run_bat("lost.ini", "saved_retired.ini")
        self.check_loss_named(rc, out)
        self.assertNotIn("OTDchmode", out)

    @unittest.skipUnless(bash(), "no bash")
    def test_sh_retired_keys_may_be_missing(self):
        self.check_same_ok(*self.run_sh("same.ini", "saved_retired.ini"))

    @unittest.skipUnless(bash(), "no bash")
    def test_sh_retired_keys_do_not_hide_a_real_loss(self):
        rc, out = self.run_sh("lost.ini", "saved_retired.ini")
        self.check_loss_named(rc, out)
        self.assertNotIn("OTDchmode", out)

    def test_old_byte_compare_would_have_reported_the_kept_settings(self):
        # the repro of the report: byte for byte the rewritten file differs although nothing was lost
        self.assertNotEqual((self.tmp / "saved.ini").read_bytes(), (self.tmp / "same.ini").read_bytes())


if __name__ == "__main__":
    unittest.main(verbosity=2)
