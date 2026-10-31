#!/usr/bin/env python3
"""TASK-1441: Thermal comfort humidity comes from indoor sources only (entry 117, Sergeant D Q57 A; climate.py:282-287).

Nothing is re-implemented. The test cuts the real code out of SATcontrol.ino and compiles it with a host C++ compiler:
  - satGetRoomTemp() (with satHumidityFresh() in front of it on the fix) and satCalcSimmerIndex();
  - satHandleHumidity() and, on the fix, satNoteBleHumidity() (the BLE room sensor -> humidity bridge);
  - the humidity block of satControlLoopRun() between "// --- Humidity input" and "// --- PV-surplus" (HEAD: the
    weather-humidity fallback; fix: the block is gone and the step is empty);
  - the published Summer Simmer Index block of satPublishMQTT() (from "Summer Simmer Index (Task #64)" up to the
    "Modulation state" banner), with publishIfChangedF/S stubbed to count calls.
The BLE side (satBLEUpdateState() copying the roster snapshot) needs the BLE stack and is not compiled; the bridge
is tested at its function boundary and its call site is source-checked.

Scenarios (Thermal comfort on, room 24.0 C, timeout 1800 s), each prints "room=<C> pub=<0|1>":
  weather  weather humidity 80 % valid, no indoor humidity -> room stays 24.0, nothing published (HEAD: SSI used)
  push     pushed 70 % just now                            -> SSI used and published
  stale    pushed 70 % 31 min ago                          -> room 24.0 and nothing published (HEAD: still published)
  ble      BLE reading 65 % just now                       -> SSI used and published (HEAD: ignored)
  blecache the same BLE reading seen again 31 min later    -> stamp not refreshed: stale again
  blenan   NaN BLE humidity, nothing pushed                -> not used

OTGW_SAT_INO points at another copy (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the old-vs-fix run.

Run: python tests/test_sat_humidity_source.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))
BLE_INO = SRC / "SATble.ino"

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define SATTraceTf(...) ((void)0)
#define SATTraceTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
#define SATDebugTln(x) ((void)0)
struct SatSettings { bool bSimulation=false, bBleEnable=false, bMultiArea=false, bUseExternalTemp=false, bThermalComfort=true;
                     int iMultiAreaCount=0; uint16_t iHumidityTimeoutS=1800; uint32_t iSensorMaxAgeS=21600; };
struct Settings { SatSettings sat; };
struct Weather { bool bValid=false; float fHumidity=0; uint32_t iLastUpdateMs=0; };
struct SatState { bool bBleTempValid=false, bExternalTempValid=false, bFallbackActive=false, bHumidityValid=false;
                  float fSimRoomTemp=0, fBleTemp=0, fExternalTemp=0, fHumidity=0; uint32_t iSimDropoutExpiryMs=0,
                  iBleTempLastMs=0, iExternalTempLastMs=0, iLastKnownRoomMs=0, iHumidityLastMs=0; Weather weather; };
struct OtBusState { uint32_t iTrThermostatMs = 0; };
struct State { SatState sat; OtBusState otBus; };
struct OTSys { float Tr = 24.0f; }; static OTSys OTcurrentSystemState;
static Settings settings; static State state;
static const uint32_t SAT_STALE_TEMP_BLE_MS = 300000UL, SAT_STALE_TEMP_MS = 300000UL;
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static float satGetWeightedRoomTemp() { return NAN; }
static float satGetOutsideTemp() { return 10.0f; }
static float satEstimateRoomTemp(float) { return 20.0f; }
static const float SAT_EPS_ERROR = 0, SAT_EPS_TEMP_COARSE = 0;
struct Shadow {};
static Shadow s_ssi, s_summer_simmer_index, s_summer_simmer_perception;
static int g_pub = 0;
static void publishIfChangedF(const char*, float, Shadow&, float, int, bool) { g_pub++; }
static void publishIfChangedS(const char*, const char*, Shadow&, bool) { g_pub++; }
@ROOM@
@HUM@
static void humidityInputStep() {
@INPUT@
}
static void publishSsiBlock() {
@PUB@
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  const uint32_t T0 = 10000000UL;
  g_ms = T0;
  if (!strcmp(sc, "weather")) {
    state.sat.weather.bValid = true; state.sat.weather.fHumidity = 80.0f; state.sat.weather.iLastUpdateMs = g_ms;
  } else if (!strcmp(sc, "push")) {
    satHandleHumidity("70");
  } else if (!strcmp(sc, "stale")) {
    satHandleHumidity("70"); g_ms = T0 + 31UL * 60000UL;
  } else if (!strcmp(sc, "ble")) {
@BLECALL1@
  } else if (!strcmp(sc, "blecache")) {
@BLECALL1@
    g_ms = T0 + 31UL * 60000UL;
@BLECALL2@
  } else if (!strcmp(sc, "blenan")) {
@BLECALLNAN@
  }
  humidityInputStep();
  g_pub = 0;
  publishSsiBlock();
  printf("room=%.2f pub=%d\n", satGetRoomTemp(), g_pub > 0);
  return 0;
}
"""


