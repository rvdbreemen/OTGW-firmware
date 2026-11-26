#!/usr/bin/env python3
"""TASK-2949.11 (ADR-265 OQ3 option D, cut 3a): the otgw-pic/settings/* topics are retained and not republished every 5 minutes.

Compiles the REAL publishAllPICsettings() and the REAL tail of the PR= answer handler in OTGW-Core.ino (the register switch and
its publish, cut out from "// --- PIC settings registers ---" to the end of the function) against a sendMQTTData() that records
topic and retain flag. Source pins (the .ino files are not compiled whole): do5minevent() no longer calls publishAllPICsettings(),
the connect stage keeps its call.
  - all 15 settings publishes of publishAllPICsettings() carry retain=true
  - a PR= answer (register 'O' and 'V' as examples, and all 15 registers in a loop) publishes its field retained, only on change
Red on the base (OTGW_SRC_REV, e.g. origin/dev): retain=false everywhere, do5minevent() calls publishAllPICsettings().
Mutants (each must turn a test red): retain false at one publish of publishAllPICsettings(); retain false in the PR= handler;
the 5-minute call back in do5minevent().
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_mqtt_state import body, read  # noqa: E402
from test_sat_published_curve import compile_cpp  # noqa: E402

PRELUDE = r"""
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define OTDebugTf(...) ((void)0)
#define snprintf_P snprintf
static size_t test_strlcpy(char* d, const char* s, size_t n) {
  size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define strlcpy test_strlcpy
struct Rec { char topic[64]; char value[32]; int retain; };
static Rec g_rec[64];
static int g_n = 0;
static bool sendMQTTData(const __FlashStringHelper* topic, const char* json, const bool retain = false) {
  Rec& r = g_rec[g_n++];
  snprintf(r.topic, sizeof(r.topic), "%s", reinterpret_cast<const char*>(topic));
  snprintf(r.value, sizeof(r.value), "%s", json);
  r.retain = retain ? 1 : 0;
  return true;
}
static bool isPICEnabled() { return true; }
static bool hasWebSocketClients() { return false; }
static void sendEventToWebSocket(char, const char*) {}
"""

MAIN = r"""
static void dump(const char* tag) {
  int retained = 0;
  for (int i = 0; i < g_n; ++i) retained += g_rec[i].retain;
  printf("%s n=%d retained=%d\n", tag, g_n, retained);
}
int main() {
  // every register of the PIC settings, as the PIC would answer
  const char regs[] = "OSWGILTDPRBCQNV";
  const char* vals[] = {"T20.5", "15.0", "1", "05", "00", "RFFTTT", "01", "O", "L", "00", "17:52", "4", "W", "10", "3"};
  for (int i = 0; i < 15; ++i) {
    snprintf(state.picSettings.sBuilddate, 4, "%s", "");
  }
  // 1. publishAllPICsettings(): fill every cached field first
  strcpy(state.picSettings.sSetpointOverride, "T20.5"); strcpy(state.picSettings.sSetback, "15.0"); strcpy(state.picSettings.sDhwOverride, "1");
  strcpy(state.picSettings.sGpio, "05"); strcpy(state.picSettings.sGpioStates, "00"); strcpy(state.picSettings.sLed, "RFFTTT");
  strcpy(state.picSettings.sTweaks, "01"); strcpy(state.picSettings.sTempSensor, "O"); strcpy(state.picSettings.sSmartPower, "L");
  strcpy(state.picSettings.sThermostatDetect, "00"); strcpy(state.picSettings.sBuilddate, "17:52 12-03-2023"); strcpy(state.picSettings.sClockMHz, "4");
  strcpy(state.picSettings.sResetCause, "W"); strcpy(state.picSettings.sStandaloneInterval, "10"); strcpy(state.picSettings.sVoltageRef, "3");
  g_n = 0;
  publishAllPICsettings();
  dump("all");
  // 2. a PR= answer for each register publishes its own field, retained, once (the second identical answer publishes nothing)
  memset(&state.picSettings, 0, sizeof(state.picSettings));
  g_n = 0;
  for (int i = 0; i < 15; ++i) prTail(regs[i], vals[i]);
  dump("pr");
  const int first = g_n;
  for (int i = 0; i < 15; ++i) prTail(regs[i], vals[i]);
  printf("pr_again new=%d\n", g_n - first);
  return 0;
}
"""


def build(transform=None, tag="x"):
    core = read("OTGW-Core.ino")
    types = read("PICtypes.h")
    struct = re.search(r"struct PicSettingsSection \{.*?\n\};", types, re.S).group(0)
    start = core.index("  // --- PIC settings registers ---")
    end = core.index("\n}\n", start) + 3
    tail = core[start:end]
    publish_all = "void publishAllPICsettings() " + body(core, "void publishAllPICsettings()")
    text = (read("picSettingValue.h").replace("#pragma once", "") + "\n" + struct + "\nstatic struct { PicSettingsSection picSettings; } state;\n"
            + publish_all + "\nstatic void prTail(char reg, const char* value) {\n" + tail)
    if transform:
        new = transform(text)
        assert new != text, "mutant changed nothing"
        text = new
    tmp = tempfile.TemporaryDirectory()
    src = Path(tmp.name) / f"pic_{tag}.cpp"
    src.write_text(PRELUDE + text + "\n" + MAIN, encoding="utf-8")
    exe = compile_cpp(src, Path(tmp.name) / f"pic_{tag}.exe")
    out = {}
    if exe:
        res = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in res.splitlines():
            name, *rest = line.split()
            out[name] = dict(x.split("=", 1) for x in rest)
    return tmp, out


class PicSettingsRetained(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp, cls.out = build(tag="fix")
        print("\n" + "\n".join(f"{k} {v}" for k, v in cls.out.items()))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def need(self):
        if not self.out:
            self.skipTest("no host C++ compiler")

    def test_publish_all_is_retained(self):
        self.need()
        self.assertEqual(self.out["all"], {"n": "15", "retained": "15"})

    def test_pr_answers_are_retained_and_only_on_change(self):
        self.need()
        self.assertEqual(self.out["pr"], {"n": "15", "retained": "15"})
        self.assertEqual(self.out["pr_again"], {"new": "0"})

    def test_do5minevent_does_not_republish_the_pic_settings(self):
        src = read("OTGW-firmware.ino")
        self.assertNotIn("publishAllPICsettings", body(src, "void do5minevent()"))

    def test_mutant_the_five_minute_call_back_is_caught(self):
        src = read("OTGW-firmware.ino")
        mutated = src.replace("  sendMQTTstateinformation();\n", "  sendMQTTstateinformation();\n  publishAllPICsettings();\n", 1)
        self.assertNotEqual(mutated, src, "mutant changed nothing: the anchor line is gone")
        self.assertIn("publishAllPICsettings", body(mutated, "void do5minevent()"))

    def test_the_connect_stage_keeps_its_call(self):
        self.assertEqual(len(re.findall(r"^\s*publishAllPICsettings\(\);", read("MQTTstuff.ino"), re.M)), 1)

    def mutant(self, name, transform, check):
        tmp, out = build(transform, name)
        try:
            if not out:
                self.skipTest("no host C++ compiler")
            self.assertTrue(check(out), f"mutant {name} still passes: {out}")
        finally:
            tmp.cleanup()

    def test_mutant_one_publish_not_retained(self):
        self.mutant("oneplain", lambda t: t.replace('state.picSettings.sVoltageRef, true);', 'state.picSettings.sVoltageRef);', 1),
                    lambda o: o["all"]["retained"] != "15")

    def test_mutant_pr_handler_not_retained(self):
        self.mutant("prplain", lambda t: t.replace("sendMQTTData(mqttTopic, stateField, true);", "sendMQTTData(mqttTopic, stateField);"),
                    lambda o: o["pr"]["retained"] != "15")


if __name__ == "__main__":
    unittest.main(verbosity=2)
