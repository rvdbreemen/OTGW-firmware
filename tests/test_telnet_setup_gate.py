#!/usr/bin/env python3
"""TASK-1395: a telnet action key that arrives while setup() still runs must not run on async_tcp.

The debug telnet input callback runs on the async_tcp task (AsyncSimpleTelnet.h:417-431 -> networkStuff.ino onTelnetInput ->
handleDebug.ino handleDebugChar). On the bench (.63, beta.79, 3 of 3 reboots, 2026-11-04) a 'q' sent 8-9 s after a reboot ran
readSettings(true) long before "Setup finished!" and interleaved its output with setup()'s lines (spliced telnet lines), so
setup() and the settings reload ran at the same time on two tasks.

This test compiles the REAL handleDebugChar() cut out of handleDebug.ino against stubs that record which action ran, and checks:
  * while state.bSetupComplete is false, the action keys (q, D, F, V, r, p, i, w, z ...) do nothing but print a notice;
  * the debug toggles (keys from debugFlags.h) and 'h' still work during setup;
  * once setup is complete every action runs as before.
Run against another copy with OTGW_HANDLEDEBUG_INO=<path> (red on the code before TASK-1395).

Run: python tests/test_telnet_setup_gate.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

INO = Path(os.environ.get("OTGW_HANDLEDEBUG_INO", REPO / "src" / "OTGW-firmware" / "handleDebug.ino"))

PRELUDE = r"""
#include <cstdio>
#include <cstring>
#include <string>
#define HAS_PIC 0
#define HAS_SAT_BLE 0
#define F(s) s
#define PSTR(s) s
#define CBOOLEAN(b) ((b) ? "true" : "false")
#define CSTR(s) (s)
#define ON 1
#define OFF 0
#define LED1 2
#define WL_CONNECTED 3
static std::string g_ran;   // actions that ran, one letter-tag each
static std::string g_out;   // everything printed
static void Debugln(const char* s = "") { g_out += s; g_out += "\n"; }
static void DebugTln(const char* s) { g_out += s; g_out += "\n"; }
static void DebugTf(const char* fmt, ...) { g_out += fmt; }
static void OTDebugTf(const char* fmt, ...) { g_out += fmt; }
struct DebugState { bool bToggle = false; };
struct State { bool bSetupComplete = false; DebugState debug; struct { bool bConnected = true; } mqtt; };
static State state;
struct Settings { char sHostname[8] = "h"; bool bMyDEBUG = false; struct { bool bEnable = true; } mqtt; struct { int iPin = 5; } outputs; };
static Settings settings;
struct DebugFlagDef { char key; const char* label; bool DebugState::*member; };
static const DebugFlagDef kFlag = { '1', "OTmsg", &DebugState::bToggle };
static const DebugFlagDef* debugFlagByKey(char c) { return c == '1' ? &kFlag : nullptr; }
static void debugFlagWrite(DebugState& d, const DebugFlagDef* f, bool on) { d.*(f->member) = on; g_ran += "T"; }
static void dumpDebugInfo() { g_ran += "D"; }
static bool isOTDirectEnabled() { return false; }
static void detectPIC() { g_ran += "p"; }
static void readSettings(bool) { g_ran += "q"; }
static void doAutoConfigure() { g_ran += "F"; }
static bool startDiscoveryVerification() { g_ran += "V"; return true; }
struct WiFiStub { int status() { return WL_CONNECTED; } } WiFi;
static void startWiFi(const char*, int) { g_ran += "W"; }
static void startTelnet() {}
static void startPICStream() {}
static void startMQTT() { g_ran += "M"; }
static void blinkLED(int, int, int) { g_ran += "b"; }
static void initOutputs() { g_ran += "i"; }
static void digitalWrite(int, int) { g_ran += "o"; }
static int digitalRead(int) { g_ran += "j"; return 0; }
static void weatherFetchOpenMeteoNow() { g_ran += "w"; }
static void resetHeapWatermark() { g_ran += "z"; }
"""

MAIN = r"""
int main(int argc, char** argv) {
  state.bSetupComplete = argv[1][0] == '1';
  for (const char* k = argv[2]; *k; k++) {
    g_ran.clear(); g_out.clear();
    handleDebugChar(*k);
    printf("%c ran=[%s] out=%zu notice=%d\n", *k, g_ran.c_str(), g_out.size(), g_out.find("setup") != std::string::npos ? 1 : 0);
  }
  return 0;
}
"""

ACTIONS = "qDFVpbiujolfwz"   # every action key of handleDebugChar except 'h' (menu) and 'r' (no-op here: WiFi and MQTT connected)


def cut_handle_debug_char(src):
    m = re.search(r"^void handleDebugChar\(char c\)\s*\{.*?^\}", src, re.S | re.M)
    if not m:
        raise AssertionError("handleDebugChar not found in " + str(INO))
    return m.group(0)


class TelnetSetupGate(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "gate.cpp"
        cpp.write_text(PRELUDE + cut_handle_debug_char(INO.read_text(encoding="utf-8")) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("gate.exe" if os.name == "nt" else "gate"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_keys(self, setup_complete, keys):
        out = subprocess.run([str(self.exe), "1" if setup_complete else "0", keys], capture_output=True, text=True, check=True).stdout
        res = {}
        for line in out.splitlines():
            m = re.match(r"(.) ran=\[(.*)\] out=(\d+) notice=(\d)", line)
            res[m.group(1)] = (m.group(2), int(m.group(3)), m.group(4) == "1")
        return res

    def test_action_keys_do_nothing_during_setup(self):
        res = self.run_keys(False, ACTIONS)
        for k in ACTIONS:
            ran, _, notice = res[k]
            self.assertEqual(ran, "", "key %r ran %r while setup() still runs" % (k, ran))
            self.assertTrue(notice, "key %r: no notice that setup is still running" % k)

    def test_q_and_D_are_refused_during_setup(self):
        # the two keys of the TASK-1395 field capture (otgw_capture.py sends q then D after a reconnect)
        res = self.run_keys(False, "qD")
        self.assertNotIn("q", res["q"][0], "readSettings(true) ran on async_tcp during setup()")
        self.assertNotIn("D", res["D"][0], "dumpDebugInfo ran on async_tcp during setup()")

    def test_toggles_and_menu_work_during_setup(self):
        res = self.run_keys(False, "1h")
        self.assertEqual(res["1"][0], "T", "a debug toggle must still flip during setup")
        self.assertGreater(res["h"][1], 200, "'h' must still print the menu during setup")

    def test_every_action_runs_after_setup(self):
        res = self.run_keys(True, ACTIONS)
        expect = {"q": "q", "D": "D", "F": "F", "V": "V", "p": "p", "b": "b", "i": "i", "u": "o", "j": "j", "o": "o",
                  "w": "w", "z": "z"}
        for k, tag in expect.items():
            self.assertEqual(res[k][0], tag, "key %r after setup" % k)
            self.assertFalse(res[k][2], "key %r after setup: no setup notice expected" % k)


if __name__ == "__main__":
    unittest.main(verbosity=2)
