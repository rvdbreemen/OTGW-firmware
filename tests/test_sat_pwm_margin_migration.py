#!/usr/bin/env python3
"""TASK-3151.3: the continuous -> PWM switch margin has the thermo-nova default 3.0 C; a stored old default (1.5 or 2.0) migrates to 3.0 once.

Robert 2026-11-28 08:57 (the margin stays a user setting, default 3.0; register entry 522) and the coordinator's decision A
(entry 523): since TASK-1888 every settings save writes SATpwmswitchmargin, so a stored 1.5 (the TASK-1888 default) or 2.0
(the TASK-2005 default) is an old default, not a user choice, and cannot be told from a deliberate one: it moves to 3.0 once.
Any other stored value stays. The marker key SATpwmmargin3 tells a migrated file apart (TASK-1961 pattern); a stale
SATpwmmargin2 line of the TASK-2005 migration matches no setter and is ignored.

The code under test is REAL, cut from the firmware sources: the SATpwmswitchmargin and SATpwmmargin3 setter lines of
updateSetting() and their write lines in writeSettings() (settingStuff.ino), clampSettingFloat(), the
"[sat-pwmmargin-migrate:begin]" block of readSettings(), and the fPwmSwitchMargin default of SATtypes.h. The host
program plays the settings file: parse = run the setter lines per stored key on a default-constructed struct, then the
migration block; flush = run the write lines. A host build cannot boot LittleFS; the bench covers the real file.

Cases: (a) 1.5 and no marker -> 3.0, dirty, after the flush the marker is stored and a second boot keeps 3.0 without a
new save; (a2) the same for a stored 2.0; (b) 2.5 and (b2) 4.0 and no marker -> stay (the marker is still written);
(c) the marker and 1.5 or 2.0 (the user set it after the migration) -> stays; (d) a new install -> 3.0 with the marker;
(e) no SATpwmswitchmargin key at all and no marker -> the default 3.0; (f) a file that only has the stale SATpwmmargin2
marker and 2.0 -> 3.0 (the old marker does not count).

Old-vs-fix: OTGW_SETTINGSTUFF_INO / OTGW_SATTYPES_H point at another copy. On the code of TASK-2005 (default 2.0, marker
SATpwmmargin2, only 1.5 migrates) every case goes red (the marker name and the default differ; a, a2, d, e and f also on the values).
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
         hasKey(f, "SATpwmmargin3") ? 1 : 0);
}
int main(int argc, char** argv) {
  std::string c = argv[1];
  File f;
  if (c == "a" || c == "a2" || c == "b" || c == "b2") {
    // written by TASK-1888 .. TASK-3151: every save stored SATpwmswitchmargin, no SATpwmmargin3 marker
    const char* v = c == "a" ? "1.50" : c == "a2" ? "2.00" : c == "b" ? "2.50" : "4.00";
    f = {{"SATpwmswitchmargin", v}};
    readSettings(f); show("boot1", f);
    if (settingsDirty) { f.clear(); writeSettings(f); }
    readSettings(f); show("boot2", f);
  } else if (c == "c" || c == "c2") {
    f = {{"SATpwmswitchmargin", c == "c" ? "1.50" : "2.00"}, {"SATpwmmargin3", "true"}};
    readSettings(f); show("boot1", f);
  } else if (c == "d") {
    settings = Settings(); writeSettings(f);
    readSettings(f); show("boot1", f);
  } else if (c == "e") {
    f = {};   // a settings file without the key, e.g. one from before TASK-1888
    readSettings(f); show("boot1", f);
  } else if (c == "f") {
    f = {{"SATpwmswitchmargin", "2.00"}, {"SATpwmmargin2", "true"}};   // only the stale TASK-2005 marker
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
    setters = lines_with(ss, r'PSTR\("SAT(pwmswitchmargin|pwmmargin\d)"\)\)\s*==\s*0\)')
    writers = lines_with(ss, r'writeJson(Float|Bool)KV\(file, F\("SAT(pwmswitchmargin|pwmmargin\d)"\)')
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
    def test_a_stored_old_default_1_5_migrates_to_3_once(self):
        r = run("a")
        self.assertEqual(r["boot1"], {"m": 3.0, "dirty": 1, "marker": 0}, "first boot: 3.0 and a deferred save")
        self.assertEqual(r["boot2"], {"m": 3.0, "dirty": 0, "marker": 1}, "after the save: marker stored, no repeat")

    def test_a2_stored_old_default_2_0_migrates_to_3_once(self):
        r = run("a2")
        self.assertEqual(r["boot1"], {"m": 3.0, "dirty": 1, "marker": 0})
        self.assertEqual(r["boot2"], {"m": 3.0, "dirty": 0, "marker": 1})

    def test_b_a_user_value_stays(self):
        r = run("b")
        self.assertEqual(r["boot1"]["m"], 2.5)
        self.assertEqual(r["boot2"], {"m": 2.5, "dirty": 0, "marker": 1})

    def test_b2_a_value_above_the_default_stays(self):
        r = run("b2")
        self.assertEqual(r["boot1"]["m"], 4.0)
        self.assertEqual(r["boot2"], {"m": 4.0, "dirty": 0, "marker": 1})

    def test_c_a_value_set_after_the_migration_stays(self):
        self.assertEqual(run("c")["boot1"], {"m": 1.5, "dirty": 0, "marker": 1})
        self.assertEqual(run("c2")["boot1"], {"m": 2.0, "dirty": 0, "marker": 1})

    def test_d_new_install_gets_3(self):
        self.assertEqual(run("d")["boot1"], {"m": 3.0, "dirty": 0, "marker": 1})

    def test_e_a_file_without_the_key_gets_the_default_3(self):
        self.assertEqual(run("e")["boot1"]["m"], 3.0)

    def test_f_the_stale_marker_of_task_2005_does_not_count(self):
        self.assertEqual(run("f")["boot1"], {"m": 3.0, "dirty": 1, "marker": 0})


if __name__ == "__main__":
    unittest.main(verbosity=2)
