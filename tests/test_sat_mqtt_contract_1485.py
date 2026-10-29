#!/usr/bin/env python3
"""TASK-1485: the sat/* MQTT contract cleanup (Robert, TASK-1466 decisions 2-5).

(2) sat/heating_curve_recommendation is all lower case: insufficient / increase / decrease / hold.
(3) sat/ssi is gone (sat/summer_simmer_index stays) and its retained topic is emptied.
(4) sat/pressure_health is ON on a pressure problem, as thermo-nova binary_sensor.py:239 (device class
    PROBLEM) and :253-295 (is_on = confirmed problem). sat/device_health is ON while the boiler status has
    not been evaluated yet, the firmware's counterpart of thermo-nova BoilerStatus.INSUFFICIENT_DATA
    (binary_sensor.py:404-406, device/__init__.py:44-49: only before the first device tracker update).
(5) the MQTT command sat/heating_curve is renamed sat/heating_curve_coeff, no alias.

Nothing below re-implements the code under test. The behaviour tests cut the real code out of the
sources and compile it with a host C++ compiler (g++, clang++ or MSVC cl via vswhere), with stubs that
record what would be published:
  - satHeatingCurveRecommendation()           (SATcycles.ino)
  - satUpdateBoilerStatus() + its _bs_ statics  (SATcontrol.ino)
  - the publisher segments "Pressure monitoring" and "Device + Cycle health" .. "Modulation state"
    of satPublishMQTT()                          (SATcontrol.ino)
  - kRetiredSatEntities + clearRetiredSatEntities()  (MQTTHaDiscovery.cpp)
  - kSatMqttCmds + dispatchSatMqttCmd()          (MQTTstuff.ino)
Without a compiler those tests are skipped, they do not pass silently.

Not reachable from here: the real publishIfChanged* helpers and the MQTT client (stubbed); a broker
capture on the bench covers the wire format.

Old-vs-fix: OTGW_SRC_REV=<git rev> reads every source from that revision.
  OTGW_SRC_REV=origin/dev python tests/test_sat_mqtt_contract_1485.py   -> red before TASK-1485
  python tests/test_sat_mqtt_contract_1485.py                          -> green

Run: python tests/test_sat_mqtt_contract_1485.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_SRC_REV")
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402


def read(rel, base="src/OTGW-firmware"):
    if REV:
        out = subprocess.run(["git", "show", f"{REV}:{base}/{rel}"], cwd=REPO, capture_output=True, check=True)
        return out.stdout.decode("utf-8")
    return (REPO / base / rel).read_text(encoding="utf-8")


def body(src, signature):
    """Definition text of the function starting with `signature`, brace-matched (prototypes skipped)."""
    i = -1
    while True:
        i = src.index(signature, i + 1)
        rest = src[i + len(signature):i + len(signature) + 40].lstrip()
        if rest.startswith("{"):
            break
    j = src.index("{", i)
    depth, k = 0, j
    while True:
        c = src[k]
        depth += c == "{"
        depth -= c == "}"
        k += 1
        if depth == 0:
            return src[i:k]


def segment(src, start, end):
    i = src.index(start)
    return src[i:src.index(end, i)]


COMMON = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#define F(x) x
#define PSTR(x) x
#define PROGMEM
#define PGM_P const char*
#define memcpy_P memcpy
#define snprintf_P snprintf
#define strcasecmp_P strcasecmp
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static std::vector<std::string> g_out;
static void feedWatchDog() {}
static void sendWebSocketJSON(const char*) {}
"""


def build(tmp, name, code):
    cpp = Path(tmp) / f"{name}.cpp"
    cpp.write_text(code, encoding="utf-8")
    exe = compile_cpp(cpp, Path(tmp) / f"{name}.exe")
    return exe


def run(exe, *args):
    r = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True)
    if r.returncode:
        raise AssertionError(f"harness exit {r.returncode}: {r.stdout}{r.stderr}")
    return r.stdout.split()


