#!/usr/bin/env python3
"""TASK-2971 H5 (ADR-148, no new ADR): BLE sensor entities carry the gateway availability and an expire_after.

Before: satBLEPublishOneDiscovery() (MQTTstuff.ino) sent no avty_t and no exp_aft, so a BLE probe with a dead battery
kept its last temperature in Home Assistant as if it were current. After: avty_t is the gateway availability topic
(MQTTPubNamespace, the same as every main entity) and exp_aft is BLE_HA_EXPIRE_AFTER_S. The state topics publish on
change plus a heartbeat of at most 11 min (SATmqttPublish.h) and stop when the probe is stale, so exp_aft must be
longer than one heartbeat plus a publish cycle: 660 + 2 x 300 = 1260 s. The four kinds share that cadence.

This test compiles the REAL satBLEPublishOneDiscovery() and mqttJsonEscape() from MQTTstuff.ino with stubs for the
MQTT client and captures the payload of all four kinds, with and without a user label. Checks:
  1. avty_t == the gateway namespace, exp_aft == 1260 for temp, rh, bat and rssi (red on the base: both absent);
  2. topic, uniq_id, stat_t, dev_cla, unit and the device block are unchanged versus the base (contract unchanged).
On the host the PROGMEM %S conversions are compiled as %s (PGM_P is a plain char pointer here).
OTGW_MQTT_BASE (default origin/dev) is the revision for the comparison.
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(HERE))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp)

INO = REPO / "src" / "OTGW-firmware" / "MQTTstuff.ino"
BASE = os.environ.get("OTGW_MQTT_BASE", "origin/dev")


def cut_func(src, signature):
    i = src.index(signature)
    while src.find(";", i) < src.find("{", i):      # skip forward declarations (prototype ends with ';' first)
        i = src.index(signature, i + 1)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
    raise AssertionError("unbalanced " + signature)


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
typedef const char* PGM_P;
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define snprintf_P snprintf
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define MQTT_TOPIC_MAX_LEN 128
#define MQTT_DISCOVERY_HEAP_MIN 8000
#define CSTR(x) (x)
static void MQTTDebugTf(const char*, ...) {}
static void PrintMQTTError() {}
static void feedWatchDog() {}
static bool canPublishMQTT() { return true; }
static uint32_t platformFreeHeap() { return 60000; }
static bool isValidIP(int) { return true; }
static int MQTTbrokerIP = 1;
struct { bool connected() { return true; } } MQTTclient;
struct { struct { bool bEnable = true; const char* sHaprefix = "homeassistant"; const char* sUniqueid = "otgw-AABBCCDDEEFF";
                  const char* sTopTopic = "OTGW"; } mqtt; } settings;
static char MQTTPubNamespace[64] = "OTGW/value/otgw-AABBCCDDEEFF";
static bool mqttPublishRaw(const char* topic, const uint8_t* p, size_t n, bool retain) {
  printf("PUB %s %d ", topic, retain ? 1 : 0); fwrite(p, 1, n, stdout); printf("\n"); return true;
}
"""

MAIN = r"""
int main() {
  const char* kinds[4][5] = {{"temp","Temperature","temperature","\xC2\xB0""C","{{ value | float }}"},
                             {"rh","Humidity","humidity","%","{{ value | float }}"},
                             {"bat","Battery","battery","%","{{ value | int }}"},
                             {"rssi","Signal Strength","signal_strength","dBm","{{ value | int }}"}};
  for (int lbl = 0; lbl < 2; lbl++)
    for (auto& k : kinds)
      satBLEPublishOneDiscovery("a4c138000001", "A4:C1:38:00:00:01", k[0], F(k[1]), F(k[2]), F(k[3]), F(k[4]),
                                lbl ? "Woon\"kamer" : nullptr);
  return 0;
}
"""


