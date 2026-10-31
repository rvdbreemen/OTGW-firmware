#!/usr/bin/env python3
"""TASK-1496: the SAT REST contract follows the TASK-1485 MQTT contract.

Items (decision for every one: follow MQTT, name, polarity and meaning):
  1. POST /api/v2/sat/settings/heating_curve_coeff replaces heating_curve (no alias), as the MQTT command.
  2. satSendHealthJSON(): flame_health, device_health, cycle_health are true = problem with the MQTT
     meaning: flame = STUCK_ON/STUCK_OFF/PWM_SHORT/SHORT_CYCLING and null while INSUFFICIENT_DATA,
     device = boiler status not evaluated yet (!bBoilerStatusKnown), cycle = OVERSHOOT/UNDERHEAT/
     UNDERHEAT_PWM/SHORT.
  3. openapi.yaml and the changelog name the breaking change; the spec/comparison rows are updated.

Nothing below re-implements the code under test: the settings dispatch chain and satSendHealthJSON() are
cut out of restAPI.ino and compiled with a host C++ compiler, with stubs that record what they are called
with. Without a compiler those tests are skipped, they do not pass silently.

Old-vs-fix: OTGW_SRC_REV=<git rev> reads restAPI.ino from that revision.
  OTGW_SRC_REV=HEAD python tests/test_sat_rest_contract_1496.py   -> red on the base
  python tests/test_sat_rest_contract_1496.py                     -> green

Run: python tests/test_sat_rest_contract_1496.py
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
    path = f"{base}/{rel}" if base else rel
    if REV:
        out = subprocess.run(["git", "show", f"{REV}:{path}"], cwd=REPO, capture_output=True, check=True)
        return out.stdout.decode("utf-8")
    return (REPO / path).read_text(encoding="utf-8")


def body(src, signature):
    """Definition text from `signature` to the first closing brace at column 0 (a comment in the function holds a brace)."""
    i = src.index(signature)
    return src[i:src.index(chr(10) + "}" + chr(10), i) + 3]


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
#define snprintf_P snprintf
#define strcasecmp_P strcasecmp
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
"""

HEALTH_STUBS = r"""
enum SATBoilerStatus { SAT_BS_OFF = 0, SAT_BS_IDLE, SAT_BS_STALLED_IGNITION };
enum SATFlameStatus { SAT_FS_INSUFFICIENT_DATA = 0, SAT_FS_HEALTHY, SAT_FS_IDLE_OK, SAT_FS_STUCK_ON,
                      SAT_FS_STUCK_OFF, SAT_FS_PWM_SHORT, SAT_FS_SHORT_CYCLING };
enum SATCycleClass { SAT_CYCLE_NONE = 0, SAT_CYCLE_GOOD, SAT_CYCLE_OVERSHOOT, SAT_CYCLE_UNDERHEAT, SAT_CYCLE_SHORT,
                     SAT_CYCLE_UNCERTAIN, SAT_CYCLE_UNDERHEAT_PWM, SAT_CYCLE_INSUFFICIENT };
struct SatState {
  bool bSetpointMismatch = false, bModulationReliable = true, bActive = false, bSafetyTripped = false;
  bool bPressureAlarm = false, bBoilerStatusKnown = false;
  SATBoilerStatus eBoilerStatus = SAT_BS_OFF;
  SATFlameStatus eFlameStatus = SAT_FS_INSUFFICIENT_DATA;
  SATCycleClass eCycle = SAT_CYCLE_NONE;
  int eLastCycleKind = 0, iCycleCount = 0, iErrorSampleCount = 0;
  float fSmoothedPressure = 0, fPressureDropRate = 0, fLastCycleDuration = 0, fLastCycleFractionCH = 0,
        fLastCycleFractionDHW = 0, fMeanError = 0, fErrorStdDev = 0;
  char sHeatCurveRec[13] = "insufficient";
};
struct State { SatState sat; }; static State state;
struct OTState { uint8_t SlaveStatus = 0; }; static OTState OTcurrentSystemState;
static uint32_t millis() { return 1000; }
static SATCycleClass satReportedLastCycleClass(const SatState& s, uint32_t) { return s.eCycle; }
static std::string g_json;
struct AsyncResponseStream {};
static AsyncResponseStream g_strm;
static AsyncResponseStream* restBeginStream(const char*) { return &g_strm; }
static void restFinalize() {}
struct JsonEmit {
  JsonEmit(AsyncResponseStream&) {}
  void beginObject() {} void endObject() {}
  template<typename K> void field(K k, bool b)        { g_json += std::string(k) + "=" + (b ? "true" : "false") + "\n"; }
  template<typename K> void field(K k, int32_t v)     { g_json += std::string(k) + "=" + std::to_string(v) + "\n"; }
  template<typename K> void field(K k, const char* s) { g_json += std::string(k) + "=" + s + "\n"; }
  template<typename K> void field(K k, float f)       { g_json += std::string(k) + "=" + std::to_string(f) + "\n"; }
  template<typename K> void fieldRaw(K k, const char* j) { g_json += std::string(k) + "=" + j + "\n"; }
  template<typename K> void fieldRawP(K k, const char* j) { g_json += std::string(k) + "=" + j + "\n"; }
};
"""

