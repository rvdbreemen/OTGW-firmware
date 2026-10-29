#!/usr/bin/env python3
"""TASK-1487: the SAT preset and the window target survive a reboot, and an unrelated settings save
never stores a preset temperature as the user's own target.

thermo-nova restores the preset, the pre-custom and the pre-activity temperature after a restart
(climate.py:187-188, :190-191, :193-194) and the target the climate had (climate.py:176-182). A
manual temperature that matches no preset clears the preset (climate.py:567). preset_mode none puts
the pre-custom temperature back (climate.py:614-616).

Before the fix the firmware kept eActivePreset / fPreCustomTemp / fPreWindowTarget in RAM only and
wrote settings.sat.fTargetTemp (the preset value while a preset is active) as SATtargettemp in
settings.ini. After a reboot the preset was gone and the preset temperature had become the user's
own target.

The code under test is NOT re-implemented. The test cuts out of src/OTGW-firmware/SATcontrol.ino:
satGetPresetName, satHandlePreset, satHandleWindow, _satCheckWindowTimer and (when present) the
TASK-1487 helpers between the "satPresetPersist begin/end" markers. From settingStuff.ino it takes
the expression writeSettings() stores as SATtargettemp, clampSettingFloat() and the SATtargettemp
branch of updateSetting(). A "reboot" resets state, loads settings.sat.fTargetTemp from that stored
expression (readSettings) and, when the restore helper exists, applies the saved preset file
contents (initSAT). On HEAD there is no restore helper, which is exactly what HEAD does at boot.

OTGW_SAT_INO / OTGW_SETTINGS_INO point at other copies (e.g. `git show <rev>:<path>`) for
old-vs-fix runs.

Run: python tests/test_sat_preset_persist.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
SETTINGS_INO = Path(os.environ.get("OTGW_SETTINGS_INO", REPO / "src" / "OTGW-firmware" / "settingStuff.ino"))

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define FPSTR(s) (s)
typedef const char* PGM_P;
#define strcasecmp_P strcasecmp
#define strcmp_P strcmp
#define snprintf_P snprintf
#define SATDebugTf(...) do {} while (0)
#define SATDebugTln(...) do {} while (0)
#define DebugTf(...) do {} while (0)
template <class T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
enum SATPreset : uint8_t {
  SAT_PRESET_NONE = 0, SAT_PRESET_AWAY, SAT_PRESET_ECO,
  SAT_PRESET_COMFORT, SAT_PRESET_SLEEP, SAT_PRESET_ACTIVITY,
  SAT_PRESET_HOME
};
struct SatSettings {
  float fTargetTemp = 20.0f;
  float fPresetAway = 15.0f, fPresetEco = 18.0f, fPresetComfort = 21.5f, fPresetSleep = 17.0f,
        fPresetActivity = 16.0f, fPresetHome = 20.5f;
  bool bWindowDetection = true;
  uint16_t iWindowMinOpenSec = 60;
  bool bPresetSync = false;
  char sPresetSyncTopic[8] = "";
};
struct SettingsT { SatSettings sat; };
struct SatState {
  SATPreset eActivePreset = SAT_PRESET_NONE;
  float fPreCustomTemp = 0.0f, fPreWindowTarget = 0.0f, fPreActivityTemp = 0.0f, fPidI = 0.0f;
  uint8_t iPreWindowPreset = 0;
  bool bWindowOpen = false;
  uint32_t iWindowOpenSinceMs = 0;
};
struct MqttState { bool bConnected = false; };
struct StateT { SatState sat; MqttState mqtt; };
static SettingsT settings;
static StateT state;
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static void satResetIntegral() {}
static void satNarrate_P(PGM_P) {}
static void sendMQTTData(const char*, const char*, bool) {}
static char* dtostrf(double v, signed char w, unsigned char p, char* b) { sprintf(b, "%*.*f", w, p, v); return b; }
static void satSyncPresetTopic(SATPreset) {}
"""

