#!/usr/bin/env python3
"""TASK-1345: host test for the SAT MQTT-loss fallback (opt-in, state only).

The real code is cut out of src/OTGW-firmware/SATcontrol.ino and compiled with a host C++
compiler; nothing is re-implemented:
  - fix:  satIsEnabled() + satOwnsControlSetpoint() and the whole satMqttFallbackStep() function
  - HEAD: satOwnsControlSetpoint() and the inline block between "// --- Fallback detection" and
          "// Early bail when SAT is disabled", wrapped in step()
OTGW_SAT_INO points at another copy (e.g. `git show HEAD:...`) for old-vs-fix runs.
Without a compiler the tests are skipped.

Run: python tests/test_sat_mqtt_fallback.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define HAS_SAT 1
#define F(x) x
enum { SAT_FB_NONE = 0, SAT_FB_MQTT_LOST = 1 };
struct SatSettings { bool bEnabled = false; bool bMqttFallback = false; };
struct MqttSettings { bool bEnable = true; };
struct Settings { SatSettings sat; MqttSettings mqtt; };
struct SatState { bool bFallbackActive = false; bool bFallbackSuppressed = false; int eFallbackReason = 0; };
struct MqttState { bool bConnected = false; uint32_t iLastConnectedMs = 0; };
struct State { SatState sat; MqttState mqtt; };
static Settings settings;
static State state;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static int g_disableCalls = 0;
static void satDisable() { g_disableCalls++; }
static void DebugTln(const char*) {}
@BODY@
int main(int argc, char** argv) {
  int sc = atoi(argv[1]);
  settings.mqtt.bEnable = true;
  settings.sat.bMqttFallback = (sc != 0);          // scenario 0: opt-in off
  state.mqtt.iLastConnectedMs = 1000; state.mqtt.bConnected = false;
  g_ms = 1000 + 6 * 60 * 1000UL;                   // MQTT lost for 6 minutes
  step();
  printf("active=%d enabledSetting=%d owns=%d disables=%d\n", (int)state.sat.bFallbackActive,
         (int)settings.sat.bEnabled, (int)OWNS(), g_disableCalls);
  if (sc == 2) { state.mqtt.bConnected = true; step(); }                    // MQTT is back
  if (sc == 3) { settings.sat.bEnabled = true; step(); }                    // user enables SAT
  if (sc == 4) { settings.sat.bMqttFallback = false; step(); }              // opt-in switched off
  if (sc == 5) {                                                            // user switches SAT off (updateSetting sets the latch)
    state.sat.bFallbackSuppressed = true; step();
    printf("active=%d enabledSetting=%d disables=%d\n", (int)state.sat.bFallbackActive, (int)settings.sat.bEnabled, g_disableCalls);
    g_ms += 60000; step();                                                  // outage goes on: no re-engage
    printf("active=%d enabledSetting=%d disables=%d\n", (int)state.sat.bFallbackActive, (int)settings.sat.bEnabled, g_disableCalls);
    state.mqtt.bConnected = true; step();                                   // MQTT back: latch cleared
    printf("latch=%d active=%d\n", (int)state.sat.bFallbackSuppressed, (int)state.sat.bFallbackActive);
    state.mqtt.bConnected = false; state.mqtt.iLastConnectedMs = g_ms; g_ms += 6 * 60 * 1000UL; step();   // new outage
  }
  if (sc == 6) { settings.mqtt.bEnable = false; step(); }                   // MQTT switched off during the takeover
  if (sc >= 2) printf("active=%d enabledSetting=%d disables=%d\n", (int)state.sat.bFallbackActive,
                      (int)settings.sat.bEnabled, g_disableCalls);
  return 0;
}
"""


def build_body(src: str) -> str:
    own_end = src.index("//=== Cleanly disable SAT")
    if "bool satMqttFallbackStep()" in src:
        a = src.index("bool satIsEnabled()")
        helpers = src[a:own_end]
        f0 = src.index("bool satMqttFallbackStep()")
        f1 = src.index("// --- end fallback detection")
        f1 = src.index("}", f1) + 1
        step = src[f0:f1] + "\nstatic bool step() { return satMqttFallbackStep(); }\n"
    else:
        a = src.index("bool satOwnsControlSetpoint()")
        helpers = src[a:own_end]
        f0 = src.index("// --- Fallback detection")
        f1 = src.index("// Early bail when SAT is disabled")
        cut = src[f0:f1].replace("return;", "return true;")  # the inline block was in a void function
        step = "static bool step() {\n" + cut + "\n  return false;\n}\n"
    return helpers + "\n#define OWNS satOwnsControlSetpoint\n" + step


class MqttFallback(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        body = build_body(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n"))
        cpp.write_text(STUBS.replace("@BODY@", body))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(sc)], capture_output=True, text=True, check=True).stdout.strip().split("\n")
        return [dict(kv.split("=") for kv in line.split()) for line in out]

    def test_a_opt_in_off_no_fallback(self):
        r = self.run_sc(0)[0]
        self.assertEqual((r["active"], r["enabledSetting"], r["owns"]), ("0", "0", "0"))

    def test_b_opt_in_on_state_only(self):
        r = self.run_sc(1)[0]
        self.assertEqual(r["active"], "1", "fallback must engage")
        self.assertEqual(r["enabledSetting"], "0", "fallback must never write settings.sat.bEnabled")
        self.assertEqual(r["owns"], "1", "SAT must own MsgID 1 during the fallback")

    def test_c_mqtt_back_lifts(self):
        r = self.run_sc(2)[1]
        self.assertEqual((r["active"], r["enabledSetting"], r["disables"]), ("0", "0", "1"))

    def test_d_user_enables_during_fallback(self):
        r = self.run_sc(3)[1]
        self.assertEqual((r["active"], r["enabledSetting"], r["disables"]), ("0", "1", "0"))

    def test_e_opt_in_switched_off_during_takeover(self):
        r = self.run_sc(4)[1]
        self.assertEqual((r["active"], r["enabledSetting"], r["disables"]), ("0", "0", "1"))

    def test_f_user_off_sticks_for_the_outage(self):
        r = self.run_sc(5)
        self.assertEqual((r[1]["active"], r[1]["disables"]), ("0", "1"), "lifted, satDisable once")
        self.assertEqual((r[2]["active"], r[2]["disables"]), ("0", "1"), "no re-engage while MQTT stays lost")
        self.assertEqual((r[3]["latch"], r[3]["active"]), ("0", "0"), "latch cleared after reconnect")
        self.assertEqual(r[4]["active"], "1", "a new outage engages again")

    def test_g_mqtt_disabled_during_takeover(self):
        r = self.run_sc(6)[1]
        self.assertEqual((r["active"], r["disables"]), ("0", "1"))

    def test_h_early_bail_and_latch_wiring(self):
        src = SAT_INO.read_text(encoding="utf-8")
        self.assertIn("if (!satIsEnabled() || isFlashing())", src)
        settings_src = (SAT_INO.parent / "settingStuff.ino").read_text(encoding="utf-8")
        self.assertIn("state.sat.bFallbackSuppressed = true", settings_src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
