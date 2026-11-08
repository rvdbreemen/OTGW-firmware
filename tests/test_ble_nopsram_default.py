#!/usr/bin/env python3
"""TASK-1693: on a board without PSRAM, BLE starts off; a stored SATbleenable always wins.

Two parts:
1. The REAL applyBleNoPsramDefault() block ([ble-nopsram-default] in settingStuff.ino) is compiled
   and called: without PSRAM it switches BLE off, with PSRAM it leaves the struct default (on).
2. Source checks on the REAL readSettings()/updateSetting() wiring, because a host build cannot
   boot LittleFS: the fresh-file branch applies the default before its first writeSettings() (that
   write is what a fresh install keeps), the sentinel is reset per parse and set by the
   SATbleenable setter, and the post-parse step applies the default only when the key was absent
   and the board has no PSRAM. These are structure checks, not a boot: the bench covers behaviour.

OTGW_SETTINGSTUFF_INO points at another copy (old-vs-fix). The old source has none of it: red.

Run: python tests/test_ble_nopsram_default.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

REPO = Path(__file__).resolve().parent.parent
INO = Path(os.environ.get("OTGW_SETTINGSTUFF_INO", REPO / "src" / "OTGW-firmware" / "settingStuff.ino"))


def text():
    return INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def block(s, begin, end):
    i, j = s.find(begin), s.find(end)
    return s[i:j] if i >= 0 and j > i else ""


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


class HelperBehaviour(unittest.TestCase):
    def test_no_psram_switches_ble_off_psram_keeps_it(self):
        blk = block(text(), "// [ble-nopsram-default:begin]", "// [ble-nopsram-default:end]")
        self.assertTrue(blk, "applyBleNoPsramDefault() block not found (no PSRAM-aware BLE default)")
        src = ("#include <cstdio>\n"
               "struct { struct { bool bBleEnable = true; } sat; } settings;\n" + blk +
               "\nint main(){int bad=0;\n"
               " settings.sat.bBleEnable=true; applyBleNoPsramDefault(false);"
               " if (settings.sat.bBleEnable) { std::printf(\"FAIL: no PSRAM left BLE on\\n\"); bad++; }\n"
               " settings.sat.bBleEnable=true; applyBleNoPsramDefault(true);"
               " if (!settings.sat.bBleEnable) { std::printf(\"FAIL: PSRAM board lost BLE\\n\"); bad++; }\n"
               " return bad;}\n")
        with tempfile.TemporaryDirectory() as td:
            cpp = Path(td) / "ble_default.cpp"
            cpp.write_text(src, encoding="utf-8")
            exe = base.compile_cpp(cpp, Path(td) / "ble_default.exe")
            if exe is None:
                self.skipTest("no host C++ compiler")
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout)


class Wiring(unittest.TestCase):
    def setUp(self):
        self.s = text()
        self.read = function_body(self.s, "void readSettings(bool show)")
        self.assertTrue(self.read, "readSettings() not found")

    def test_fresh_install_gets_the_default_before_the_first_write(self):
        fresh = re.search(r"if \(!LittleFS\.exists\(SETTINGS_FILE\)\)\s*\{(.*?)\n  \}", self.read, re.S)
        self.assertIsNotNone(fresh, "fresh-file branch not found")
        body = fresh.group(1)
        i, j = body.find("applyBleNoPsramDefault(psramFound())"), body.find("writeSettings(show)")
        self.assertGreaterEqual(i, 0, "the fresh-file branch does not apply the PSRAM-aware BLE default")
        self.assertLess(i, j, "the default must be applied before the first writeSettings()")

    def test_sentinel_is_reset_per_parse_and_set_by_the_setter(self):
        self.assertIn("g_sawBleEnableKey = false;", self.read, "sentinel not reset in readSettings()")
        self.assertRegex(self.s, r'PSTR\("SATbleenable"\)\) == 0\)\s*\{[^}]*g_sawBleEnableKey = true;',
                         "the SATbleenable setter does not set the sentinel")

    def test_missing_key_without_psram_switches_ble_off(self):
        m = re.search(r"if \(!g_sawBleEnableKey && !psramFound\(\)\) \{(.*?)\}", self.read, re.S)
        self.assertIsNotNone(m, "no post-parse default for a missing SATbleenable key")
        self.assertIn("applyBleNoPsramDefault(false)", m.group(1))
        self.assertIn("settingsDirty = true", m.group(1), "the derived value must be persisted (deferred flush)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
