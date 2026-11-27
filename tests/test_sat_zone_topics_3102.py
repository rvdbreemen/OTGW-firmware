#!/usr/bin/env python3
"""TASK-3102 steps 2 to 4 (entry 459, entry 503): the extra rooms in MQTT and in /api/v2/sat/status.

Step 2: sat/zone/<n>/{active,output,error} publish from ONE extra room, and only for a zone that has received data
(satZones[i].iLastUpdateMs != 0). A single-room install with no zone data publishes none of them (TASK-1440).
Step 3: sat/zone_count is a retained state through publishIfChangedI (ADR-111).
Step 4: /api/v2/sat/status carries a "zones" array (index 1-based, room_temp, setpoint, active) for up to iZoneCount zones.

Nothing is re-implemented: the real satPublishZoneDiagnostics() and the real block between 'begin/end sat-status-zones
record' (with the real JsonEmit) are cut out of SATcontrol.ino and compiled on a host C++ compiler against small stubs.
The wiring (the call gate, the snapshot, the emit call, the zone_count line) is checked on the source text. The full
satSendStatusJSON() and the real broker are not run: that is the bench.

Old-vs-fix: OTGW_SAT_INO=<path to another SATcontrol.ino> (e.g. `git show origin/dev:src/OTGW-firmware/SATcontrol.ino`).
Run: python tests/test_sat_zone_topics_3102.py
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
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = HERE.parent / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
SRC = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

PUB_PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define SAT_MAX_ZONES 4
static const float SAT_EPS_PID_OUTPUT = 0.5f, SAT_EPS_ERROR = 0.05f;
struct SATShadowB { bool v = false; }; struct SATShadowF { float v = 0; };
struct SATZoneState { bool bOff = false, bRoomValid = false, bSpValid = false; uint32_t iLastUpdateMs = 0;
                      float fPidOutput = 0, fSetpoint = 0, fRoomTemp = 0; };
static SATZoneState satZones[SAT_MAX_ZONES];
struct { struct { uint8_t iZoneCount = 0; uint16_t iZoneTimeoutS = 600; } sat; } settings;
static uint32_t g_ms = 100000;
static uint32_t millis() { return g_ms; }
static void feedWatchDog() {}
static std::vector<std::string> g_topics;
static bool publishIfChangedB(const char* t, bool, SATShadowB&, bool) { g_topics.push_back(t); return true; }
static bool publishIfChangedF(const char* t, float, SATShadowF&, float, uint8_t, bool) { g_topics.push_back(t); return true; }
"""

PUB_MAIN = r"""
int main(int argc, char** argv) {
  // argv[1] = zone count; argv[2..] = indexes (0-based) of zones that have received data
  settings.sat.iZoneCount = (uint8_t)atoi(argv[1]);
  for (int i = 2; i < argc; i++) { SATZoneState& z = satZones[atoi(argv[i])]; z.iLastUpdateMs = g_ms - 1000; z.bRoomValid = z.bSpValid = true; }
  satPublishZoneDiagnostics();
  for (auto& t : g_topics) printf("%s\n", t.c_str());
  return 0;
}
"""

