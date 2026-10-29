#!/usr/bin/env python3
"""TASK-1478: the weather JSON parser must not wait on an open socket after the last byte.

Field capture: every Open-Meteo fetch blocked loop() for about 5 s. The HTTPClient default is
keep-alive, so the socket stays open after the closing '}' and wstreamGet() only gave up after
its 5000 ms idle timeout. Fix: HTTP/1.0 (http.useHTTP10(true)) plus an early return in
weatherParseStream() as soon as the top-level object closes.

The code under test is NOT re-implemented here. The test cuts the real wstreamGet / wstreamPeek /
wstreamSkipString / wstreamSkipArray / wstreamReadNumber / weatherParseStream and the forecast
arrays out of src/OTGW-firmware/SATweather.ino and compiles them on the host with stubs: a fake
WiFiClient over a byte buffer, a fake HTTPClient whose connected() stays true (keep-alive), and a
millis() that advances 1 ms per call (a 5 s idle wait shows as >= 5000 simulated ms).

OTGW_SATWEATHER_INO points at another copy, e.g. the output of
`git show HEAD:src/OTGW-firmware/SATweather.ino`, for the old-vs-fix run: on HEAD the parsed values
are right but the tail test fails (>= 5000 ms).

Without a host C++ compiler the compile tests are skipped, they do not pass silently.

Run: python tests/test_weather_stream_tail.py
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

WEATHER_INO = Path(os.environ.get("OTGW_SATWEATHER_INO",
                                  REPO / "src" / "OTGW-firmware" / "SATweather.ino"))

CURRENT_FIELDS = [
    ("time", '"2026-10-29T12:00"'), ("interval", "900"),
    ("temperature_2m", "11.4"), ("relative_humidity_2m", "83"), ("apparent_temperature", "8.9"),
    ("is_day", "1"), ("precipitation", "0.2"), ("rain", "0.2"), ("showers", "0.0"),
    ("snowfall", "0.0"), ("weather_code", "61"), ("cloud_cover", "97"), ("pressure_msl", "1008.3"),
    ("surface_pressure", "1006.9"), ("wind_speed_10m", "14.2"), ("wind_direction_10m", "231"),
    ("wind_gusts_10m", "33.1"),
]


def make_json() -> str:
    cur_units = ",".join('"%s":"u"' % k for k, _ in CURRENT_FIELDS)
    cur = ",".join('"%s":%s' % kv for kv in CURRENT_FIELDS)
    temp = [round(10.0 + i * 0.25, 2) for i in range(24)]
    dew = [round(7.5 + i * 0.1, 2) if i != 5 else "null" for i in range(24)]
    cloud = [(90 + i % 10) if i != 3 else "null" for i in range(24)]
    prec = [(i * 3) % 101 for i in range(24)]

    def arr(a):
        return "[" + ",".join(str(x) for x in a) + "]"
    hourly = ('"time":["2026-10-29T00:00","2026-10-29T01:00"],"temperature_2m":%s,"dew_point_2m":%s,'
              '"cloud_cover":%s,"precipitation_probability":%s,"relative_humidity_2m":%s'
              % (arr(temp), arr(dew), arr(cloud), arr(prec), arr([80] * 24)))
    return ('{"latitude":52.14,"longitude":6.18,"generationtime_ms":0.07,"utc_offset_seconds":0,'
            '"timezone":"GMT","elevation":12.0,"current_units":{%s},"current":{%s},'
            '"hourly_units":{"time":"iso8601","temperature_2m":"\\u00b0C"},"hourly":{%s}}'
            % (cur_units, cur, hourly))


STUB = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#define HAS_WEATHER_FORECAST 1
#define PSTR(x) x
#define strcmp_P strcmp

static uint32_t g_ms = 0;
static uint32_t millis() { return ++g_ms; }       // every call is 1 simulated ms
static void yield() {}

static std::string g_data;
static size_t g_pos = 0;
static uint32_t g_msAtLastByte = 0;

struct WiFiClient {
  int available() { return (int)(g_data.size() - g_pos); }
  int read() {
    if (g_pos >= g_data.size()) return -1;
    int c = (unsigned char)g_data[g_pos++];
    if (g_pos == g_data.size()) g_msAtLastByte = g_ms;
    return c;
  }
  int peek() { return g_pos < g_data.size() ? (unsigned char)g_data[g_pos] : -1; }
};
struct HTTPClient { bool connected() { return true; } };   // keep-alive: socket never closes

struct WeatherState {
  float fTemperature = -999, fApparentTemp = -999, fHumidity = -999, fWindSpeed = -999, fCloudCover = -999;
  float fWindDirection = -999, fWindGusts = -999, fPressureMsl = -999, fPrecipitation = -999;
  float fRain = -999, fSnowfall = -999;
  uint16_t iWeatherCode = 0; bool bIsDay = false;
};
struct SatState { WeatherState weather; };
struct OtgwState { SatState sat; };
static OtgwState state;

#include "weather_cut.inc"

int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "rb");
  char buf[8192]; size_t n = fread(buf, 1, sizeof(buf), f); fclose(f);
  g_data.assign(buf, n);
  WiFiClient c; HTTPClient h;
  uint32_t t0 = g_ms;
  weatherParseStream(&c, &h);
  uint32_t tail = g_ms - g_msAtLastByte;
  const WeatherState& w = state.sat.weather;
  printf("temp=%.2f apparent=%.2f hum=%.0f wind=%.1f cloud=%.0f dir=%.0f gusts=%.1f msl=%.1f precip=%.1f rain=%.1f snow=%.1f code=%u isday=%d\n",
         w.fTemperature, w.fApparentTemp, w.fHumidity, w.fWindSpeed, w.fCloudCover, w.fWindDirection,
         w.fWindGusts, w.fPressureMsl, w.fPrecipitation, w.fRain, w.fSnowfall, (unsigned)w.iWeatherCode, (int)w.bIsDay);
  printf("count=%u t0=%.2f t23=%.2f dew4=%.2f dew5=%.2f dew6=%.2f cloud3=%u cloud4=%u prec10=%u\n",
         (unsigned)_weather_forecastCount, _weather_forecastTemp[0], _weather_forecastTemp[23],
         _weather_forecastDewPt[4], _weather_forecastDewPt[5], _weather_forecastDewPt[6],
         (unsigned)_weather_forecastCloud[3], (unsigned)_weather_forecastCloud[4], (unsigned)_weather_forecastPrecipProb[10]);
  printf("tail_ms=%u total_ms=%u consumed=%zu of %zu\n", tail, g_ms - t0, g_pos, g_data.size());
  return 0;
}
'''


def cut(src: str) -> str:
    a = src.index("static const uint8_t  WEATHER_FORECAST_HOURS")
    b = src.index("#endif  // HAS_WEATHER_FORECAST", a)
    arrays = src[a:b]
    p = src.index("// Read one byte from stream")
    q = src.index("//=== HTTP Fetch ===")
    q = src.rindex("//====", 0, q)
    return arrays + "\n" + src[p:q]


def run_parser(workdir: Path, payload: str):
    cpp = workdir / "weather_host.cpp"
    (workdir / "weather_cut.inc").write_text(cut(WEATHER_INO.read_text(encoding="utf-8")), encoding="utf-8")
    cpp.write_text(STUB, encoding="utf-8")
    exe = compile_cpp(cpp, workdir / "weather_host.exe")
    if exe is None:
        return None
    (workdir / "in.json").write_text(payload, encoding="utf-8")
    r = subprocess.run([str(exe), str(workdir / "in.json")], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    out = {}
    for line in r.stdout.splitlines():
        for kv in line.split():
            if "=" in kv:
                k, v = kv.split("=", 1)
                out[k] = float(v)
    print("\n" + r.stdout.strip())
    return out


class WeatherStreamTail(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = run_parser(Path(cls.tmp.name), make_json())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if self.out is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def test_parsed_values(self):
        o = self.out
        self.assertAlmostEqual(o["temp"], 11.4, places=2)
        self.assertAlmostEqual(o["apparent"], 8.9, places=2)
        self.assertEqual(o["hum"], 83)
        self.assertAlmostEqual(o["wind"], 14.2, places=1)
        self.assertEqual(o["cloud"], 97)
        self.assertEqual(o["dir"], 231)
        self.assertAlmostEqual(o["gusts"], 33.1, places=1)
        self.assertAlmostEqual(o["msl"], 1008.3, places=1)
        self.assertEqual(o["code"], 61)
        self.assertEqual(o["isday"], 1)

    def test_forecast_values_with_null(self):
        o = self.out
        self.assertEqual(o["count"], 24)
        self.assertAlmostEqual(o["t0"], 10.0, places=2)
        self.assertAlmostEqual(o["t23"], 15.75, places=2)
        self.assertAlmostEqual(o["dew4"], 7.9, places=2)
        self.assertEqual(o["dew5"], 0.0)          # null becomes 0, position kept
        self.assertAlmostEqual(o["dew6"], 8.1, places=2)
        self.assertEqual(o["cloud3"], 0)          # null
        self.assertEqual(o["cloud4"], 94)
        self.assertEqual(o["prec10"], 30)

    def test_no_wait_after_last_byte(self):
        # keep-alive socket: connected() stays true and no more bytes come
        self.assertLess(self.out["tail_ms"], 1000,
                        "parser waited %d simulated ms after the last byte" % self.out["tail_ms"])


class FetchSource(unittest.TestCase):
    def test_http10_before_get(self):
        src = WEATHER_INO.read_text(encoding="utf-8")
        fn = src[src.index("static void weatherFetchOpenMeteo()\n{"):]
        a = fn.find("http.useHTTP10(true)")
        b = fn.find("http.GET()")
        self.assertGreater(a, 0, "weatherFetchOpenMeteo() does not call http.useHTTP10(true)")
        self.assertLess(a, b, "http.useHTTP10(true) must come before http.GET()")


if __name__ == "__main__":
    unittest.main(verbosity=2)