MAIN = r"""
#ifndef HAVE_PRESET_PERSIST
static size_t satPresetStateFormat(char* buf, size_t n) { if (n) buf[0] = 0; return 0; }   // HEAD: no file
static bool satPresetStateApply(const char*) { return false; }                             // HEAD: no restore
#endif
static float storedTarget() { return (float)(STORED_TARGET_EXPR); }   // what writeSettings() stores
static void reboot() {
  char buf[128];
  satPresetStateFormat(buf, sizeof buf);
  float stored = storedTarget();
  state = StateT();                              // RAM state is gone
  settings.sat.fTargetTemp = stored;             // readSettings()
  satPresetStateApply(buf);                      // initSAT()
}
static void show(const char* tag) {
  printf("%s preset=%d target=%.2f stored=%.2f\n", tag, (int)state.sat.eActivePreset,
         settings.sat.fTargetTemp, storedTarget());
}
static void windowSwitch() {               // open, and stay open past the minimum time
  satHandleWindow(true);
  g_ms += 61000;
  _satCheckWindowTimer();
}
int main(int argc, char** argv) {
  const char* s = argv[1];
  settings.sat.fTargetTemp = 20.0f;
  if (!strcmp(s, "preset")) {
    satHandlePreset("comfort"); show("before");
    reboot(); show("after");
    satHandlePreset("none"); show("none");
  } else if (!strcmp(s, "window")) {
    windowSwitch(); show("before");
    reboot(); show("after");
    satHandleWindow(false); show("closed");
  } else if (!strcmp(s, "preset_window")) {
    satHandlePreset("eco"); windowSwitch(); show("before");
    reboot(); show("after");
    satHandleWindow(false); show("closed");
    satHandlePreset("none"); show("none");
  } else if (!strcmp(s, "window_none")) {
    satHandlePreset("eco"); windowSwitch();
    satHandlePreset("none"); show("none");
    g_ms += 61000; _satCheckWindowTimer(); show("rearm");
    satHandleWindow(false); show("closed");
    reboot(); show("after");
  } else if (!strcmp(s, "window_comfort")) {
    satHandlePreset("eco"); windowSwitch();
    satHandlePreset("comfort"); show("comfort");
    satHandleWindow(false); show("closed");
    reboot(); show("after");
  } else if (!strcmp(s, "window_reboot_none")) {
    satHandlePreset("eco"); windowSwitch();
    reboot(); satHandlePreset("none"); show("none");
    satHandleWindow(false); show("closed");
  } else if (!strcmp(s, "manual")) {
    satHandlePreset("eco");
    settingTargetTemp("22"); show("manual");
  } else if (!strcmp(s, "same")) {
    satHandlePreset("eco");
    settingTargetTemp("18.0"); show("same");
  }
  return 0;
}
"""


