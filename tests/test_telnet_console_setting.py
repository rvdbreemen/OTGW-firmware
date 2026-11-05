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
#include <cstdint>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
typedef uint8_t byte;
struct Settings { bool bTelnetEnabled = true; } settings;

// Controllable fake clock; the real safeTimers.h (DECLARE_TIMER_SEC / DUE) runs against it.
static uint32_t fakeNow = 0;
static uint32_t millis() { return fakeNow; }
static long random(long lo, long hi) { return lo; }

// debugTelnet models the AsyncTCP listener: stop() with a client connected closes it from the server side,
// which leaves local port 23 in TIME_WAIT for 120 s; begin() then fails tcp_bind (no SO_REUSEADDR).
static int nBegin = 0, nStop = 0;
static bool listening = false, clientConnected = false;
static uint32_t busyUntil = 0;
struct TelnetStub {
  void onConnect(void (*)()) {}
  void setLineMode(bool) {}
  void setNoDelay(bool) {}
  void onInputReceived(void (*)(const char*)) {}
  bool begin(bool) {
    nBegin++;
    if (listening) return true;
    if ((int32_t)(fakeNow - busyUntil) < 0) return false;
    listening = true;
    return true;
  }
  void stop() {
    nStop++;
    if (clientConnected) busyUntil = fakeNow + 120000UL;
    clientConnected = false;
    listening = false;
  }
} debugTelnet;
static void sendTelnetBanner() {}
static void onTelnetInput(const char*) {}
static const char* getActiveIP() { return "0.0.0.0"; }
#define F(x) x
#define PSTR(x) x
#define DebugT(...) do {} while (0)
#define DebugTln(...) do {} while (0)
#define DebugTf(...) do {} while (0)
#include "@SAFETIMERS@"
"""

# One command per stdin line; "print" emits "listening nBegin nStop".
MAIN = r"""
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string c; long v = 0;
    is >> c >> v;
    if (c == "enable") settings.bTelnetEnabled = v != 0;
    else if (c == "client") clientConnected = v != 0;
    else if (c == "start") startTelnet();
    else if (c == "adv") fakeNow += (uint32_t)v;
    else if (c == "tick") telnetRelistenTick();
    else if (c == "print") printf("%d %d %d\n", listening ? 1 : 0, nBegin, nStop);
  }
  return 0;
}
"""

TICK_MARK = "// TASK-1644: stop() closes a connected client"


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


def build_part1_source():
    """Cut the real telnet code from networkStuff.ino: the TASK-1644 static block, startTelnet() and (when the
    source has it) telnetRelistenTick(). Old sources lack the block and the tick; the second value reports that.
    safeTimers.h is the real header, run against the fake millis()."""
    src = read("networkStuff.ino")
    at = src.find(TICK_MARK)
    block = src[at:src.find("void startTelnet()")] if at >= 0 else ""
    start_fn = cut_function(src, "void startTelnet()")
    has_tick = "void telnetRelistenTick()" in src
    tick = cut_function(src, "void telnetRelistenTick()") if has_tick else "void telnetRelistenTick() {}"
    prelude = PRELUDE.replace("@SAFETIMERS@", (SRC / "safeTimers.h").as_posix())
    return prelude + block + start_fn + "\n" + tick + "\n" + MAIN, has_tick


class StartTelnetHonoursSetting(unittest.TestCase):
    exe = None
    has_tick = False

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "telnet.cpp"
        code, cls.has_tick = build_part1_source()
        cpp.write_text(code, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("telnet.exe" if os.name == "nt" else "telnet"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_script(self, lines):
        """Run commands, return one (listening, nBegin, nStop) tuple per 'print'."""
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True,
                             check=True).stdout
        return [tuple(int(x) for x in row.split()) for row in out.splitlines()]

    def need_tick(self):
        if not self.has_tick:
            self.fail("networkStuff.ino has no telnetRelistenTick(): after telnetenabled off with a client, "
                      "then on, begin() fails in the 120 s TIME_WAIT and nothing retries (port 23 stays closed)")

    def run_with(self, enabled):
        return self.run_script(["enable %d" % (1 if enabled else 0), "start", "print"])[0][1:]

    @staticmethod
    def steps(seconds):
        lines = []
        for _ in range(seconds):
            lines += ["adv 1000", "tick", "print"]
        return lines

    def test_off_closes_port_23_and_never_listens(self):
        n_begin, n_stop = self.run_with(False)
        self.assertEqual(n_begin, 0, "startTelnet() opened port 23 with telnetenabled off")
        self.assertEqual(n_stop, 1, "startTelnet() with telnetenabled off must close a running server (live apply)")

    def test_on_listens(self):
        n_begin, n_stop = self.run_with(True)
        self.assertEqual((n_begin, n_stop), (1, 0))

    # --- TASK-1644 bench finding: re-enable inside the TIME_WAIT window of the closed client ---------------
    OFF_ON = ["enable 1", "start", "client 1", "enable 0", "start", "enable 1", "start"]

    def test_reenable_after_off_with_client_listens_within_130s(self):
        self.need_tick()
        rows = self.run_script(self.OFF_ON + ["print"] + self.steps(130))
        self.assertEqual(rows[0][0], 0, "model check: the immediate begin() must fail inside TIME_WAIT")
        t_listen = next((i for i, r in enumerate(rows[1:], 1) if r[0] == 1), None)
        self.assertIsNotNone(t_listen, "port 23 never re-listened within 130 s of the re-enable")
        self.assertLessEqual(t_listen, 130)

    def test_normal_boot_never_calls_begin_from_tick(self):
        self.need_tick()
        rows = self.run_script(["enable 1", "start", "print"] + self.steps(200))
        self.assertEqual(rows[-1][1], 1, "tick called begin() although telnet was never stopped")

    def test_window_expires_after_180s(self):
        self.need_tick()
        at_190 = self.run_script(self.OFF_ON + self.steps(190))[-1]
        at_400 = self.run_script(self.OFF_ON + self.steps(400))[-1]
        self.assertGreater(at_190[1], 2, "tick never retried begin() inside the window")
        self.assertEqual(at_190[1], at_400[1], "begin() still called after the 180 s retry window expired")

    def test_off_again_during_window_stops_retries(self):
        self.need_tick()
        pre = self.OFF_ON + self.steps(25) + ["enable 0", "start", "print"]
        before = self.run_script(pre)[-1]
        after = self.run_script(pre + self.steps(200))[-1]
        self.assertEqual(before[1], after[1], "tick retried begin() after telnetenabled went off again")
        self.assertEqual(after[0], 0, "port 23 listening although telnetenabled is off")


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