class T2HeatingCurveRecommendationLowercase(unittest.TestCase):
    """(2) satHeatingCurveRecommendation() returns lower-case values only."""

    @classmethod
    def setUpClass(cls):
        fn = body(read("SATcycles.ino"), "const char* satHeatingCurveRecommendation()")
        code = COMMON + r"""
struct SatState { bool bActive = true; char sHeatCurveRec[13] = "insufficient"; };
struct State { SatState sat; }; static State state;
static const uint8_t HCR_DAYS = 30;
static const float   HCR_THRESHOLD_C  = 0.5f;
static const uint8_t HCR_SUSTAIN_DAYS = 3;
static float   _hcr_dailyMedian[HCR_DAYS];
static uint8_t _hcr_head = 0, _hcr_count = 0;
""" + fn + r"""
int main(int argc, char** argv) {   // active m1 m2 m3 (newest last); "x" = no median for that day
  state.sat.bActive = atoi(argv[1]) != 0;
  for (int i = 2; i < argc; i++) {
    if (argv[i][0] == 'x') continue;
    _hcr_dailyMedian[_hcr_head] = (float)atof(argv[i]); _hcr_head = (_hcr_head + 1) % HCR_DAYS; _hcr_count++;
  }
  printf("%s\n", satHeatingCurveRecommendation());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "hcr", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def rec(self, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return run(self.exe, *args)[0]

    def test_room_too_cold_is_increase(self):
        self.assertEqual(self.rec(1, -1.0, -1.0, -1.0), "increase")

    def test_room_too_warm_is_decrease(self):
        self.assertEqual(self.rec(1, 1.0, 1.0, 1.0), "decrease")

    def test_mixed_is_hold(self):
        self.assertEqual(self.rec(1, 1.0, -1.0, 0.0), "hold")

    def test_insufficient_unchanged(self):
        self.assertEqual(self.rec(1, 1.0, "x", "x"), "insufficient")
        self.assertEqual(self.rec(0, 1.0, 1.0, 1.0), "insufficient")


PUB_STUBS = r"""
struct SATShadowF {}; struct SATShadowB {}; struct SATShadowS {};
static bool publishIfChangedF(const char* t, float v, SATShadowF&, float, int, bool) { char b[64]; snprintf(b, sizeof b, "%s=%.2f", t, v); g_out.push_back(b); return true; }
static bool publishIfChangedB(const char* t, bool v, SATShadowB&, bool) { g_out.push_back(std::string(t) + "=" + (v ? "true" : "false")); return true; }
static bool publishIfChangedS(const char* t, const char* v, SATShadowS&, bool) { g_out.push_back(std::string(t) + "=" + v); return true; }
static bool publishIfChangedBStr(const char* t, bool v, SATShadowB&, const char* on, const char* off, bool) { g_out.push_back(std::string(t) + "=" + (v ? on : off)); return true; }
static void satPressureHealthPublish() {}
enum SATBoilerStatus {
  SAT_BS_OFF = 0, SAT_BS_IDLE, SAT_BS_PREHEATING, SAT_BS_AT_SETPOINT,
  SAT_BS_MODULATING_UP, SAT_BS_MODULATING_DOWN, SAT_BS_IGNITION_SURGE,
  SAT_BS_STALLED_IGNITION, SAT_BS_ANTI_CYCLING, SAT_BS_PUMP_STARTING,
  SAT_BS_WAITING_FLAME, SAT_BS_OVERSHOOT_COOLING, SAT_BS_POST_CYCLE,
  SAT_BS_HEATING, SAT_BS_COOLING, SAT_BS_HEATING_HOT_WATER
};
enum SATCycleClass { SAT_CYCLE_NONE = 0, SAT_CYCLE_GOOD, SAT_CYCLE_OVERSHOOT, SAT_CYCLE_UNDERHEAT, SAT_CYCLE_UNDERHEAT_PWM, SAT_CYCLE_SHORT };
struct SatState {
  SATBoilerStatus eBoilerStatus = SAT_BS_OFF; bool bBoilerStatusKnown = false;
  float fSmoothedPressure = 1.5f, fPressureDropRate = 0, fHumidity = 50, fFinalSetpoint = 40, fLastCycleDuration = 0;
  bool bPressureAlarm = false, bPressureHealthy = true, bDhwActive = false, bModulationReliable = false;
};
struct State { SatState sat; }; static State state;
struct SatSettings { float fOvershootMargin = 3.0f; }; struct Settings { SatSettings sat; }; static Settings settings;
struct OTState { float RelModLevel = 0; }; static OTState OTcurrentSystemState;
static const float SAT_EPS_PRESSURE = 0.01f, SAT_EPS_FRACTION = 0.001f, SAT_EPS_ERROR = 0.01f, SAT_EPS_TEMP_COARSE = 0.1f;
static uint32_t g_now = 1000; static uint32_t millis() { return g_now; }
static bool g_flame = false; static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return 30.0f; }
static float satGetRoomTemp() { return 26.0f; }
static bool satHumidityFresh(uint32_t) { return true; }
static float satCalcSimmerIndex(float t, float) { return t; }
static const char* satSimmerPerception(float) { return "comfortable"; }
static SATCycleClass satReportedLastCycleClass(const SatState&, uint32_t) { return SAT_CYCLE_NONE; }
static SATShadowF s_pressure, s_pressure_drop_rate, s_ssi, s_summer_simmer_index;
static SATShadowB s_pressure_alarm, s_pressure_health, s_device_health, s_cycle_health;
static SATShadowS s_summer_simmer_perception;
"""


class T34Publishers(unittest.TestCase):
    """(3) no sat/ssi publish; (4) pressure_health and device_health ON = problem."""

    @classmethod
    def setUpClass(cls):
        sat = read("SATcontrol.ino")
        statics = segment(sat, "// --- Boiler Status Tracking ---", "// --- Previous flame state")
        consts = segment(sat, "static const uint32_t BS_ANTI_CYCLE_MIN_OFF_MS", "static void satUpdateBoilerStatus()")
        upd = body(sat, "static void satUpdateBoilerStatus()")
        press = segment(sat, "  // Pressure monitoring + ch_pressure delegate", "  // Modulation + setpoint sync")
        health = segment(sat, "  // Device + Cycle health binary sensors", "  // Modulation state + PWM state")
        code = COMMON + PUB_STUBS + statics + consts + upd + r"""