DISPATCH_STUBS = r"""
static std::vector<std::string> g_calls;
static bool g_apiError = false;
static void updateSetting(const char* name, const char*) { g_calls.push_back(name); }
static void sendApiError(int, const char*) { g_apiError = true; }
static void webSend(int, const char*, const char*) {}
static void satResetControlState() {}
static void satHandleControlMode(const char*) {}
static void satHandlePreset(const char*) {}
static void satHandleHeatingMode(const char*) {}
static bool satMaxTSetInputOk(float) { return true; }
static bool satBoilerHardwarePresent() { return false; }
#define EVALBOOLEAN(x) (x[0] == '1')
"""


def build(tmp, name, code):
    cpp = Path(tmp) / f"{name}.cpp"
    cpp.write_text(code, encoding="utf-8")
    return compile_cpp(cpp, Path(tmp) / f"{name}.exe")


def run(exe, *args):
    r = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True)
    if r.returncode:
        raise AssertionError(f"harness exit {r.returncode}: {r.stdout}{r.stderr}")
    return r.stdout


class RestHealthJson(unittest.TestCase):
    """satSendHealthJSON(): the three health booleans follow the MQTT contract."""

    @classmethod
    def setUpClass(cls):
        fn = body(read("restAPI.ino"), "static void satSendHealthJSON()")
        code = COMMON + HEALTH_STUBS + fn + r"""
int main(int argc, char** argv) {   // flameStatus bootKnown cycleClass boilerStatus safetyTripped
  state.sat.eFlameStatus = (SATFlameStatus)atoi(argv[1]);
  state.sat.bBoilerStatusKnown = atoi(argv[2]) != 0;
  state.sat.eCycle = (SATCycleClass)atoi(argv[3]);
  state.sat.eBoilerStatus = (SATBoilerStatus)atoi(argv[4]);
  state.sat.bSafetyTripped = atoi(argv[5]) != 0;
  satSendHealthJSON();
  printf("%s", g_json.c_str());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "health", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def js(self, flame=0, known=0, cycle=0, bs=0, trip=0):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = run(self.exe, flame, known, cycle, bs, trip)
        return dict(line.split("=", 1) for line in out.splitlines())

    # flame_health: ON = problem, not published (null) while INSUFFICIENT_DATA
    def test_flame_stuck_on_is_problem(self):
        self.assertEqual(self.js(flame=3)["flame_health"], "true")

    def test_flame_short_cycling_is_problem(self):
        self.assertEqual(self.js(flame=6)["flame_health"], "true")

    def test_flame_healthy_is_no_problem(self):
        self.assertEqual(self.js(flame=1)["flame_health"], "false")

    def test_flame_insufficient_data_is_null(self):
        self.assertEqual(self.js(flame=0)["flame_health"], "null")

    def test_flame_safety_trip_does_not_decide(self):
        self.assertEqual(self.js(flame=1, trip=1)["flame_health"], "false")

    # device_health: ON while the boiler status has not been evaluated yet
    def test_device_not_evaluated_is_problem(self):
        self.assertEqual(self.js(known=0, bs=1)["device_health"], "true")

    def test_device_evaluated_is_no_problem(self):
        self.assertEqual(self.js(known=1, bs=1)["device_health"], "false")

    def test_device_status_off_alone_is_no_problem(self):
        self.assertEqual(self.js(known=1, bs=0)["device_health"], "false")

    # cycle_health: ON for OVERSHOOT, UNDERHEAT, UNDERHEAT_PWM, SHORT
    def test_cycle_problem_classes(self):
        for cls_id in (2, 3, 4, 6):
            self.assertEqual(self.js(cycle=cls_id)["cycle_health"], "true", cls_id)

    def test_cycle_other_classes_no_problem(self):
        for cls_id in (0, 1, 5, 7):
            self.assertEqual(self.js(cycle=cls_id)["cycle_health"], "false", cls_id)


class RestSettingsDispatch(unittest.TestCase):
    """POST /api/v2/sat/settings/<name>: heating_curve_coeff replaces heating_curve."""

    @classmethod
    def setUpClass(cls):
        src = read("restAPI.ino")
        i = src.index('strcasecmp_P(sub, PSTR("settings"))')
        a = src.index("bool handled = false;", i)
        b = src.index("if (!handled) {", a)
        chain = src[a:b]
        chain = re.sub(r"\breturn;", "return false;", chain)
        code = COMMON + DISPATCH_STUBS + r"""
