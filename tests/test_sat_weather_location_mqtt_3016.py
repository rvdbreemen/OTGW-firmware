#!/usr/bin/env python3
"""TASK-3016: the SAT weather location (latitude, longitude) over MQTT.

Robert, #alpha-testing 2026-11-21 (msg 1573739569545871583, 1573739678606295074): "Use this MQTT implementation." /
"Yes, implement this request": an MQTT set command and a retained state topic for both values.

  - set/<node>/sat/weather_lat and sat/weather_lon store the value through updateSetting() (the same setting
    the REST path writes, SATweatherlat / SATweatherlon) when the WHOLE payload is one finite number inside
    -90..90 / -180..180. Anything else is ignored: the REST path clamps 95 to 90, but a clamped coordinate would
    store a wrong place.
  - state: sat/weather_lat and sat/weather_lon, retained, 4 decimals, through publishIfChangedF (ADR-111).

Nothing below re-implements the code under test: the harness cuts the real handlers, the real kSatMqttCmds table
and dispatchSatMqttCmd() out of MQTTstuff.ino and the real publish lines out of satPublishMQTT() (SATcontrol.ino)
and compiles them with a host C++ compiler. Without a compiler the tests are skipped, not passed.
Not reachable from the host: the real MQTT client and the broker wire format (bench-tester).

Old-vs-fix:  OTGW_SRC_REV=origin/dev python tests/test_sat_weather_location_mqtt_3016.py   -> red before TASK-3016
             python tests/test_sat_weather_location_mqtt_3016.py                           -> green
Run: python tests/test_sat_weather_location_mqtt_3016.py
"""
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_mqtt_contract_1485 import COMMON, build, read, run, segment, body  # noqa: E402

START = "// TASK-3016: weather location over MQTT."


