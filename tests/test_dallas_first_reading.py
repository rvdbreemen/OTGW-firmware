#!/usr/bin/env python3
"""TASK-1824: the first Dallas reading after boot must not publish the DS18B20 power-on value 85.0.

initSensors() sets setWaitForConversion(false), so requestTemperatures() only STARTS a conversion
and getTempC() reads the scratchpad, which holds the result of the previous conversion. On the
first poll after boot no conversion has completed and the scratchpad holds 85.0; the old code
stored and published it. A sensor that browns out returns to 85.0 the same way.

This test cuts the REAL pollSensors() (and, on the fix, the 'dallas-read record' block before it)
out of sensors_ext.ino and compiles it on the host against a model of the DS18B20 scratchpad:
85.0 at power-on, a conversion started by requestTemperatures() completes before the next poll
(the poll interval is 1..3600 s, a 12-bit conversion takes at most 750 ms), getTempC() returns the
scratchpad. MQTT publishes are recorded. Nothing of pollSensors() is re-implemented.

Old-vs-fix: OTGW_SENSORS_INO=<path to another sensors_ext.ino>
(e.g. `git show origin/dev:src/OTGW-firmware/sensors_ext.ino`).
Run: python tests/test_dallas_first_reading.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("OTGW_SENSORS_INO") or HERE.parent / "src" / "OTGW-firmware" / "sensors_ext.ino")
TEXT = SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
BEGIN, END = "// begin dallas-read record", "// end dallas-read record"


def poll_sensors_body(text):
    i = text.index(" void pollSensors()")
    j = text.index("\n}\n", i)
    return text[i:j + 3]


def record_block(text):
    i, j = text.find(BEGIN), text.find(END)
    return text[i:j] if i >= 0 and j > i else ""   # absent on the old code


PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define PSTR(s) (s)
#define snprintf_P snprintf
#define DEVICE_DISCONNECTED_C -127.0f
typedef uint8_t DeviceAddress[8];
static void DebugTf(const char*, ...) {}
static void DebugFlush() {}
static long random(long a, long b) { return a; }
static const float SIM_SENSOR_MIN = 20.0f, SIM_SENSOR_MAX = 60.0f;
static const uint32_t SIM_SENSOR_UPDATE_INTERVAL_SECONDS = 10;
static time_t simLastUpdateTime = 0;
static time_t g_now = 1000000;
#define time(x) (g_now)

struct DebugState { bool bSensorSim = false, bSensors = false; };
struct State { DebugState debug; } state;
struct MqttSettings { bool bEnable = true; };
struct SatSettings { char sSensorArea[4][18] = {{0}}; };
struct Settings { MqttSettings mqtt; SatSettings sat; } settings;
static bool bSensorsDetected = true;
static const int OTGWdallasdataid = 246;
static bool getMQTTConfigDone(int) { return true; }
static void setMQTTConfigPending(int) {}
static void satSetAreaTemp(int, float) {}

struct { int id; DeviceAddress addr; float tempC; time_t lasttime; } DallasrealDevice[16];
static int DallasrealDeviceCount = 1;
static char* getDallasAddress(DeviceAddress) { static char a[] = "28FF000000000001"; return a; }

// The published values, in order.
static float g_pub[64]; static int g_npub = 0;
static void sendMQTTData(const char*, const char* msg) { if (g_npub < 64) g_pub[g_npub++] = (float)atof(msg); }

// DS18B20 scratchpad model: 85.0 at power-on; requestTemperatures() completes the conversion that
// an earlier request started (the poll interval is far longer than a conversion) and starts a new one.
struct FakeDs18b20 {
  float scratch = 85.0f;      // power-on value
  bool  converting = false;
  float measured = 21.5f;     // what the sensor would measure now
  void requestTemperatures() { if (converting) scratch = measured; converting = true; }
  float getTempC(const uint8_t*) { return scratch; }
  void powerOnReset() { scratch = 85.0f; converting = false; }
} sensors;
"""

MAIN = r"""
// argv[1] = scenario. Prints the published values, one per line.
int main(int argc, char** argv) {
  const char* sc = argv[1];
  auto poll = [&]() { pollSensors(); g_now += 20; };
  if (!strcmp(sc, "boot")) {               // power-on, sensor at 21.5 °C, three polls
    poll(); poll(); poll();
  } else if (!strcmp(sc, "heat")) {        // a real temperature that climbs to 85.0 and stays
    const float t[] = {80.0f, 83.0f, 85.0f, 85.0f};
    for (float v : t) { sensors.measured = v; poll(); }
    sensors.measured = 85.0f; poll();
  } else if (!strcmp(sc, "brownout")) {    // valid readings, then the sensor resets
    poll(); poll(); poll();
    sensors.powerOnReset(); poll(); poll();
  } else if (!strcmp(sc, "disconnected")) {
    poll(); poll();
    sensors.scratch = DEVICE_DISCONNECTED_C; sensors.converting = false; poll();
  }
  for (int i = 0; i < g_npub; i++) printf("%.1f\n", g_pub[i]);
  return 0;
}
"""


class DallasFirstReading(unittest.TestCase):
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "dallas.cpp"
        cpp.write_text(PRELUDE + record_block(TEXT) + "\n" + poll_sensors_body(TEXT) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("dallas.exe" if os.name == "nt" else "dallas"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def published(self, sc):
        out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout
        return [float(x) for x in out.split()]

    def test_first_reading_after_boot_is_not_the_power_on_value(self):
        pub = self.published("boot")
        self.assertNotIn(85.0, pub, "the power-on scratchpad value reached MQTT")
        self.assertTrue(pub, "a completed conversion is published")
        self.assertEqual(pub[0], 21.5)

    def test_a_real_85_reached_step_by_step_is_still_published(self):
        pub = self.published("heat")
        self.assertIn(85.0, pub)
        self.assertEqual(pub[-1], 85.0)

    def test_a_brownout_back_to_85_is_not_published_and_the_old_value_stays(self):
        pub = self.published("brownout")
        self.assertNotIn(85.0, pub)
        self.assertEqual(set(pub), {21.5})

    def test_a_disconnected_sensor_is_still_skipped(self):
        self.assertNotIn(-127.0, self.published("disconnected"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
