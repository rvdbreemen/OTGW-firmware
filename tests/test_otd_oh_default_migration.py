#!/usr/bin/env python3
"""TASK-1961: OH= defaults to 1 like the PIC; settings from before this release migrate to 1 once.

PIC ground truth (OTGW-other-projects/otgw-6.8 gateway.asm, repo 770aa45): SavedSettings :5459 sets OverrideHigh
(settings bit 6, :512) as the factory default, so a PIC copies the MsgID 100 override flags into the high byte
(MessageID100 :3303-3311) unless OH=0 was sent.

Robert chose B (DM 1571441135526875227): every unit goes to OH=1 once, with a persistent marker so it never repeats;
an OH=0 set afterwards stays. Since TASK-1836 (beta.203) every settings save writes OTDoverridehb, so a stored 0 is
not a user choice; the marker key OTDohpicdefault tells a migrated file apart.

The code under test is REAL, cut from the firmware sources: the OTDoverridehb and OTDohpicdefault setter lines of
updateSetting() and their writeJsonBoolKV() lines in writeSettings() (settingStuff.ino), the
"[otd-oh-migrate:begin]" block of readSettings(), and the bOverrideHB default of OTDirecttypes.h. The host program
plays the settings file: parse = run the setter lines per stored key on a default-constructed struct, then the
migration block; flush = run the write lines. A host build cannot boot LittleFS; the bench covers the real file.

Cases: (a) a file with OTDoverridehb=false and no marker -> OH=1, settingsDirty, and after the flush the marker is
stored; (b) a file with the marker and OTDoverridehb=false -> OH stays 0, nothing dirty; (c) a new install (no file:
defaults, then writeSettings, then a re-read) -> OH=1; (d) after (a) the user sends OH=0 -> it stays 0 after a reboot.

Old-vs-fix: OTGW_SETTINGSTUFF_INO / OTGW_OTD_TYPES point at another copy. The code before TASK-1961 has no marker and
default 0: (a), (c) and (d) go red.
Run: python tests/test_otd_oh_default_migration.py
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
TYPES = Path(os.environ.get("OTGW_OTD_TYPES", REPO / "src" / "OTGW-firmware" / "OTDirecttypes.h"))


def read(p):
    return p.read_text(encoding="utf-8").replace("\r\n", "\n")


def lines_with(src, pattern):
    return "\n".join(l for l in src.split("\n") if re.search(pattern, l))


PRE = r"""
#include <cstdio>
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
#define EVALBOOLEAN(v) (strcasecmp((v), "true") == 0 || strcmp((v), "1") == 0)
static void DebugTln(const char*) {}
struct Settings { struct { @OH_DEFAULT@ } otd; };
static Settings settings;
static bool settingsDirty = false;
static bool g_sawOtdOhMarkerKey = false;
static constexpr bool kOtdOhMarkerValue = true;   // its presence counts, not its value
typedef std::vector<std::pair<std::string, std::string>> File;   // the settings file: key, value in write order
static void writeJsonBoolKV(File& f, const char* k, bool v) { f.push_back({k, v ? "true" : "false"}); }

static void updateSetting(const char* field, const char* newValue) {
  if (false) {}
@SETTERS@
}
static void writeSettings(File& file) {
@WRITERS@
}
static void readSettings(const File& file) {   // the parse, then the post-parse step
  settings = Settings();
  settingsDirty = false;
  g_sawOtdOhMarkerKey = false;
  for (auto& kv : file) updateSetting(kv.first.c_str(), kv.second.c_str());
@MIGRATE@
}
static bool hasKey(const File& f, const char* k) { for (auto& kv : f) if (kv.first == k) return true; return false; }
static void show(const char* tag, const File& f) {
  printf("%s oh=%d dirty=%d marker=%d\n", tag, settings.otd.bOverrideHB ? 1 : 0, settingsDirty ? 1 : 0,
         hasKey(f, "OTDohpicdefault") ? 1 : 0);
}
int main(int argc, char** argv) {
  std::string c = argv[1];
  File f;
  if (c == "a" || c == "d") {
    f = {{"OTDignoretransitions", "true"}, {"OTDoverridehb", "false"}};   // written by beta.203 .. this release
    readSettings(f); show("boot1", f);
    if (settingsDirty) { f.clear(); writeSettings(f); }                  // the deferred flushSettings()
    if (c == "d") { updateSetting("OTDoverridehb", "false"); f.clear(); writeSettings(f); }   // OH=0 (otdPersistBool)
    readSettings(f); show("boot2", f);
  } else if (c == "b") {
    f = {{"OTDoverridehb", "false"}, {"OTDohpicdefault", "true"}};
    readSettings(f); show("boot1", f);
  } else if (c == "c") {                         // no settings.ini: defaults, writeSettings(), then the re-read
    settings = Settings(); writeSettings(f);
    readSettings(f); show("boot1", f);
  }
  return 0;
}
"""


def build():
    ss, ty = read(SS), read(TYPES)
    m = re.search(r"^\s*bool\s+bOverrideHB\s*=\s*(true|false)\s*;", ty, re.M)
    assert m, "bOverrideHB default not found in OTDirecttypes.h"
    setters = lines_with(ss, r'PSTR\("OTD(overridehb|ohpicdefault)"\)\)\s*==\s*0\)')
    writers = lines_with(ss, r'writeJsonBoolKV\(file, F\("OTD(overridehb|ohpicdefault)"\)')
    assert "OTDoverridehb" in setters and "OTDoverridehb" in writers, "OTDoverridehb setter / writer not found"
    blk = re.search(r"// \[otd-oh-migrate:begin\][^\n]*\n(.*?)// \[otd-oh-migrate:end\]", ss, re.S)
    return (PRE.replace("@OH_DEFAULT@", "bool bOverrideHB = %s;" % m.group(1))
               .replace("@SETTERS@", setters).replace("@WRITERS@", writers)
               .replace("@MIGRATE@", blk.group(1) if blk else "  // no migration block in this source"))


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="otd_oh_mig_"))
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
        res[tag] = {k: int(v) for k, v in (x.split("=") for x in kv)}
    return res


class OhDefaultMigration(unittest.TestCase):
    def test_a_old_file_with_oh0_and_no_marker_migrates_to_1_once(self):
        r = run("a")
        self.assertEqual(r["boot1"], {"oh": 1, "dirty": 1, "marker": 0}, "first boot: OH=1 and a deferred save")
        self.assertEqual(r["boot2"], {"oh": 1, "dirty": 0, "marker": 1}, "after the save: marker stored, no repeat")

    def test_b_marker_and_oh0_stays_0(self):
        self.assertEqual(run("b")["boot1"], {"oh": 0, "dirty": 0, "marker": 1})

    def test_c_new_install_gets_1(self):
        self.assertEqual(run("c")["boot1"], {"oh": 1, "dirty": 0, "marker": 1})

    def test_d_oh0_after_the_migration_stays_0(self):
        self.assertEqual(run("d")["boot2"], {"oh": 0, "dirty": 0, "marker": 1})


if __name__ == "__main__":
    unittest.main(verbosity=2)
