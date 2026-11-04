#!/usr/bin/env python3
"""TASK-1604 (TASK-1471 R1, R2): the weather GET runs on a one-shot worker task, and the PIC refresh
stops after a failed update check.

The code under test is NOT re-implemented here. The compiled tests cut the real weather parser, the
WeatherResult scratch struct and the worker/handoff/scheduler block (weatherFetchWork ..
weatherLoop) out of src/OTGW-firmware/SATweather.ino and compile them on the host with stubs: a fake
HTTPClient/WiFiClient over a byte buffer, a platformTaskCreatePinned stub that only records the call
(the test runs the worker body by hand, so "worker" and "loop" interleave deterministically), a
controllable millis() and heap level.

Source tests (no compiler needed):
  S1 http.GET() exists only in weatherFetchWork(); the loop-path functions never call it
  S2 weatherFetchWork() sets a connect timeout of at most 2000 ms
  S3 the worker is created through the platform shim, not a raw FreeRTOS call
  S4 R2: refreshpic() returns after an empty checkforupdatepic() answer, before any GET
Compiled tests:
  C1 handoff: nothing is written to state.sat.weather before the loop task consumes the result;
     consuming applies it once and a second consume is a no-op (idempotent)
  C2 busy guard: no second worker while one runs or while its result is unconsumed
  C3 heap CRITICAL: no worker is started
  C4 failure path and first-fetch backoff (60 s, 120 s) keep working through the handoff
  C5 task creation failure counts one fetch error and leaves the state IDLE

OTGW_SATWEATHER_INO / OTGW_CORE_INO point at another copy (for example the output of
`git show HEAD:src/OTGW-firmware/SATweather.ino`) for the old-vs-fix run.

Run: python tests/test_weather_worker_handoff.py
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
from test_weather_stream_tail import make_json  # noqa: E402

WEATHER_INO = Path(os.environ.get("OTGW_SATWEATHER_INO", REPO / "src" / "OTGW-firmware" / "SATweather.ino"))
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8")


def function_chunks(src: str):
    """Split a .ino into top-level chunks that end at a line holding only '}'."""
    return src.split("\n}\n")


# ---------------------------------------------------------------- source tests
class SourceChecks(unittest.TestCase):
    def test_s1_get_only_in_worker(self):
        src = read(WEATHER_INO)
        holders = [c for c in function_chunks(src) if "http.GET()" in c]
        self.assertEqual(len(holders), 1, "expected exactly one http.GET()")
        self.assertIn("weatherFetchWork()", holders[0],
                      "http.GET() must live in weatherFetchWork() (worker task), not in a loop-path function")
        # the loop-path functions must not touch HTTP at all
        for name in ("weatherLoop", "weatherFetch", "weatherRequestFetch", "weatherConsumeResult",
                     "weatherApplyResult", "weatherSeedWork", "weatherFetchOpenMeteoNow"):
            m = re.search(r"\n(?:static )?\w+ %s\([^)]*\)\n\{\n(.*?)\n\}\n" % name, src, re.S)
            self.assertIsNotNone(m, name + " not found")
            for bad in ("http.GET", "HTTPClient", "weatherParseStream", "weatherFetchWork("):
                self.assertNotIn(bad, m.group(1), "%s() must not use %s" % (name, bad))

    def test_s2_connect_timeout(self):
        src = read(WEATHER_INO)
        self.assertIn("http.setConnectTimeout(WEATHER_CONNECT_TIMEOUT_MS)", src)
        m = re.search(r"WEATHER_CONNECT_TIMEOUT_MS\s*=\s*(\d+)UL", src)
        self.assertIsNotNone(m)
        self.assertLessEqual(int(m.group(1)), 2000)

    def test_s3_shim_not_raw_freertos(self):
        src = read(WEATHER_INO)
        self.assertIn("platformTaskCreatePinned(weatherWorkerBody", src)
        self.assertNotIn("xTaskCreate", src)
        self.assertNotIn("vTaskDelete", src)
        self.assertIn("platformTaskExit();", src)

    def test_s4_r2_refreshpic_stops_on_empty_version(self):
        src = read(CORE_INO)
        a = src.index("void refreshpic(String filename, String version)")
        body = src[a:src.index("\n}\n", a)]
        p1 = body.index("latest = checkforupdatepic(filename);")
        p2 = body.index("if (latest != version)")
        guard = body[p1:p2]
        self.assertRegex(guard, r"if \(latest\.length\(\) == 0\)\s*\{[^}]*return;",
                         "refreshpic() must return when checkforupdatepic() gave no version")
        self.assertLess(p2, body.index("http.GET()"))


# ---------------------------------------------------------------- compiled harness
STUB = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#define HAS_WEATHER_FORECAST 1
#define PSTR(x) x
#define F(x) x
#define strcmp_P strcmp
#define snprintf_P snprintf
#define DebugTf(...) ((void)0)
#define DebugTln(x) ((void)0)
#ifndef __GNUC__   // MSVC host: the harness is single threaded, so plain accesses stand in for the builtins
#define __ATOMIC_ACQUIRE 0
#define __ATOMIC_RELEASE 0
#define __atomic_load_n(p, o) (*(p))
#define __atomic_store_n(p, v, o) ((void)(*(p) = (v)))
#endif

static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static void yield() {}

static std::string g_data;
static size_t g_pos = 0;
struct WiFiClient {
  int available() { return (int)(g_data.size() - g_pos); }
  int read() { return g_pos < g_data.size() ? (unsigned char)g_data[g_pos++] : -1; }
  int peek() { return g_pos < g_data.size() ? (unsigned char)g_data[g_pos] : -1; }
};
static int g_httpCode = 200;
static uint32_t g_connectTimeout = 0;
static int g_getCalls = 0;
static WiFiClient g_stream;
struct HTTPClient {
  void setConnectTimeout(uint32_t t) { g_connectTimeout = t; }
  void setTimeout(uint32_t) {}
  bool begin(WiFiClient&, const char*) { return true; }
  void useHTTP10(bool) {}
  int GET() { g_getCalls++; return g_httpCode; }
  WiFiClient* getStreamPtr() { return &g_stream; }
  void end() {}
  bool connected() { return true; }
};
// TASK-1605: the worker pre-connects by IP (networkStuff.ino); the connect order itself is
// tested in test_no_hostbyname.py. Here the connection always succeeds.
static bool netConnectUrl(WiFiClient&, const char*, uint32_t, uint32_t) { return true; }

struct WeatherState {
  float fTemperature = 0, fApparentTemp = 0, fHumidity = 0, fWindSpeed = 0, fCloudCover = 0;
  float fWindDirection = 0, fWindGusts = 0, fPressureMsl = 0, fPrecipitation = 0, fRain = 0, fSnowfall = 0;
  uint16_t iWeatherCode = 0; bool bIsDay = false;
  bool bValid = false; uint32_t iLastUpdateMs = 0; uint16_t iFetchErrors = 0;
};
struct SatState { WeatherState weather; };
struct OtgwState { SatState sat; };
static OtgwState state;
struct SatSettings { bool bWeatherEnable = true; float fWeatherLat = 52.1f, fWeatherLon = 6.2f; uint16_t iWeatherInterval = 900; };
struct Settings { SatSettings sat; };
static Settings settings;
struct { float Toutside = 0.0f; } OTcurrentSystemState;

enum HeapHealthLevel { HEAP_HEALTHY, HEAP_LOW, HEAP_WARNING, HEAP_CRITICAL };
static HeapHealthLevel g_heap = HEAP_HEALTHY;
static HeapHealthLevel getHeapHealth() { return g_heap; }

using PlatformTask = void*;
static int g_createCalls = 0;
static bool g_createFails = false;
static void (*g_workerFn)(void*) = nullptr;
static PlatformTask platformTaskCreatePinned(void (*fn)(void*), const char*, uint32_t, void*, unsigned) {
  g_createCalls++;
  if (g_createFails) return nullptr;
  g_workerFn = fn;
  return (PlatformTask)1;
}
static void platformTaskExit() {}

static int g_restartTimer = 0;
static bool g_due = false;
#define RESTART_TIMER(t) (g_restartTimer++)
#define DUE(t) (g_due)
static int g_dumps = 0;
static void weatherDumpStateToTelnet() { g_dumps++; }

static const char kWeatherUrlFmt[] = "http://x/?lat=%.4f&lon=%.4f";
static const uint16_t WEATHER_POLL_MIN_SEC = 300;
static const uint32_t WEATHER_FIRST_RETRY_MS = 60000UL;

#include "weather_cut.inc"

static void runWorker(const std::string& body, int code) {
  g_data = body; g_pos = 0; g_httpCode = code;
  g_workerFn(nullptr);
}
#define P(k, v) printf("%s=%g\n", k, (double)(v))

int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "rb");
  static char buf[8192]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f);
  std::string json(buf, n);
  const WeatherState& w = state.sat.weather;

  // ---- C1 handoff + idempotence
  P("c1_req", weatherRequestFetch());
  P("c1_create", g_createCalls);
  P("c1_connect_timeout_set_before_run", 0);
  g_ms = 1000;
  runWorker(json, 200);
  P("c1_connect_timeout", g_connectTimeout);
  P("c1_gets", g_getCalls);
  P("c1_temp_before_consume", w.fTemperature);
  P("c1_valid_before_consume", w.bValid);
  P("c1_errors_before_consume", w.iFetchErrors);
  g_ms = 2000;
  weatherConsumeResult();
  P("c1_temp", w.fTemperature);
  P("c1_hum", w.fHumidity);
  P("c1_valid", w.bValid);
  P("c1_lastupdate", w.iLastUpdateMs);
  P("c1_fc_t0", _weather_forecastTemp[0]);
  P("c1_fc_count", _weather_forecastCount);
  P("c1_timer_restarts", g_restartTimer);
  g_ms = 9000;
  weatherConsumeResult();
  P("c1_lastupdate_after_second", w.iLastUpdateMs);
  P("c1_timer_restarts_after_second", g_restartTimer);
  P("c1_errors", w.iFetchErrors);

  // ---- C2 busy guard
  int c0 = g_createCalls;
  P("c2_first", weatherRequestFetch());
  P("c2_second_while_running", weatherRequestFetch());
  P("c2_create_delta", g_createCalls - c0);
  runWorker(json, 200);                       // READY, not consumed
  P("c2_third_while_ready", weatherRequestFetch());
  P("c2_create_delta_ready", g_createCalls - c0);
  weatherConsumeResult();
  P("c2_after_consume", weatherRequestFetch());
  P("c2_create_delta_final", g_createCalls - c0);
  runWorker(json, 200); weatherConsumeResult();

  // ---- C3 heap critical
  g_heap = HEAP_CRITICAL; c0 = g_createCalls;
  P("c3_req", weatherRequestFetch());
  P("c3_create_delta", g_createCalls - c0);
  g_heap = HEAP_HEALTHY;

  // ---- C5 create failure
  g_createFails = true; int e0 = w.iFetchErrors;
  P("c5_req", weatherRequestFetch());
  P("c5_errors_delta", w.iFetchErrors - e0);
  g_createFails = false;
  P("c5_req_after", weatherRequestFetch());     // state must be IDLE again, so this starts
  runWorker(json, 200); weatherConsumeResult();

  // ---- C4 failure path and first-fetch backoff through weatherLoop
  state.sat.weather.bValid = false; g_restartTimer = 0; g_ms = 400000UL;
  c0 = g_createCalls;
  weatherLoop();                               // first try starts
  P("c4_started", g_createCalls - c0);
  weatherLoop();                               // busy, no second worker
  P("c4_still_one", g_createCalls - c0);
  runWorker(json, 503);
  e0 = w.iFetchErrors;
  weatherLoop();                               // consume the failure, backoff 60 s
  P("c4_errors_delta", w.iFetchErrors - e0);
  P("c4_valid", w.bValid);
  P("c4_retry_ms", _wxFirstRetryMs);
  weatherLoop();
  P("c4_no_retry_inside_60s", g_createCalls - c0);
  g_ms += 60001;
  weatherLoop();                               // second try
  P("c4_second_try", g_createCalls - c0);
  runWorker(json, 503);
  weatherLoop();
  P("c4_retry_ms_2", _wxFirstRetryMs);
  g_ms += 120001;
  weatherLoop();
  runWorker(json, 200);
  weatherLoop();                               // success: backoff cleared, poll timer restarted
  P("c4_valid_after_success", w.bValid);
  P("c4_retry_ms_after_success", _wxFirstRetryMs);
  P("c4_timer_restarted", g_restartTimer);

  // ---- manual fetch dumps once on consume
  g_dumps = 0;
  weatherFetchOpenMeteoNow();
  runWorker(json, 200);
  weatherConsumeResult();
  P("manual_dumps", g_dumps);
  return 0;
}
'''


def cut(src: str) -> str:
    a = src.index("static const uint8_t  WEATHER_FORECAST_HOURS")
    b = src.index("#endif  // HAS_WEATHER_FORECAST", a)
    arrays = src[a:b]
    sa = src.index("struct WeatherResult")
    sb = src.index("static WeatherResult _wxWork;") + len("static WeatherResult _wxWork;")
    p = src.index("// Read one byte from stream")
    q = src.index("//=== HTTP Fetch (worker task)")
    q = src.rindex("//====", 0, q)
    h1 = src.index("static const uint8_t WX_IDLE")
    h2 = src.index("//=== REST API: GET /api/v2/sat/weather ===")
    h2 = src.rindex("//====", 0, h2)
    return arrays + "\n" + src[sa:sb] + "\n" + src[p:q] + "\n" + src[h1:h2]


def run_harness(workdir: Path):
    (workdir / "weather_cut.inc").write_text(cut(read(WEATHER_INO)), encoding="utf-8")
    cpp = workdir / "handoff_host.cpp"
    cpp.write_text(STUB, encoding="utf-8")
    exe = compile_cpp(cpp, workdir / "handoff_host.exe")
    if exe is None:
        return None
    (workdir / "in.json").write_text(make_json(), encoding="utf-8")
    r = subprocess.run([str(exe), str(workdir / "in.json")], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    out = {}
    for line in r.stdout.splitlines():
        k, v = line.split("=", 1)
        out[k] = float(v)
    print("\n" + r.stdout.strip())
    return out


class Compiled(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        try:
            cls.out = run_harness(Path(cls.tmp.name))
            cls.err = None
        except ValueError as e:        # old code: the handoff block does not exist
            cls.out, cls.err = None, e

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if self.err is not None:
            self.fail("handoff block not found in the source: %s" % self.err)
        if self.out is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def test_c1_handoff_and_idempotence(self):
        o = self.out
        self.assertEqual(o["c1_req"], 1)
        self.assertEqual(o["c1_create"], 1)
        self.assertEqual(o["c1_gets"], 1)
        self.assertEqual(o["c1_connect_timeout"], 2000)
        # the worker ran, but nothing reached the SAT state yet
        self.assertEqual(o["c1_temp_before_consume"], 0)
        self.assertEqual(o["c1_valid_before_consume"], 0)
        self.assertEqual(o["c1_errors_before_consume"], 0)
        # consume applies it
        self.assertAlmostEqual(o["c1_temp"], 11.4, places=2)
        self.assertEqual(o["c1_hum"], 83)
        self.assertEqual(o["c1_valid"], 1)
        self.assertEqual(o["c1_lastupdate"], 2000)
        self.assertAlmostEqual(o["c1_fc_t0"], 10.0, places=2)
        self.assertEqual(o["c1_fc_count"], 24)
        self.assertEqual(o["c1_timer_restarts"], 1)
        # a second consume does nothing
        self.assertEqual(o["c1_lastupdate_after_second"], 2000)
        self.assertEqual(o["c1_timer_restarts_after_second"], 1)
        self.assertEqual(o["c1_errors"], 0)

    def test_c2_busy_guard(self):
        o = self.out
        self.assertEqual(o["c2_first"], 1)
        self.assertEqual(o["c2_second_while_running"], 0)
        self.assertEqual(o["c2_create_delta"], 1)
        self.assertEqual(o["c2_third_while_ready"], 0)
        self.assertEqual(o["c2_create_delta_ready"], 1)
        self.assertEqual(o["c2_after_consume"], 1)
        self.assertEqual(o["c2_create_delta_final"], 2)

    def test_c3_heap_critical(self):
        self.assertEqual(self.out["c3_req"], 0)
        self.assertEqual(self.out["c3_create_delta"], 0)

    def test_c5_create_failure(self):
        self.assertEqual(self.out["c5_req"], -1)
        self.assertEqual(self.out["c5_errors_delta"], 1)
        self.assertEqual(self.out["c5_req_after"], 1)

    def test_c4_backoff(self):
        o = self.out
        self.assertEqual(o["c4_started"], 1)
        self.assertEqual(o["c4_still_one"], 1)
        self.assertEqual(o["c4_errors_delta"], 1)
        self.assertEqual(o["c4_valid"], 0)
        self.assertEqual(o["c4_retry_ms"], 60000)
        self.assertEqual(o["c4_no_retry_inside_60s"], 1)
        self.assertEqual(o["c4_second_try"], 2)
        self.assertEqual(o["c4_retry_ms_2"], 120000)
        self.assertEqual(o["c4_valid_after_success"], 1)
        self.assertEqual(o["c4_retry_ms_after_success"], 0)
        self.assertEqual(o["c4_timer_restarted"], 1)

    def test_manual_fetch_dumps_once(self):
        self.assertEqual(self.out["manual_dumps"], 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