static void publishPressure() {
""" + press + r"""
}
static void publishHealth() {
""" + health + r"""
}
int main(int argc, char** argv) {   // scenario: fresh | evaluated | pressure_ok | pressure_bad
  std::string sc = argv[1];
  if (sc == "fresh") publishHealth();
  else if (sc == "evaluated") { satUpdateBoilerStatus(); g_now += 5000; satUpdateBoilerStatus(); publishHealth(); }
  else if (sc == "pressure_ok") { state.sat.bPressureHealthy = true; publishPressure(); }
  else if (sc == "pressure_bad") { state.sat.bPressureHealthy = false; publishPressure(); }
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "pub", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def out(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return dict(line.split("=", 1) for line in run(self.exe, sc))

    def test_no_sat_ssi_publish(self):
        o = self.out("fresh")
        self.assertNotIn("sat/ssi", o)
        self.assertEqual(o["sat/summer_simmer_index"], "26.00")

    def test_pressure_health_off_when_healthy(self):
        self.assertEqual(self.out("pressure_ok")["sat/pressure_health"], "OFF")

    def test_pressure_health_on_when_problem(self):
        self.assertEqual(self.out("pressure_bad")["sat/pressure_health"], "ON")

    def test_device_health_on_before_first_evaluation(self):
        self.assertEqual(self.out("fresh")["sat/device_health"], "ON")

    def test_device_health_off_once_evaluated(self):
        self.assertEqual(self.out("evaluated")["sat/device_health"], "OFF")


class T3RetiredSsi(unittest.TestCase):
    """(3) the retained sat/ssi topic is emptied on every SAT discovery pass."""

    @classmethod
    def setUpClass(cls):
        disc = read("MQTTHaDiscovery.cpp")
        table = segment(disc, "struct RetiredSatEntity {", "void clearRetiredSatEntities(")
        fn = body(disc, "void clearRetiredSatEntities(const HaDiscoveryContext &ctx)")
        code = COMMON + r"""
#define STREAM_TOPIC_MAX 128
struct HaDiscoveryContext { const char* mqttPubTopic; const char* haPrefix; const char* nodeId; };
static bool publishEmptyRetained(const char* t) { g_out.push_back(t); return true; }
""" + table + fn + r"""
int main() {
  HaDiscoveryContext ctx{ "OTGW/value/otgw-1", "homeassistant", "otgw-1" };
  clearRetiredSatEntities(ctx);
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "ret", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_sat_ssi_emptied(self):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        self.assertIn("OTGW/value/otgw-1/sat/ssi", run(self.exe))


class T5CommandRename(unittest.TestCase):
    """(5) set/<node>/sat/heating_curve_coeff sets SATcoefficient; sat/heating_curve is unknown."""

    @classmethod
    def setUpClass(cls):
        mq = read("MQTTstuff.ino")
        types = segment(mq, "typedef void (*SatMqttHandler)(const char*);", "// Dispatch table.")
        table = segment(mq, "static const SatMqttCmdEntry kSatMqttCmds[] PROGMEM = {", "// Returns true when the sub-command")
        fn = body(mq, "static bool dispatchSatMqttCmd(const char* cmd, const char* payload)")
        handlers = sorted(set(re.findall(r"\{\s*\"[^\"]+\",\s*nullptr,\s*(\w+)\s*\}", table)))
        stubs = "".join(f"static void {h}(const char*) {{ g_out.push_back(\"handler:{h}\"); }}\n" for h in handlers)
        code = COMMON + r"""
static void updateSetting(const char* k, const char* v) { g_out.push_back(std::string("setting:") + k + "=" + v); }
""" + types + stubs + table + fn + r"""
int main(int argc, char** argv) {
  bool hit = dispatchSatMqttCmd(argv[1], argv[2]);
  printf("%s\n", hit ? "hit" : "miss");
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "cmd", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def dispatch(self, cmd, val):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return run(self.exe, cmd, val)

    def test_new_command_sets_coefficient(self):
        self.assertEqual(self.dispatch("heating_curve_coeff", "1.8"), ["hit", "setting:SATcoefficient=1.8"])

    def test_old_command_is_gone(self):
        self.assertEqual(self.dispatch("heating_curve", "1.8"), ["miss"])


class TSource(unittest.TestCase):
    """Source-level: the removed names survive only on RETIRED lines, docs list the breaking changes."""

    def test_sat_ssi_only_on_retired_lines(self):
        for rel in ("SATcontrol.ino", "MQTTHaDiscovery.cpp", "MQTTstuff.ino", "restAPI.ino"):
            for line in read(rel).splitlines():
                if re.search(r"sat/ssi\b", line):
                    self.assertIn("RETIRED-SAT-ENTITY", line, f"{rel}: {line.strip()}")

    def test_device_health_is_not_boiler_off(self):
        # Behaviour on HEAD and after the fix is the same (the evaluator never yields SAT_BS_OFF, so OFF
        # only ever meant "not evaluated yet"); the fix makes that explicit so a later evaluator change
        # that yields OFF for "no CH demand" (thermo-nova device/status.py:39-40) cannot flip the topic.
        sat = read("SATcontrol.ino")
        seg = segment(sat, 'publishIfChangedBStr(F("sat/device_health")', ";")
        self.assertNotIn("SAT_BS_OFF", seg)
        self.assertIn("bBoilerStatusKnown", seg)

    def test_mqtt_md_and_changelog(self):
        md = read("docs/api/MQTT.md", base=".")
        self.assertNotRegex(md, r"\| `sat/ssi` \|")
        self.assertIn("`sat/heating_curve_coeff` | `SATcoefficient`", md)
        self.assertNotRegex(md, r"\"INCREASE\"|\"DECREASE\"|\"HOLD\"")
        cl = read("CHANGELOG.md", base=".")
        unreleased = cl[cl.index("## [Unreleased]"):]
        unreleased = unreleased[:unreleased.index("\n## [", 10)]
        for needle in ("TASK-1485", "sat/ssi", "sat/pressure_health", "sat/device_health",
                       "sat/heating_curve_recommendation", "heating_curve_coeff"):
            self.assertIn(needle, unreleased)


if __name__ == "__main__":
    unittest.main(verbosity=2)