static bool dispatch(const char* settingName, const char* val) {
""" + chain + r"""
  return handled;
}
int main(int argc, char** argv) {
  bool h = dispatch(argv[1], argv[2]);
  printf("%d %d", h ? 1 : 0, (int)g_calls.size());
  for (auto& c : g_calls) printf(" %s", c.c_str());
  printf("\n");
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "dispatch", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def disp(self, name, val="1.5"):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return run(self.exe, name, val).split()

    def test_heating_curve_coeff_writes_the_coefficient(self):
        self.assertEqual(self.disp("heating_curve_coeff"), ["1", "1", "SATcoefficient"])

    def test_heating_curve_is_no_longer_a_setting_name(self):
        self.assertEqual(self.disp("heating_curve"), ["0", "0"])

    def test_other_settings_still_dispatch(self):
        self.assertEqual(self.disp("overshoot_margin", "3.0")[0], "1")


class RestDocs(unittest.TestCase):
    """AC1/AC3: the breaking change is named in openapi.yaml and the changelog."""

    def test_openapi_names_the_new_setting_and_polarity(self):
        y = read("docs/api/openapi.yaml", "")
        self.assertIn("heating_curve_coeff", y)
        self.assertNotIn('example: "heating_curve"\n', y)
        self.assertIn("BREAKING (TASK-1496)", y)

    def test_changelog_lists_it(self):
        c = read("CHANGELOG.md", "")
        self.assertIn("TASK-1496", c)
        self.assertIn("settings/heating_curve_coeff", c)

    def test_spec_rows_describe_the_new_contract(self):
        spec = read("docs/sat/firmware-sat-spec.md", "")
        cmp_ = read("docs/sat/sat-comparison.md", "")
        self.assertNotIn('"ON" while boiler status is OFF', spec)
        self.assertNotIn("sat/pressure_health` ON = healthy (SATcontrol", spec)
        self.assertNotIn("insufficient / INCREASE / DECREASE / HOLD", spec)
        self.assertIn("`heating_curve_coeff` (MQTT since TASK-1485", spec)
        self.assertNotIn('`sat/pressure_health` "ON" = healthy (SATcontrol.ino:2886', cmp_)
        self.assertNotIn("MQTT `sat/device_health` ON when the status is `OFF`", cmp_)


if __name__ == "__main__":
    unittest.main()
