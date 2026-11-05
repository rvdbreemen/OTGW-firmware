#!/usr/bin/env python3
"""TASK-1644 (GitHub #154 point 4): a persisted setting switches the telnet debug console (port 23) off. Default on.

Before TASK-1644, startTelnet() in networkStuff.ino always called debugTelnet.begin(false). The console has no login, and
its keys u/i drive the GPIO output (handleDebug.ino), so anyone on the LAN could connect and use them.

Part 1 compiles the REAL startTelnet() (cut from networkStuff.ino by brace matching) against a recording debugTelnet stub
and runs it with the setting on and off. Every caller (boot, WiFi reconnect, the deferred settings apply) goes through
startTelnet(), so this is the function that has to honour the setting.

Part 2 reads the real sources and checks that the setting is wired end to end: the struct default (true), the settings
file writer and reader, the live apply through the deferred side-effect flag (no reboot), GET /api/v2/settings, the POST
whitelist, settingsHelp.h and the v2 Settings catalogue.

Run against older copies with OTGW_SRC_DIR=<dir holding the firmware sources> (red on the code before TASK-1644).

Run: python tests/test_telnet_console_setting.py
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

SRC = Path(os.environ.get("OTGW_SRC_DIR", REPO / "src" / "OTGW-firmware"))

PRELUDE = r"""
#include <cstdio>
#include <cstring>
struct Settings { bool bTelnetEnabled = true; } settings;
static int nBegin = 0, nStop = 0;
struct TelnetStub {
  void onConnect(void (*)()) {}
  void setLineMode(bool) {}
  void setNoDelay(bool) {}
  void onInputReceived(void (*)(const char*)) {}
  bool begin(bool) { nBegin++; return true; }
  void stop() { nStop++; }
} debugTelnet;
static void sendTelnetBanner() {}
static void onTelnetInput(const char*) {}
static const char* getActiveIP() { return "0.0.0.0"; }
#define F(x) x
#define PSTR(x) x
#define DebugT(...) do {} while (0)
#define DebugTln(...) do {} while (0)
#define DebugTf(...) do {} while (0)
"""

MAIN = r"""
int main(int argc, char** argv) {
  settings.bTelnetEnabled = argv[1][0] == '1';
  startTelnet();
  printf("%d %d\n", nBegin, nStop);
  return 0;
}
"""


def cut_function(src, signature):
    at = src.find(signature)
    if at < 0:
        raise AssertionError(signature + " not found")
    start = src.find("{", at)
    depth = 0
    for i in range(start, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[at:i + 1]
    raise AssertionError("unbalanced braces in " + signature)


def read(name):
    return (SRC / name).read_text(encoding="utf-8")


class StartTelnetHonoursSetting(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "telnet.cpp"
        cpp.write_text(PRELUDE + cut_function(read("networkStuff.ino"), "void startTelnet()") + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("telnet.exe" if os.name == "nt" else "telnet"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_with(self, enabled):
        out = subprocess.run([str(self.exe), "1" if enabled else "0"], capture_output=True, text=True, check=True).stdout
        return tuple(int(x) for x in out.split())

    def test_off_closes_port_23_and_never_listens(self):
        n_begin, n_stop = self.run_with(False)
        self.assertEqual(n_begin, 0, "startTelnet() opened port 23 with telnetenabled off")
        self.assertEqual(n_stop, 1, "startTelnet() with telnetenabled off must close a running server (live apply)")

    def test_on_listens(self):
        n_begin, n_stop = self.run_with(True)
        self.assertEqual((n_begin, n_stop), (1, 0))


class SettingWiredEndToEnd(unittest.TestCase):
    def test_struct_default_is_on(self):
        self.assertRegex(read("OTGW-firmware.h"), r"\bbool\s+bTelnetEnabled\s*=\s*true\s*;")

    def test_settings_file_write_and_read(self):
        s = read("settingStuff.ino")
        self.assertIn('writeJsonBoolKV(file, F("telnetenabled"), settings.bTelnetEnabled);', s)
        m = re.search(r'strcasecmp_P\(field, PSTR\("telnetenabled"\)\)==0\)\s*\{(.*?)\}', s, re.S)
        self.assertIsNotNone(m, "updateSetting() has no telnetenabled branch")
        self.assertIn("settings.bTelnetEnabled = EVALBOOLEAN(newValue);", m.group(1))
        self.assertIn("SIDE_EFFECT_TELNET", m.group(1), "the setting is not applied live")

    def test_live_apply_calls_start_telnet(self):
        s = read("settingStuff.ino")
        m = re.search(r"if \(pendingSideEffects & SIDE_EFFECT_TELNET\) \{(.*?)\}", s, re.S)
        self.assertIsNotNone(m, "no deferred apply for SIDE_EFFECT_TELNET")
        self.assertIn("startTelnet();", m.group(1))
        flags = [int(v, 16) for v in re.findall(r"#define SIDE_EFFECT_\w+\s+0x([0-9A-Fa-f]+)", s)]
        self.assertEqual(len(flags), len(set(flags)), "two SIDE_EFFECT_* flags share a bit")

    def test_rest_get_post_help_and_ui(self):
        r = read("restAPI.ino")
        self.assertIn('addBool(F("telnetenabled"), settings.bTelnetEnabled, "b");', r)
        known = r[r.find("knownSettings[] = {"):]
        known = known[:known.find("};")]
        self.assertIn('"telnetenabled"', known, "POST /api/v2/settings would refuse telnetenabled")
        self.assertRegex(read("settingsHelp.h"), r'(?m)^"telnetenabled":"', "no settingsHelp.h entry")
        self.assertRegex(read("data/v2.js"), r"\btelnetenabled:\s*\{ cat: 'system'")


if __name__ == "__main__":
    unittest.main(verbosity=2)