EMIT_PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define FPSTR(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define snprintf_P snprintf
typedef const char* PGM_P;
class __FlashStringHelper;
struct String { const char* c_str() const { return ""; } };
struct Print {
  void print(char c) { fputc(c, stdout); }
  void print(const char* s) { fputs(s, stdout); }
  void print(const __FlashStringHelper* s) { fputs(reinterpret_cast<const char*>(s), stdout); }
};
"""

EMIT_MAIN = r"""
int main(int argc, char** argv) {
  // argv[1] = zone count; zone 0 has data (21.5 / 22.0, active), zone 1 has none, zone 2 has data but is stale
  const uint8_t n = (uint8_t)atoi(argv[1]);
  const uint32_t now = 1000000;
  bool has[4] = {true, false, true, false}; float room[4] = {21.5f, 0.0f, 19.0f, 0}; float sp[4] = {22.0f, 0.0f, 20.0f, 0};
  bool act[4];
  act[0] = satZoneStatusActive(true, false, true, true, now - 1000, now, 600000);
  act[1] = satZoneStatusActive(false, false, true, true, now - 1000, now, 600000);   // fresh stamp but no data flag
  act[2] = satZoneStatusActive(true, false, true, true, now - 700000, now, 600000);   // older than the timeout
  act[3] = satZoneStatusActive(true, true, true, true, now - 1000, now, 600000);      // OFF
  Print out; JsonEmit je(out);
  je.beginObject(); satEmitStatusZones(je, n, has, act, room, sp); je.endObject();
  printf("\n");
  return 0;
}
"""


def cut_publisher(src):
    m = re.search(r"^static void satPublishZoneDiagnostics\(\)\n\{.*?^\}\n", src, re.S | re.M)
    return m.group(0) if m else None


def cut_block(src):
    i = src.find("// begin sat-status-zones record")
    j = src.find("// end sat-status-zones record")
    return src[i:j] if i >= 0 and j > i else None


class Publisher(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fn = cut_publisher(SRC)
        assert fn, "satPublishZoneDiagnostics() not found"
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "pub.cpp"
        cpp.write_text(PUB_PRE + fn + PUB_MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("pub.exe" if os.name == "nt" else "pub"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def topics(self, count, *with_data):
        out = subprocess.run([str(self.exe), str(count)] + [str(i) for i in with_data], capture_output=True, text=True, check=True).stdout
        return out.split()

    def test_one_extra_room_with_data_publishes_its_three_topics(self):
        self.assertEqual(sorted(self.topics(1, 0)), ["sat/zone/1/active", "sat/zone/1/error", "sat/zone/1/output"])

    def test_one_extra_room_without_data_publishes_nothing(self):
        self.assertEqual(self.topics(1), [])

    def test_only_the_zone_with_data_publishes(self):
        self.assertEqual(sorted(self.topics(2, 1)), ["sat/zone/2/active", "sat/zone/2/error", "sat/zone/2/output"])

    def test_no_extra_room_publishes_nothing(self):
        self.assertEqual(self.topics(0, 0), [])


class StatusZones(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        block = cut_block(SRC)
        assert block, "no 'sat-status-zones record' block in SATcontrol.ino"
        je = (FW / "jsonEmit.h").read_text(encoding="utf-8")
        je = re.sub(r"#include <(Arduino|platform)\.h>.*", "", je)
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "zones.cpp"
        cpp.write_text(EMIT_PRE + je + block + EMIT_MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("zones.exe" if os.name == "nt" else "zones"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def zones(self, n):
        out = subprocess.run([str(self.exe), str(n)], capture_output=True, text=True, check=True).stdout
        return json.loads(out)["zones"]

    def test_no_extra_room_is_an_empty_array(self):
        self.assertEqual(self.zones(0), [])

    def test_zone_with_data_has_values_and_is_active(self):
        z = self.zones(1)
        self.assertEqual(len(z), 1)
        self.assertEqual((z[0]["index"], z[0]["active"]), (1, True))
        self.assertAlmostEqual(z[0]["room_temp"], 21.5)
        self.assertAlmostEqual(z[0]["setpoint"], 22.0)

    def test_zone_without_data_has_null_values_and_is_not_active(self):
        z = self.zones(2)[1]
        self.assertEqual((z["index"], z["room_temp"], z["setpoint"], z["active"]), (2, None, None, False))

    def test_stale_and_off_zones_are_not_active(self):
        z = self.zones(4)
        self.assertEqual([x["active"] for x in z], [True, False, False, False])


class Wiring(unittest.TestCase):
    """Text checks (not a run)."""

    def test_wiring(self):
        self.assertRegex(SRC, r"if \(settings\.sat\.iZoneCount > 0\) satPublishZoneDiagnostics\(\);")
        self.assertRegex(SRC, r'publishIfChangedI\(F\("sat/zone_count"\),\s+\(int32_t\)settings\.sat\.iZoneCount,\s+s_zone_count,\s+true\);')
        self.assertIn("satEmitStatusZones(je, snap->zoneN, snap->zoneHas, snap->zoneActive, snap->zoneRoom, snap->zoneSp);", SRC)
        self.assertIn("snap->zoneN = zc;", SRC)


if __name__ == "__main__":
    unittest.main()