class TCommand(unittest.TestCase):
    exe = None
    err = None

    @classmethod
    def setUpClass(cls):
        mq = read("MQTTstuff.ino")
        cls.tmp = tempfile.TemporaryDirectory()
        try:
            handlers = segment(mq, START, "static void _satResetIntegralCmd")
        except ValueError:
            cls.err = "the weather location handlers are not in MQTTstuff.ino"
            return
        types = segment(mq, "typedef void (*SatMqttHandler)(const char*);", "// Dispatch table.")
        table = segment(mq, "static const SatMqttCmdEntry kSatMqttCmds[] PROGMEM = {", "// Returns true when the sub-command")
        fn = body(mq, "static bool dispatchSatMqttCmd(const char* cmd, const char* payload)")
        names = set(re.findall(r"\{\s*\"[^\"]+\",\s*nullptr,\s*(\w+)\s*\}", table)) - {"_satWeatherLatCmd", "_satWeatherLonCmd"}
        stubs = "".join(f"static void {h}(const char*) {{ g_out.push_back(\"handler:{h}\"); }}\n" for h in sorted(names))
        code = COMMON + r"""
#define DebugTf(...) ((void)0)
static void updateSetting(const char* k, const char* v) { g_out.push_back(std::string("setting:") + k + "=" + v); }
""" + types + stubs + handlers + table + fn + r"""
int main(int argc, char** argv) {
  bool hit = dispatchSatMqttCmd(argv[1], argc > 2 ? argv[2] : "");
  printf("%s\n", hit ? "hit" : "miss");
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.exe = build(cls.tmp.name, "wxloc", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def dispatch(self, cmd, val):
        if self.err:
            self.fail(self.err)
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return run(self.exe, cmd, val)

    def test_valid_latitude_is_stored(self):
        self.assertEqual(self.dispatch("weather_lat", "52.3702"), ["hit", "setting:SATweatherlat=52.3702"])

    def test_valid_longitude_is_stored(self):
        self.assertEqual(self.dispatch("weather_lon", "-122.4194"), ["hit", "setting:SATweatherlon=-122.4194"])

    def test_range_edges_are_accepted(self):
        for cmd, val in (("weather_lat", "90"), ("weather_lat", "-90"), ("weather_lon", "180"), ("weather_lon", "-180"), ("weather_lat", "0")):
            key = "SATweatherlat" if cmd.endswith("lat") else "SATweatherlon"
            self.assertEqual(self.dispatch(cmd, val), ["hit", f"setting:{key}={val}"], (cmd, val))

    def test_out_of_range_is_rejected_not_clamped(self):
        for cmd, val in (("weather_lat", "95"), ("weather_lat", "-90.1"), ("weather_lon", "180.5"), ("weather_lon", "-181")):
            self.assertEqual(self.dispatch(cmd, val), ["hit"], (cmd, val))

    def test_garbage_is_rejected(self):
        for cmd, val in (("weather_lat", "abc"), ("weather_lat", "52x"), ("weather_lat", "5 2"), ("weather_lon", "nan"),
                         ("weather_lon", "inf"), ("weather_lat", "-inf"), ("weather_lat", ""), ("weather_lon", "1e999")):
            self.assertEqual(self.dispatch(cmd, val), ["hit"], (cmd, val))

    def test_other_commands_still_dispatch(self):
        self.assertEqual(self.dispatch("zone_count", "2"), ["hit", "setting:SATzonecount=2"])


class TState(unittest.TestCase):
    exe = None
    err = None

    @classmethod
    def setUpClass(cls):
        sat = read("SATcontrol.ino")
        cls.tmp = tempfile.TemporaryDirectory()
        lines = [l.strip() for l in sat.splitlines() if re.search(r'publishIfChangedF\(F\("sat/weather_l(at|on)"\)', l)]
        if len(lines) != 2:
            cls.err = f"expected 2 publishIfChangedF lines for sat/weather_lat|lon in satPublishMQTT, found {len(lines)}"
            return
        cls.src_lines = lines
        decl = [l.strip() for l in sat.splitlines() if re.search(r"static SATShadowF s_weather_lat, s_weather_lon;", l)]
        if not decl:
            cls.err = "the shadows s_weather_lat / s_weather_lon are not declared"
            return
        hdr = read("SATmqttPublish.h")
        eps = re.search(r"static constexpr float SAT_EPS_COORD\s*=\s*([0-9.]+f);", hdr)
        if not eps:
            cls.err = "SAT_EPS_COORD is not defined in SATmqttPublish.h"
            return
        code = COMMON + r"""
struct SATShadowF { float last; unsigned nextRepublishMs; bool valid; unsigned char epoch; };
struct SatSettings { float fWeatherLat = 52.3702f; float fWeatherLon = 4.8952f; };
struct Settings { SatSettings sat; }; static Settings settings;
static constexpr float SAT_EPS_COORD = """ + eps.group(1) + r""";
static bool publishIfChangedF(const char* topic, float v, SATShadowF& sh, float eps, int dec, bool retained) {
  char b[96]; snprintf(b, sizeof b, "%s|%.*f|%s|%g", topic, dec, v, retained ? "retained" : "volatile", eps);
  g_out.push_back(b); return true;
}
int main() {
  """ + decl[0] + "\n  " + "\n  ".join(lines) + r"""
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.exe = build(cls.tmp.name, "wxstate", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def out(self):
        if self.err:
            self.fail(self.err)
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return run(self.exe)

    def test_both_state_topics_are_retained_with_four_decimals(self):
        got = self.out()
        self.assertEqual(got[0].split("|")[:3], ["sat/weather_lat", "52.3702", "retained"])
        self.assertEqual(got[1].split("|")[:3], ["sat/weather_lon", "4.8952", "retained"])

    def test_change_threshold_is_below_the_published_resolution(self):
        eps = float(self.out()[0].split("|")[3])
        self.assertLess(eps, 0.0001)


if __name__ == "__main__":
    unittest.main(verbosity=2)
