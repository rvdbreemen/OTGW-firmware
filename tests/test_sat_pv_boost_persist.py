#!/usr/bin/env python3
"""TASK-1459: set/<node>/sat/pv_boost_enabled over MQTT is persisted, like the other SAT switches.

Before the fix the kSatMqttCmds row for "pv_boost_enabled" pointed at a handler
(_satPvBoostEnabledCmd -> satHandlePvBoostEnabled) that wrote settings.sat.bPvBoostEnabled in RAM
only, so the switch fell back after a reboot. The other SAT switches use a settingKey row, which the
dispatcher forwards to updateSetting(), the path that marks the settings for the debounced save.

The code under test is NOT re-implemented: the test cuts the real SatMqttHandler typedef, the
SatMqttCmdEntry struct, the kSatMqttCmds[] table and dispatchSatMqttCmd() out of
src/OTGW-firmware/MQTTstuff.ino and compiles them on the host. Every handler the table names is
stubbed (it records its call), and updateSetting() records its key and value. A source check on
settingStuff.ino confirms that updateSetting("SATpvboostenabled", ...) sets the field, clears the
boost state on disable, and that the settings writer stores the key.

OTGW_MQTT_INO points at another copy (e.g. `git show <rev>:src/OTGW-firmware/MQTTstuff.ino`) for
old-vs-fix runs.

Run: python tests/test_sat_pv_boost_persist.py
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
MQTT_INO = Path(os.environ.get("OTGW_MQTT_INO", REPO / "src" / "OTGW-firmware" / "MQTTstuff.ino"))
SETTINGS_INO = REPO / "src" / "OTGW-firmware" / "settingStuff.ino"

HEAD = r"""
#include <cstdio>
#include <cstring>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define PROGMEM
#define memcpy_P memcpy
static char g_key[64] = "", g_val[64] = "", g_handler[64] = "";
static void updateSetting(const char* k, const char* v) {
  snprintf(g_key, sizeof g_key, "%s", k); snprintf(g_val, sizeof g_val, "%s", v);
}
"""

MAIN = r"""
int main(int argc, char** argv) {   // cmd payload
  bool found = dispatchSatMqttCmd(argv[1], argv[2]);
  printf("found=%d key=%s val=%s handler=%s\n", (int)found, g_key, g_val, g_handler);
  return 0;
}
"""


def _block(src, start_rx, end_char="}", include_semicolon=False):
    m = re.search(start_rx, src, re.M)
    if not m:
        raise AssertionError(f"{start_rx} not found in {MQTT_INO}")
    i = src.index("{", m.start())
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                end = j + 1
                if include_semicolon and src[end:end + 1] == ";":
                    end += 1
                return src[m.start():end]
    raise AssertionError(f"unterminated block for {start_rx}")


def program(src):
    typedef = re.search(r"^typedef void \(\*SatMqttHandler\)\(const char\*\);", src, re.M).group(0)
    struct = _block(src, r"^struct SatMqttCmdEntry\s*\{", include_semicolon=True)
    table = _block(src, r"^static const SatMqttCmdEntry kSatMqttCmds\[\] PROGMEM = \{", include_semicolon=True)
    dispatch = _block(src, r"^static bool dispatchSatMqttCmd\(")
    handlers = sorted(set(re.findall(r"\{\s*\"\w+\",\s*(?:\"\w+\"|nullptr),\s*(\w+)\s*\}", table)) - {"nullptr"})
    stubs = "".join(f'static void {h}(const char*) {{ snprintf(g_handler, sizeof g_handler, "{h}"); }}\n'
                    for h in handlers)
    return HEAD + typedef + "\n" + struct + "\n" + stubs + table + "\n" + dispatch + "\n" + MAIN


class PvBoostPersist(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = MQTT_INO.read_text(encoding="utf-8")
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        (d / "t.cpp").write_text(program(cls.src), encoding="utf-8")
        cls.exe = compile_cpp(d / "t.cpp", d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def dispatch(self, cmd, payload):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), cmd, payload], capture_output=True, text=True, check=True).stdout
        m = re.match(r"found=(\d) key=(\S*) val=(\S*) handler=(\S*)", out)
        self.assertIsNotNone(m, out)
        return int(m.group(1)), m.group(2), m.group(3), m.group(4)

    def test_pv_boost_enabled_goes_through_update_setting(self):
        found, key, val, handler = self.dispatch("pv_boost_enabled", "1")
        self.assertEqual(found, 1)
        self.assertEqual((key, val), ("SATpvboostenabled", "1"),
                         f"the MQTT switch must persist through updateSetting (handler called: {handler or 'none'})")
        self.assertEqual(handler, "", "no RAM-only handler may run for pv_boost_enabled")

    def test_disable_goes_through_update_setting_too(self):
        _, key, val, _ = self.dispatch("pv_boost_enabled", "0")
        self.assertEqual((key, val), ("SATpvboostenabled", "0"))

    def test_another_setting_switch_still_dispatches(self):
        # control: a known settingKey row (dhw_enabled -> SATdhwenabled) is unaffected
        _, key, val, _ = self.dispatch("dhw_enabled", "1")
        self.assertEqual((key, val), ("SATdhwenabled", "1"))

    def test_update_setting_branch_persists_and_clears_the_boost(self):
        s = SETTINGS_INO.read_text(encoding="utf-8")
        m = re.search(r'strcasecmp_P\(field, PSTR\("SATpvboostenabled"\)\) == 0\) \{(.*?)\n  \}', s, re.S)
        self.assertIsNotNone(m, "updateSetting() has no SATpvboostenabled branch")
        body = m.group(1)
        self.assertIn("settings.sat.bPvBoostEnabled =", body)
        for f in ("state.sat.bPvBoostActive = false", "state.sat.fPvBoostAppliedC = 0.0f", "state.sat.iPvBoostStartedMs = 0"):
            self.assertIn(f, body, "disable must clear the running boost: " + f)
        self.assertIn('writeJsonBoolKV (file, F("SATpvboostenabled"),', s, "the settings writer must store the key")


if __name__ == "__main__":
    unittest.main(verbosity=2)