def program(src):
    pre_const = ""  # the constant lives inside the function (the discovery harness slices only the function)
    if "BLE_HA_EXPIRE_AFTER_S" in src:
        i = src.index("static const uint32_t BLE_HA_EXPIRE_AFTER_S")
        pre_const = src[i:src.index("\n", i) + 1]
    esc = cut_func(src, "static void mqttJsonEscape(const char* src, char* dst, size_t dstSize)")
    fn = cut_func(src, "static bool satBLEPublishOneDiscovery(const char* macCompact,")
    fn = fn.replace("%S", "%s")
    return PRE + pre_const + esc + "\n" + fn + MAIN


def run(src, tmp, name):
    cpp = Path(tmp) / (name + ".cpp")
    cpp.write_text(program(src), encoding="utf-8")
    exe = base.compile_cpp(cpp, Path(tmp) / (name + (".exe" if os.name == "nt" else "")))
    if exe is None:
        raise unittest.SkipTest("no host C++ compiler (g++, clang++ or MSVC) found")
    out = subprocess.run([str(exe)], capture_output=True, check=True).stdout.decode("utf-8")
    pubs = []
    for line in out.splitlines():
        if line.startswith("PUB "):
            _, topic, retain, raw = line.split(" ", 3)
            pubs.append((topic, retain == "1", json.loads(raw)))
    return pubs


class BleDiscoveryAvailability(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        fix = INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        old = subprocess.run(["git", "show", f"{BASE}:src/OTGW-firmware/MQTTstuff.ino"], cwd=REPO,
                             capture_output=True, check=True).stdout.decode("utf-8").replace("\r\n", "\n")
        cls.fix = run(fix, cls.tmp.name, "fix")
        cls.old = run(old, cls.tmp.name, "old")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_all_kinds_publish(self):
        self.assertEqual(len(self.fix), 8)
        self.assertTrue(all(retain for _, retain, _ in self.fix))

    def test_availability_and_expire_after(self):
        for topic, _, p in self.fix:
            with self.subTest(topic):
                self.assertEqual(p.get("avty_t"), "OTGW/value/otgw-AABBCCDDEEFF")
                self.assertEqual(p.get("exp_aft"), 1260)

    def test_base_had_neither(self):
        for topic, _, p in self.old:
            with self.subTest(topic):
                self.assertNotIn("avty_t", p)
                self.assertNotIn("exp_aft", p)

    def test_contract_unchanged(self):
        self.assertEqual([t for t, _, _ in self.fix], [t for t, _, _ in self.old], "discovery topics changed")
        for (topic, _, new), (_, _, old) in zip(self.fix, self.old):
            with self.subTest(topic):
                self.assertEqual({k: v for k, v in new.items() if k not in ("avty_t", "exp_aft")}, old)

    def test_expire_after_outlasts_the_publish_gap(self):
        """exp_aft must exceed the longest silence of a live probe: one heartbeat plus two publish cycles, and the
        heartbeat plus the stale timeout. Read from the sources, so a later change to either constant is caught."""
        src = REPO / "src" / "OTGW-firmware"
        hb = re.search(r"SAT_HEARTBEAT_MAX_MS = 11UL \* 60UL \* 1000UL|SAT_HEARTBEAT_MAX_MS = (\d+)",
                       (src / "SATmqttPublish.h").read_text(encoding="utf-8"))
        self.assertIsNotNone(hb, "SAT_HEARTBEAT_MAX_MS not found")
        hb_ms = 660000 if hb.group(1) is None else int(hb.group(1))
        stale_ms = int(re.search(r"BLE_STALE_MS\s*=\s*(\d+)", (src / "SATble.ino").read_text(encoding="utf-8")).group(1))
        imax = int(re.search(r"SATbleinterval\"\)\) == 0\) settings\.sat\.iBleInterval = constrain\(atoi\(newValue\), \d+, (\d+)\)",
                             (src / "settingStuff.ino").read_text(encoding="utf-8")).group(1))
        exp_ms = self.fix[0][2]["exp_aft"] * 1000
        self.assertGreaterEqual(exp_ms, hb_ms + 2 * imax * 1000)
        self.assertGreater(exp_ms, hb_ms + stale_ms)


if __name__ == "__main__":
    unittest.main(verbosity=2)
