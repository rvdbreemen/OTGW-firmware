#!/usr/bin/env python3
"""TASK-1950: OT-Direct drops the setback fields that have no PIC counterpart; SB= and FS= stay like the PIC.

Since TASK-1806 OT-Direct has no setback on a thermostat loss, and since TASK-1802 bug 9 it detects the loss after a
fixed 3 s. Decision (coordinator under Robert's delegation, 2026-11-15, option A): remove what has no PIC counterpart
(state.otd.bSetbackActive / otSetbackEngaged and every REST field and web UI row that showed it; the
OTDsetbacktimeout setting / iSetbackTimeout and its REST fields), keep SB= and FS=: they are PIC commands
(gateway.asm 6.6 SetSetBack :4124, PR=S; SetFailSafety :5252-5259, saved with SaveConfig), stored and answered like
the PIC, with no effect on OT-Direct.

Settings migration: an old settings.ini with OTDsetbacktimeout keeps loading. updateSetting() has no branch for the
key, so the cascade leaves the struct unchanged and the no-op path returns (platformSettingsNoopUnchanged); the next
save no longer writes the key. Checked here on the source, the same way as tests/test_sat_autotune_removed.py.

    python tests/test_otd_setback_fields_removed.py                    # the working tree
    OTGW_SRC_REV=<sha> python tests/test_otd_setback_fields_removed.py  # another revision (old vs fix)
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = "src/OTGW-firmware/"
REV = os.environ.get("OTGW_SRC_REV")


def read(name):
    if REV is None:
        return (REPO / (FW + name)).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{FW}{name}"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=True).stdout.replace("\r\n", "\n")


FILES = ["OTDirect.ino", "OTDirecttypes.h", "restAPI.ino", "settingStuff.ino", "settingsHelp.h", "handleDebug.ino",
         "data/v2.js"]


def first_hit(rx, s):
    m = re.search(rx, s)
    if not m:
        return None
    line = s.count("\n", 0, m.start()) + 1
    return f"line {line}: {s.splitlines()[line - 1].strip()[:120]}"


class Removed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = {f: read(f) for f in FILES}

    def test_no_setback_active_state(self):
        for f, s in self.src.items():
            with self.subTest(file=f):
                self.assertIsNone(first_hit(r"\bbSetbackActive\b|\botSetbackEngaged\b|\bsetback_active\b", s))

    def test_no_setback_timeout_setting(self):
        for f, s in self.src.items():
            with self.subTest(file=f):
                self.assertIsNone(first_hit(r"\biSetbackTimeout\b|(?i:\botdsetbacktimeout\b)|\bsetback_timeout\b", s))

    def test_no_otdsetback_device_info_field(self):
        self.assertNotIn('F("otdsetback")', self.src["restAPI.ino"])

    def test_web_ui_has_no_setback_row(self):
        self.assertNotIn("otd_row_setback", self.src["data/v2.js"])


class OldSettingsLoad(unittest.TestCase):
    def test_an_old_otdsetbacktimeout_key_is_ignored_without_an_error(self):
        src = read("settingStuff.ino")
        body = src[src.index("void updateSetting("):src.index("} // updateSetting()")]
        self.assertNotRegex(body, r"(?i)OTDsetbacktimeout", "no branch: the key falls through the cascade")
        self.assertNotRegex(body, r"(?i)unknown (setting|key|field)", "an unknown key is not reported as an error")
        self.assertIn("platformSettingsNoopUnchanged(&settings, sizeof(settings))", body,
                      "an unchanged struct returns through the no-op path")
        self.assertNotRegex(src, r'F\("OTDsetbacktimeout"\)', "the next save drops the key")


class PicCommandsStay(unittest.TestCase):
    """SB= and FS= are PIC commands: they stay stored and answered (PIC parity)."""

    @classmethod
    def setUpClass(cls):
        cls.otd = read("OTDirect.ino")
        cls.st = read("settingStuff.ino")

    def test_sb_is_parsed_stored_and_printed(self):
        self.assertRegex(self.otd, r"cmd0 == 'S' && cmd1 == 'B'")
        self.assertRegex(self.otd, r"otdPersistFloat\(settings\.otd\.fSetbackTemp, otSetbackTemp\)")
        self.assertRegex(self.otd, r'PSTR\("PR: S=%s"\)')

    def test_fs_is_parsed_and_stored(self):
        self.assertRegex(self.otd, r"cmd0 == 'F' && cmd1 == 'S'")
        self.assertRegex(self.otd, r"otdPersistBool\(settings\.otd\.bFailSafe, otFailSafeEnabled\)")

    def test_their_settings_keys_load_and_save(self):
        for key in ("OTDsetbacktemp", "OTDfailsafe"):
            with self.subTest(key=key):
                self.assertIn(f'PSTR("{key}")', self.st)
                self.assertIn(f'F("{key}")', self.st)


if __name__ == "__main__":
    unittest.main(verbosity=2)
