#!/usr/bin/env python3
"""TASK-2005: the continuous -> PWM switch margin moves to a 2.0 C default; a stored old default 1.5 migrates to 2.0 once.

Sergeant D entry 407 (2 C for 60 s instead of 1.5 C for 30 s); Q-407a answered by Robert (maintainer, in session): A, 2.0
is the new default of the existing setting SATpwmswitchmargin. Since TASK-1888 every settings save writes
SATpwmswitchmargin, so a stored 1.5 is the old default, not a user choice (our reading, recorded in entry 407): it moves
to 2.0 once. Any other stored value stays. The marker key SATpwmmargin2 tells a migrated file apart (TASK-1961 pattern).

The code under test is REAL, cut from the firmware sources: the SATpwmswitchmargin and SATpwmmargin2 setter lines of
updateSetting() and their write lines in writeSettings() (settingStuff.ino), clampSettingFloat(), the
"[sat-pwmmargin-migrate:begin]" block of readSettings(), and the fPwmSwitchMargin default of SATtypes.h. The host
program plays the settings file: parse = run the setter lines per stored key on a default-constructed struct, then the
migration block; flush = run the write lines. A host build cannot boot LittleFS; the bench covers the real file.

Cases: (a) a file with 1.5 and no marker -> 2.0, dirty, after the flush the marker is stored and a second boot keeps 2.0
without a new save; (b) a file with 2.5 and no marker -> 2.5 stays (the marker is still written); (c) a file with the
marker and 1.5 (the user set 1.5 after the migration) -> stays 1.5; (d) a new install -> 2.0.

Old-vs-fix: OTGW_SETTINGSTUFF_INO / OTGW_SATTYPES_H point at another copy. The code before TASK-2005 has default 1.5 and
no migration: (a) and (d) go red.
Run: python tests/test_sat_pwm_margin_migration.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SS = Path(os.environ.get("OTGW_SETTINGSTUFF_INO", REPO / "src" / "OTGW-firmware" / "settingStuff.ino"))
TYPES = Path(os.environ.get("OTGW_SATTYPES_H", REPO / "src" / "OTGW-firmware" / "SATtypes.h"))


def read(p):
    return p.read_text(encoding="utf-8").replace("\r\n", "\n")


def lines_with(src, pattern):
    return "\n".join(l for l in src.split("\n") if re.search(pattern, l))


PRE = r"""
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#define PSTR(s) (s)
#define F(s) (s)
#define strcasecmp_P strcasecmp
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
using std::isfinite;
static void DebugTln(const char*) {}
static void DebugTf(const char*, ...) {}
struct Settings { struct { @DEFAULT@ } sat; };
static Settings settings;
static bool settingsDirty = false;
static bool g_sawSatPwmMarginMarkerKey = false;
static constexpr bool kSatPwmMarginMarkerValue = true;
typedef std::vector<std::pair<std::string, std::string>> File;
static void writeJsonBoolKV(File& f, const char* k, bool v) { f.push_back({k, v ? "true" : "false"}); }
static void writeJsonFloatKV(File& f, const char* k, float v, int = 2) {
  char b[16]; snprintf(b, sizeof b, "%.2f", v); f.push_back({k, b});
}
@CLAMP@
static void updateSetting(const char* field, const char* newValue) {
  if (false) {}
@SETTERS@
}
static void writeSettings(File& file) {
@WRITERS@
}
static void readSettings(const File& file) {
  settings = Settings();
  settingsDirty = false;
  g_sawSatPwmMarginMarkerKey = false;
  for (auto& kv : file) updateSetting(kv.first.c_str(), kv.second.c_str());
@MIGRATE@
}
static bool hasKey(const File& f, const char* k) { for (auto& kv : f) if (kv.first == k) return true; return false; }
static void show(const char* tag, const File& f) {
  printf("%s m=%.2f dirty=%d marker=%d\n", tag, settings.sat.fPwmSwitchMargin, settingsDirty ? 1 : 0,
         hasKey(f, "SATpwmmargin2") ? 1 : 0);
}
int main(int argc, char** argv) {
  std::string c = argv[1];
  File f;
  if (c == "a" || c == "b") {
    // written by TASK-1888 .. beta.360: every save stored SATpwmswitchmargin, no marker
    f = {{"SATovershootmargin", "2.00"}, {"SATpwmswitchmargin", c == "a" ? "1.50" : "2.50"}};
    readSettings(f); show("boot1", f);
    if (settingsDirty) { f.clear(); writeSettings(f); }
    readSettings(f); show("boot2", f);
  } else if (c == "c") {
    f = {{"SATpwmswitchmargin", "1.50"}, {"SATpwmmargin2", "true"}};
    readSettings(f); show("boot1", f);
  } else if (c == "d") {
    settings = Settings(); writeSettings(f);
    readSettings(f); show("boot1", f);
  }
  return 0;
}
"""


def build():
    ss, ty = read(SS), read(TYPES)
    m = re.search(r"^\s*float\s+fPwmSwitchMargin\s*=\s*([0-9.]+)f\s*;", ty, re.M)
    assert m, "fPwmSwitchMargin default not found in SATtypes.h"
    clamp = re.search(r"^static float clampSettingFloat\(.*?^}\n", ss, re.S | re.M)
    assert clamp, "clampSettingFloat not found"
    setters = lines_with(ss, r'PSTR\("SAT(pwmswitchmargin|pwmmargin2)"\)\)\s*==\s*0\)')
    writers = lines_with(ss, r'writeJson(Float|Bool)KV\(file, F\("SAT(pwmswitchmargin|pwmmargin2)"\)')
    assert "SATpwmswitchmargin" in setters and "SATpwmswitchmargin" in writers, "setter / writer not found"
    blk = re.search(r"// \[sat-pwmmargin-migrate:begin\][^\n]*\n(.*?)// \[sat-pwmmargin-migrate:end\]", ss, re.S)
    return (PRE.replace("@DEFAULT@", "float fPwmSwitchMargin = %sf;" % m.group(1)).replace("@CLAMP@", clamp.group(0))
               .replace("@SETTERS@", setters).replace("@WRITERS@", writers)
               .replace("@MIGRATE@", blk.group(1) if blk else "  // no migration block in this source"))


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="sat_pwm_mig_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(build(), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no host C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(case):
    out = subprocess.run([str(_exe), case], capture_output=True, text=True, check=True).stdout
    res = {}
    for line in out.splitlines():
        tag, *kv = line.split()
        res[tag] = {k: (float(v) if k == "m" else int(v)) for k, v in (x.split("=") for x in kv)}
    return res


class PwmMarginMigration(unittest.TestCase):
    def test_a_stored_old_default_1_5_migrates_to_2_once(self):
        r = run("a")
        self.assertEqual(r["boot1"], {"m": 2.0, "dirty": 1, "marker": 0}, "first boot: 2.0 and a deferred save")
        self.assertEqual(r["boot2"], {"m": 2.0, "dirty": 0, "marker": 1}, "after the save: marker stored, no repeat")

    def test_b_a_user_value_stays(self):
        r = run("b")
        self.assertEqual(r["boot1"]["m"], 2.5)
        self.assertEqual(r["boot2"], {"m": 2.5, "dirty": 0, "marker": 1})

    def test_c_1_5_set_after_the_migration_stays(self):
        self.assertEqual(run("c")["boot1"], {"m": 1.5, "dirty": 0, "marker": 1})

    def test_d_new_install_gets_2(self):
        self.assertEqual(run("d")["boot1"], {"m": 2.0, "dirty": 0, "marker": 1})


if __name__ == "__main__":
    unittest.main(verbosity=2)