def between(src, start, end, include_start=True):
    a = src.index(start)
    b = src.index(end, a)
    return src[a:b] if include_start else src[a + len(start):b]


def build_cpp(src: str) -> str:
    room_start = "static bool satHumidityFresh" if "static bool satHumidityFresh" in src else "static float satGetRoomTemp()"
    # satHumidityFresh and satGetRoomTemp are both needed: cut to the end of satGetRoomTemp()
    a = src.index(room_start)
    b = src.index("\n  return otRoom;\n}\n", a) + len("\n  return otRoom;\n}\n")
    room = src[a:b]
    sa = src.index("static float satCalcSimmerIndex")
    sb = src.index("Circulatory Collapse Imminent", sa)
    sb = src.index("\n}\n", sb) + 3
    room = src[sa:sb] + room
    hum = between(src, "bool satHandleHumidity", "// --- Sun elevation handler")
    if "--- Humidity input" in src:
        inp = between(src, "  // --- Humidity input", "  // --- PV-surplus")
    else:
        inp = ""
    pa = src.index("Summer Simmer Index (Task #64): requires")
    pb = src.rindex("\n  // ---", pa, src.index("Modulation state + PWM state", pa))
    pub = src[pa - 4:pb]  # "// " of the comment line start
    ble_call = "    satNoteBleHumidity(65.0f, g_ms);" if "satNoteBleHumidity" in src else \
        "    /* HEAD: BLE humidity never reaches fHumidity */"
    ble_call2 = "    satNoteBleHumidity(65.0f, T0);" if "satNoteBleHumidity" in src else ""
    ble_nan = "    satNoteBleHumidity(NAN, g_ms);" if "satNoteBleHumidity" in src else ""
    return (STUBS.replace("@ROOM@", room).replace("@HUM@", hum).replace("@INPUT@", inp).replace("@PUB@", pub)
            .replace("@BLECALL1@", ble_call.replace("g_ms", "T0")).replace("@BLECALL2@", ble_call2)
            .replace("@BLECALLNAN@", ble_nan))


class HumiditySource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout
        m = re.match(r"room=([-\d.]+) pub=(\d)", out)
        return float(m.group(1)), int(m.group(2))

    def test_a_weather_humidity_is_not_an_indoor_source(self):
        room, pub = self.run_sc("weather")
        self.assertEqual(room, 24.0, "no indoor humidity: Thermal comfort uses the room temperature")
        self.assertEqual(pub, 0, "no SSI published without indoor humidity")

    def test_b_fresh_pushed_humidity_is_used(self):
        room, pub = self.run_sc("push")
        self.assertGreater(room, 24.5, "SSI of 24 C at 70 % is above the raw temperature")
        self.assertEqual(pub, 1)

    def test_c_stale_pushed_humidity_is_not_used_nor_published(self):
        room, pub = self.run_sc("stale")
        self.assertEqual(room, 24.0, "older than the timeout: raw room temperature")
        self.assertEqual(pub, 0, "a stale humidity must stop the published SSI")

    def test_d_ble_humidity_becomes_the_humidity_used(self):
        room, pub = self.run_sc("ble")
        self.assertGreater(room, 24.5, "BLE humidity 65 % feeds Thermal comfort")
        self.assertEqual(pub, 1)

    def test_e_cached_ble_value_does_not_refresh_the_stamp(self):
        room, pub = self.run_sc("blecache")
        self.assertEqual(room, 24.0, "the same BLE reading (same stamp) must not make the humidity fresh again")
        self.assertEqual(pub, 0)

    def test_f_nan_ble_humidity_does_nothing(self):
        room, pub = self.run_sc("blenan")
        self.assertEqual(room, 24.0)
        self.assertEqual(pub, 0)

    def test_g_ble_call_site_and_timestamp_in_source(self):
        ble = BLE_INO.read_text(encoding="utf-8")
        self.assertIn("satNoteBleHumidity(snap.fHumidity, snap.iHumidityMs)", ble)
        self.assertIn("_bleRuntime[slot].iHumidityMs  = millis();", ble)


if __name__ == "__main__":
    unittest.main(verbosity=2)