def _fn(src: str, sig: str, path: Path) -> str:
    m = re.search(r"^" + sig + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(f"{sig} not found in {path}")
    return m.group(0)


def extract(sat: str, st: str) -> str:
    parts = [_fn(sat, r"static const char\* satGetPresetName\(", SAT_INO)]
    marker = re.search(r"// --- satPresetPersist begin\n(.*?)// --- satPresetPersist end", sat, re.S)
    if marker:
        parts.append("#define HAVE_PRESET_PERSIST 1\n" + marker.group(1))
    parts += [_fn(sat, r"void satHandlePreset\(", SAT_INO),
              _fn(sat, r"void satHandleWindow\(", SAT_INO),
              _fn(sat, r"static void _satCheckWindowTimer\(", SAT_INO),
              _fn(st, r"static float clampSettingFloat\(", SETTINGS_INO)]
    m = re.search(r'writeJsonFloatKV\(file, F\("SATtargettemp"\), (.+?)\);', st)
    if not m:
        raise AssertionError("SATtargettemp writer not found in " + str(SETTINGS_INO))
    parts.append("#define STORED_TARGET_EXPR " + m.group(1))
    # The SATtargettemp branch of updateSetting(): from its `else if` up to the next `else if`.
    b = re.search(r'\n  else if \(strcasecmp_P\(field, PSTR\("SATtargettemp"\)\) == 0\)(.*?)\n  else if', st, re.S)
    if not b:
        raise AssertionError("SATtargettemp branch of updateSetting() not found in " + str(SETTINGS_INO))
    parts.append("static void settingTargetTemp(const char* newValue) {\n"
                 "  const char* field = \"SATtargettemp\";\n  if (false) {}\n  else if (strcasecmp_P(field, PSTR(\"SATtargettemp\")) == 0)"
                 + b.group(1) + "\n}\n")
    return "\n".join(parts) + "\n"


class SatPresetPersist(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + extract(SAT_INO.read_text(encoding="utf-8"),
                                       SETTINGS_INO.read_text(encoding="utf-8")) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, name):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), name], capture_output=True, text=True, check=True).stdout
        rows = {}
        for line in out.splitlines():
            m = re.match(r"(\w+) preset=(\d+) target=(\S+) stored=(\S+)", line)
            self.assertIsNotNone(m, out)
            rows[m.group(1)] = (int(m.group(2)), float(m.group(3)), float(m.group(4)))
        return rows

    COMFORT, ECO, ACTIVITY, NONE = 3, 2, 5, 0

    def test_settings_save_stores_the_users_target_not_the_preset(self):
        r = self.run_case("preset")
        self.assertEqual(r["before"][0], self.COMFORT)
        self.assertAlmostEqual(r["before"][1], 21.5)
        self.assertAlmostEqual(r["before"][2], 20.0, msg="writeSettings stored the preset temperature as the user's target")

    def test_preset_survives_a_reboot_and_none_restores_the_users_target(self):
        r = self.run_case("preset")
        self.assertEqual(r["after"][0], self.COMFORT, msg="preset lost after reboot")
        self.assertAlmostEqual(r["after"][1], 21.5)
        self.assertEqual(r["none"][0], self.NONE)
        self.assertAlmostEqual(r["none"][1], 20.0, msg="preset none did not restore the user's own target")

    def test_window_activity_survives_a_reboot_and_close_restores(self):
        r = self.run_case("window")
        self.assertEqual(r["before"][0], self.ACTIVITY)
        self.assertAlmostEqual(r["before"][2], 20.0, msg="window activity target stored as the user's target")
        self.assertEqual(r["after"][0], self.ACTIVITY, msg="window activity lost after reboot")
        self.assertAlmostEqual(r["after"][1], 16.0)
        self.assertEqual(r["closed"][0], self.NONE)
        self.assertAlmostEqual(r["closed"][1], 20.0, msg="window close after reboot did not restore the target")

    def test_preset_then_window_survives_a_reboot(self):
        r = self.run_case("preset_window")
        self.assertAlmostEqual(r["before"][2], 20.0)
        self.assertEqual(r["after"][0], self.ACTIVITY)
        self.assertEqual(r["closed"][0], self.ECO)
        self.assertAlmostEqual(r["closed"][1], 18.0)
        self.assertAlmostEqual(r["closed"][2], 20.0)
        self.assertEqual(r["none"][0], self.NONE)
        self.assertAlmostEqual(r["none"][1], 20.0)

    # Reviewer finding (TASK-1487): eco -> window switch -> none stored eco's 18 as the user's target.
    # thermo-nova restores the pre-activity target on a close only while the preset is still ACTIVITY
    # (climate.py:830-832), so a preset chosen during the switch ends it and stays.
    def test_preset_none_during_window_switch_stores_the_users_target(self):
        r = self.run_case("window_none")
        self.assertEqual(r["none"][0], self.NONE)
        self.assertAlmostEqual(r["none"][1], 20.0)
        self.assertAlmostEqual(r["none"][2], 20.0, msg="writeSettings stored the eco temperature after preset none")
        self.assertEqual(r["rearm"][0], self.NONE, msg="the still-open window switched to activity again")
        self.assertAlmostEqual(r["rearm"][1], 20.0)
        self.assertEqual(r["closed"][0], self.NONE, msg="window close overwrote preset none")
        self.assertAlmostEqual(r["closed"][1], 20.0)
        self.assertAlmostEqual(r["closed"][2], 20.0)
        self.assertEqual(r["after"][0], self.NONE)
        self.assertAlmostEqual(r["after"][1], 20.0)

    def test_preset_chosen_during_window_switch_survives_the_close(self):
        r = self.run_case("window_comfort")
        self.assertEqual(r["comfort"][0], self.COMFORT)
        self.assertAlmostEqual(r["comfort"][2], 20.0)
        self.assertEqual(r["closed"][0], self.COMFORT, msg="window close overwrote the preset chosen during the switch")
        self.assertAlmostEqual(r["closed"][1], 21.5)
        self.assertAlmostEqual(r["closed"][2], 20.0)
        self.assertEqual(r["after"][0], self.COMFORT)
        self.assertAlmostEqual(r["after"][1], 21.5)
        self.assertAlmostEqual(r["after"][2], 20.0)

    def test_preset_none_after_a_reboot_during_window_switch(self):
        r = self.run_case("window_reboot_none")
        self.assertEqual(r["none"][0], self.NONE)
        self.assertAlmostEqual(r["none"][1], 20.0)
        self.assertAlmostEqual(r["none"][2], 20.0)
        self.assertEqual(r["closed"][0], self.NONE)
        self.assertAlmostEqual(r["closed"][1], 20.0)
        self.assertAlmostEqual(r["closed"][2], 20.0)

    def test_manual_target_clears_the_preset(self):
        # climate.py:567: a manual temperature that matches no preset sets preset none.
        r = self.run_case("manual")
        self.assertEqual(r["manual"][0], self.NONE, msg="manual target left the preset active")
        self.assertAlmostEqual(r["manual"][1], 22.0)
        self.assertAlmostEqual(r["manual"][2], 22.0)

    def test_unchanged_target_in_a_settings_post_keeps_the_preset(self):
        r = self.run_case("same")
        self.assertEqual(r["same"][0], self.ECO)
        self.assertAlmostEqual(r["same"][1], 18.0)
        self.assertAlmostEqual(r["same"][2], 20.0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
