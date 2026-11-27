#!/usr/bin/env python3
"""TASK-3102 steps 0 and 1 (entry 503, Robert Q12 A; entry 459): the zone count default is 0, a stored value is never changed.

settings.sat.iZoneCount counts the EXTRA rooms. The default moves from 1 to 0 (no extra rooms) and the clamp from 1..4 to 0..4.
A settings file that predates the SATzonecount key has no stored value: that install ran with the old default 1 and keeps 1
(sentinel g_sawSatZoneCountKey, same pattern as TASK-3011). A fresh install writes its in-RAM defaults first, so the key is
present and 0 applies.

Nothing is re-implemented: the test cuts the real SATzonecount branch of updateSetting() and the real migration block
([sat-zonecount-migrate:begin/end]) out of settingStuff.ino and compiles them with a host C++ compiler.
OTGW_SS_INO / OTGW_TYPES_H point at another copy (e.g. `git show origin/dev:...`) for an old-vs-fix run.

Not covered here (text checks only): the readSettings() flow that creates the file for a fresh install (it writes the in-RAM
defaults and re-reads), the REST range, the help text. A settings file that is empty or cannot be opened takes the early returns
with the RAM defaults (0); there is no user data in such a file.

Run: python tests/test_sat_zone_count_default_3102.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = HERE.parent / "src" / "OTGW-firmware"
SS_INO = Path(os.environ.get("OTGW_SS_INO", FW / "settingStuff.ino"))
TYPES_H = Path(os.environ.get("OTGW_TYPES_H", FW / "SATtypes.h"))


def read(p):
    return Path(p).read_text(encoding="utf-8").replace("\r\n", "\n")


MAIN = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#ifdef _MSC_VER
#define strcasecmp_P _stricmp
#else
#include <strings.h>
#define strcasecmp_P strcasecmp
#endif
#define PSTR(x) x
#define F(x) x
template <typename T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void DebugTln(const char*) {}
static bool settingsDirty = false;
static bool g_sawSatZoneCountKey = false;
struct { struct { uint8_t iZoneCount = @DEFAULT@; } sat; } settings;
static void applyField(const char* field, const char* newValue) {
  if (false) {}
@BRANCH@
}
static void migrate() {
@MIGRATE@
}
int main(int argc, char** argv) {
  // argv[1] = '@' (key absent from the file) or the stored text
  if (argv[1][0] != '@') applyField("SATzonecount", argv[1]);
  migrate();
  printf("%d %d\n", (int)settings.sat.iZoneCount, settingsDirty ? 1 : 0);
  return 0;
}
"""


def cut(src):
    m = re.search(r"  else if \(strcasecmp_P\(field, PSTR\(\"SATzonecount\"\)\) == 0\)(.*?)\n  (?=else if|// )", src, re.S)
    branch = "  else if (strcasecmp_P(field, PSTR(\"SATzonecount\")) == 0)" + m.group(1) if m else None
    b = re.search(r"// \[sat-zonecount-migrate:begin\][^\n]*\n(.*?)// \[sat-zonecount-migrate:end\]", src, re.S)
    return branch, (b.group(1) if b else None)


class ZoneCount(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = read(SS_INO)
        types = read(TYPES_H)
        cls.default = int(re.search(r"uint8_t\s+iZoneCount\s*=\s*(\d+);", types).group(1))
        branch, migrate = cut(src)
        cls.has_block = migrate is not None
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(MAIN.replace("@DEFAULT@", str(cls.default)).replace("@BRANCH@", branch or "").replace("@MIGRATE@", migrate or ""))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def load(self, stored):
        out = subprocess.run([str(self.exe), stored], capture_output=True, text=True, check=True).stdout.split()
        return int(out[0]), int(out[1])

    def test_default_is_zero(self):
        self.assertEqual(self.default, 0)

    def test_old_install_without_the_key_keeps_one_and_persists_it(self):
        self.assertTrue(self.has_block, "no [sat-zonecount-migrate] block")
        self.assertEqual(self.load("@"), (1, 1))

    def test_fresh_install_has_the_key_with_the_default(self):
        self.assertEqual(self.load("0"), (0, 0))

    def test_a_stored_value_is_never_migrated(self):
        for stored, want in (("0", 0), ("1", 1), ("2", 2), ("3", 3), ("4", 4)):
            self.assertEqual(self.load(stored), (want, 0), stored)

    def test_clamp_is_zero_to_four(self):
        self.assertEqual(self.load("9"), (4, 0))
        self.assertEqual(self.load("-2"), (0, 0))
        self.assertEqual(self.load("abc"), (0, 0))   # atoi of text is 0, as before


class Wiring(unittest.TestCase):
    """Text checks (not a run)."""

    def test_wiring(self):
        ss = read(SS_INO)
        self.assertIn("g_sawSatZoneCountKey = false;", ss)                      # reset at the start of readSettings()
        self.assertIn('writeJsonIntKV(file, F("SATzonecount"), settings.sat.iZoneCount);', ss)   # every save writes the key
        rest = read(FW / "restAPI.ino")
        self.assertRegex(rest, r'addInt \(F\("satzonecount"\),\s+settings\.sat\.iZoneCount, "i", 0, 4\);')
        help_ = read(FW / "settingsHelp.h")
        self.assertRegex(help_, r'"satzonecount":"[^"]*0 to 4[^"]*"')


if __name__ == "__main__":
    unittest.main()
