#!/usr/bin/env python3
"""TASK-1920: the filesystem image ships no settings.ini, so a fresh unit starts on the firmware's own defaults.

Before: src/OTGW-firmware/data/settings.ini (14 keys) went into the LittleFS image (build.py stage_data_dir() copies
data/ whole). A unit that booted on it took the 1.x-upgrade path: MQTT off, legacy OT topic names (no
MQTTuseLegacyOtTopics key, settingStuff.ino readSettings()) and no setup wizard (no ui_onboarded key). Decision A
(coordinator under Robert's delegation, 2026-11-15): take the file out of the image.

Checks, on the real sources:
1. data/ holds no settings.ini, and build.py adds none to the staging copy of data/;
2. readSettings() handles a missing file by writing the defaults (writeSettings) and reading them back, and
   writeSettings() writes MQTTenable, MQTTuseLegacyOtTopics and ui_onboarded, so the re-read sees the keys whose
   absence means a 1.x upgrade: the first boot keeps the struct defaults;
3. those struct defaults are the "fresh unit" values: MQTT on, new topic names, wizard not yet done.
Red on the code before TASK-1920 (check 1). Run: python tests/test_fs_image_settings_defaults.py
"""
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"


def text(p):
    return p.read_text(encoding="utf-8").replace("\r\n", "\n")


def function_body(s, signature):
    i = s.find(signature)
    if i < 0:
        return ""
    j, depth = s.index("{", i), 0
    for k in range(j, len(s)):
        depth += s[k] == "{"
        depth -= s[k] == "}"
        if depth == 0:
            return s[i:k + 1]
    return ""


class ImageHasNoSettings(unittest.TestCase):
    def test_1_no_settings_ini_in_data_or_staging(self):
        self.assertFalse((SRC / "data" / "settings.ini").exists(),
                         "src/OTGW-firmware/data/settings.ini would ship in the LittleFS image (TASK-1920)")
        build = text(REPO / "build.py")
        m = re.search(r"\ndef stage_data_dir\(.*?(?=\ndef )", build, re.S)
        self.assertIsNotNone(m, "build.py stage_data_dir() not found")
        self.assertNotIn("settings.ini", m.group(0), "stage_data_dir() must not add a settings.ini to the image")

    def test_2_first_boot_writes_and_rereads_the_defaults(self):
        ss = text(SRC / "settingStuff.ino")
        read = function_body(ss, "void readSettings(bool show)")
        fresh = re.search(r"if \(!LittleFS\.exists\(SETTINGS_FILE\)\)\s*\{(.*?)\n  \}", read, re.S)
        self.assertIsNotNone(fresh, "readSettings(): the missing-file branch is gone")
        body = fresh.group(1)
        w, r = body.find("writeSettings("), body.find("readSettings(false)")
        self.assertTrue(0 <= w < r, "a missing settings.ini must be written from the defaults, then read back")
        write = function_body(ss, "bool writeSettings(bool show)")
        for key in ("MQTTenable", "MQTTuseLegacyOtTopics", "ui_onboarded"):
            self.assertIn('F("%s")' % key, write, key + " must be written, else the re-read takes the 1.x-upgrade path")

    def test_3_struct_defaults_are_the_fresh_unit_values(self):
        mq = text(SRC / "MQTTstuff.h")
        self.assertRegex(mq, r"\n  bool\s+bEnable\s*=\s*true;", "MQTT on by default")
        self.assertRegex(mq, r"\n  bool\s+bUseLegacyOtTopics\s*=\s*false;", "new topic names by default")
        self.assertRegex(text(SRC / "UItypes.h"), r"\n  bool bOnboarded\s*=\s*false;", "the setup wizard runs on a fresh unit")


if __name__ == "__main__":
    unittest.main(verbosity=2)
