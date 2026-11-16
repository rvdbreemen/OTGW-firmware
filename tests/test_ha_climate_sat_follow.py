#!/usr/bin/env python3
"""TASK-1979 / ADR-248: the Home Assistant thermostat card follows SAT while SAT is enabled.

(a) streamClimateDiscovery() idx 0: the card's target command goes to set/<node>/sat/climate_target with the
    bare value ({{ value }}), its state topics are <pub>/sat/climate_current and <pub>/sat/climate_target,
    min_temp is 5, and the six SAT presets ride with their command and state topics (never "none"). The
    DHW card (idx 1) is unchanged.
(b) kSatMqttCmds has a climate_target row. Its handler sends the value to SAT's target path while SAT owns
    the control setpoint (ADR-179) and queues TT=<value> otherwise; a non-numeric payload does nothing.
(c) satPublishClimateBridge(): sat/climate_target and sat/climate_current carry SAT's target and room
    temperature while SAT is enabled and TrSet / Tr while it is off, publish again after the force flag,
    and run inside one status-burst window (ADR-088).

Nothing here re-implements the code under test. The tests cut the real code out of the sources and compile
it with a host C++ compiler (g++, clang++ or MSVC cl via vswhere); stubs record what would be published.
Without a compiler the behaviour tests are skipped, they do not pass silently.
  - streamClimateDiscovery() + the real JSON helpers + MqttJsonWriter + measureMallocPublish()
  - kSatMqttCmds + dispatchSatMqttCmd() + _satClimateTargetCmd()
  - satClimateBridgeForce() + satPublishClimateBridge()

Not reachable from here: the real publishIfChangedF() helper (stubbed with the same first-seen / change rule),
the MQTT client, and Home Assistant itself. The bench run with an HA screenshot covers those.

Old-vs-fix: OTGW_SRC_REV=<git rev> reads every source from that revision.
  OTGW_SRC_REV=origin/dev python tests/test_ha_climate_sat_follow.py   -> red before TASK-1979
  python tests/test_ha_climate_sat_follow.py                           -> green

Run: python tests/test_ha_climate_sat_follow.py
"""
import json
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
    """Definition text of the function starting with `signature`, brace-matched (prototypes skipped).
    None when the source has no such definition."""
    i = -1
    while True:
        try:
            i = src.index(signature, i + 1)
        except ValueError:
            return None
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
    """Text from `start` up to (not including) `end`; None when either is missing."""
    try:
        i = src.index(start)
        return src[i:src.index(end, i)]
    except ValueError:
        return None


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
#define strlen_P strlen
#define snprintf_P snprintf
#define strcasecmp_P strcasecmp
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#define DebugTf(...) ((void)0)
static std::vector<std::string> g_out;
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


class T1Discovery(unittest.TestCase):
    """(a) the real streamClimateDiscovery() compose, both climate entities."""

    @classmethod
    def setUpClass(cls):
        disc = read("MQTTHaDiscovery.cpp")
        hdr = read("MQTTstuff.h")
        writer = segment(hdr, "struct MqttJsonWriter {", "// Streaming discovery functions")
        ctx = segment(hdr, "struct HaDiscoveryContext {", "// ----")
        helpers = "".join(
            body(disc, sig) + "\n" for sig in (
                "static bool writeJsonKV(MqttJsonWriter &w, PGM_P key, const char *value)",
                "static bool writeJsonKV_P(MqttJsonWriter &w, PGM_P key, PGM_P value)",
            ))
        # one-liners: brace matching would trip over the '}' character literal, so take the whole line
        for name in ("writeJsonOpen", "writeJsonClose", "writeJsonComma"):
            helpers += re.search(r"^static bool " + name + r"\(MqttJsonWriter &w\).*$", disc, re.M).group(0) + "\n"
        measure = "template <typename ComposeFn>\n" + body(disc, "static bool measureMallocPublish(const char *topic, ComposeFn &&compose)")
        fn = body(disc, "bool streamClimateDiscovery(uint8_t climateIdx,\n                            HaDiscoveryContext &ctx)")
        cls.fn = fn
        code = COMMON + r"""
enum class HaDevice : uint8_t { Esp = 0 };
#define STREAM_TOPIC_MAX 128
static const uint32_t STREAM_HEAP_MIN = 4000;
""" + writer + "\n" + ctx + r"""
static const char kAvtyT[] PROGMEM = "avty_t";
static const char kIcon[] PROGMEM = "icon";
static const char kJsonAttrTopic[] PROGMEM = "json_attributes_topic";
static bool mqttIsConnected() { return true; }
static bool canPublishMQTT() { return true; }
static uint32_t platformFreeHeap() { return 100000; }
static void feedWatchDog() {}
static void incPublishedTopicCount() {}
static PGM_P haSourcePrefix(HaDevice, const HaDiscoveryContext&) { return ""; }
static bool writeDeviceBlock(MqttJsonWriter &w, const HaDiscoveryContext&) { return w.writeProgmem("\"dev\":{}"); }
static bool writeOriginBlock(MqttJsonWriter &w, const HaDiscoveryContext&) { return w.writeProgmem("\"o\":{}"); }
static bool mqttPublishRaw(const char* topic, const uint8_t* p, size_t len, bool retain) {
  printf("%s\n%.*s\n", topic, (int)len, (const char*)p); (void)retain; return true;
}
""" + helpers + measure + "\n" + fn + r"""
int main(int argc, char** argv) {
  HaDiscoveryContext ctx{};
  ctx.nodeId = "otgw-1"; ctx.mqttPubTopic = "OTGW/value/otgw-1"; ctx.mqttSubTopic = "OTGW/set/otgw-1";
  ctx.haPrefix = "homeassistant";
  return streamClimateDiscovery((uint8_t)atoi(argv[1]), ctx) ? 0 : 3;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "disc", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def payload(self, idx):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        topic, js = run(self.exe, idx).strip().split("\n", 1)
        return topic, json.loads(js)

    def test_thermostat_keeps_its_unique_id(self):
        topic, p = self.payload(0)
        self.assertEqual(topic, "homeassistant/climate/otgw-1/climate/config")
        self.assertEqual(p["uniq_id"], "otgw-1-thermostat")

    def test_target_command_goes_to_sat_climate_target_bare(self):
        _, p = self.payload(0)
        self.assertEqual(p["temp_cmd_t"], "OTGW/set/otgw-1/sat/climate_target")
        self.assertEqual(p["temp_cmd_tpl"], "{{ value }}")

    def test_state_topics_are_the_new_scalars(self):
        _, p = self.payload(0)
        self.assertEqual(p["curr_temp_t"], "OTGW/value/otgw-1/sat/climate_current")
        self.assertEqual(p["temp_stat_t"], "OTGW/value/otgw-1/sat/climate_target")

    def test_min_temp_is_5(self):
        _, p = self.payload(0)
        self.assertEqual(p["min_temp"], "5")
        self.assertEqual(p["max_temp"], "30")

    def test_presets_without_none_and_with_their_topics(self):
        _, p = self.payload(0)
        self.assertEqual(p["pr_modes"], ["comfort", "eco", "away", "sleep", "activity", "home"])
        self.assertNotIn("none", p["pr_modes"])
        self.assertEqual(p["pr_mode_cmd_t"], "OTGW/set/otgw-1/sat/preset")
        self.assertEqual(p["pr_mode_stat_t"], "OTGW/value/otgw-1/sat/preset")

    def test_dhw_card_is_unchanged(self):
        topic, p = self.payload(1)
        self.assertEqual(topic, "homeassistant/climate/otgw-1/dhw_control/config")
        self.assertEqual(p["temp_cmd_t"], "OTGW/set/otgw-1/command")
        self.assertEqual(p["temp_cmd_tpl"], "SW={{ value }}")
        self.assertEqual(p["curr_temp_t"], "OTGW/value/otgw-1/Tdhw")
        self.assertEqual(p["temp_stat_t"], "OTGW/value/otgw-1/TdhwSet")
        self.assertEqual(p["min_temp"], "40")
        for k in ("pr_modes", "pr_mode_cmd_t", "pr_mode_stat_t"):
            self.assertNotIn(k, p)

    def test_old_thermostat_command_is_gone(self):
        # Sending TT= from the card while SAT is enabled is what ADR-248 removes.
        self.assertNotIn("TT={{ value }}", self.fn)


class T2ClimateTargetCommand(unittest.TestCase):
    """(b) the climate_target row, dispatched by the real dispatchSatMqttCmd()."""

    @classmethod
    def setUpClass(cls):
        mq = read("MQTTstuff.ino")
        types = segment(mq, "typedef void (*SatMqttHandler)(const char*);", "// Dispatch table.")
        table = segment(mq, "static const SatMqttCmdEntry kSatMqttCmds[] PROGMEM = {", "// Returns true when the sub-command")
        disp = body(mq, "static bool dispatchSatMqttCmd(const char* cmd, const char* payload)")
        handler = body(mq, "static void _satClimateTargetCmd(const char* v)")
        cls.table, cls.handler = table, handler
        if None in (types, table, disp) or handler is None:
            cls.exe = None
            cls.tmp = None
            return
        names = sorted(set(re.findall(r"\{\s*\"[^\"]+\",\s*nullptr,\s*(\w+)\s*\}", table)) - {"_satClimateTargetCmd"})
        stubs = "".join(f"static void {h}(const char*) {{ g_out.push_back(\"handler:{h}\"); }}\n" for h in names)
        code = COMMON + r"""
enum class CmdQueueResult : uint8_t { Queued, Replaced, Sent, Dropped };
static bool g_satOwns = false;
static bool satOwnsControlSetpoint() { return g_satOwns; }
static bool satHandleTargetTemp(const char* v) { g_out.push_back(std::string("sat_target:") + v); return true; }
static CmdQueueResult addCommandToQueue(const char* c, int n, const bool = false, const int16_t = 1000, const bool = false) {
  g_out.push_back(std::string("queue:") + c + ":" + std::to_string(n)); return CmdQueueResult::Queued;
}
static void updateSetting(const char* k, const char* v) { g_out.push_back(std::string("setting:") + k + "=" + v); }
""" + types + stubs + handler + "\n" + table + disp + r"""
int main(int argc, char** argv) {   // satOwns cmd payload
  g_satOwns = atoi(argv[1]) != 0;
  bool hit = dispatchSatMqttCmd(argv[2], argv[3]);
  printf("%s\n", hit ? "hit" : "miss");
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "cmd", code)

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def dispatch(self, owns, payload):
        self.assertIsNotNone(self.handler, "_satClimateTargetCmd() does not exist")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return run(self.exe, owns, "climate_target", payload).split()

    def test_row_exists(self):
        self.assertRegex(self.table or "", r'\{\s*"climate_target",\s*nullptr,\s*_satClimateTargetCmd\s*\}')

    def test_sat_enabled_goes_to_the_sat_target_path(self):
        self.assertEqual(self.dispatch(1, "21.5"), ["hit", "sat_target:21.5"])

    def test_sat_disabled_queues_tt(self):
        self.assertEqual(self.dispatch(0, "21.5"), ["hit", "queue:TT=21.5:7"])

    def test_non_numeric_does_nothing(self):
        self.assertEqual(self.dispatch(0, "abc"), ["hit"])
        self.assertEqual(self.dispatch(0, "21.5;CS=99"), ["hit"])
        self.assertEqual(self.dispatch(0, ""), ["hit"])

    def test_sat_enabled_never_queues_tt(self):
        for v in ("18", "21.5", "5"):
            self.assertNotIn("queue", " ".join(self.dispatch(1, v)))


class T3Publisher(unittest.TestCase):
    """(c) the real satPublishClimateBridge() and the real always-on SAT core publisher / satRepublishCoreState(),
    with a stubbed publishIfChanged*() and a stubbed millis(). The tracked-time helpers and hvacHeartbeatDue()
    are the real ones from OTGW-Core.ino (the hvac_mode / hvac_action heartbeat)."""

    @classmethod
    def setUpClass(cls):
        sat = read("SATcontrol.ino")
        seg = segment(sat, "static SATShadowF s_climate_target, s_climate_current;", "void satPublishMQTT()")
        cls.seg = seg
        core_seg = segment(sat, "static SATShadowB s_core_enabled,", "static void satPublishCoreState()")
        core_fn = body(sat, "static void satPublishCoreState()")
        repub_fn = body(sat, "void satRepublishCoreState()")
        if None in (seg, core_seg, core_fn, repub_fn):
            cls.exe = None
            cls.tmp = None
            return
        oc = read("OTGW-Core.ino")
        tracked = "".join(re.search(r"^static constexpr \w+ " + n + r"\b.*$", oc, re.M).group(0) + "\n"
                          for n in ("TRACKED_TIME_UNSEEN", "TRACKED_TIME_MODULUS", "HVAC_HEARTBEAT_INTERVAL_SEC"))
        tracked += "".join(body(oc, sig) + "\n" for sig in (
            "static uint16_t currentTrackedSeconds()",
            "static uint16_t elapsedTrackedSeconds(uint16_t now, uint16_t lastTime)",
            "static bool hasTrackedTime(uint16_t trackedTime)",
            "static bool hvacHeartbeatDue(uint16_t lastSent)"))
        code = COMMON + r"""
static uint32_t g_ms = 0; static uint32_t millis() { return g_ms; }
struct SATShadowF { float last; bool valid; };
struct SATShadowB { int8_t last; bool valid; };
struct SATShadowS { char last[24]; bool valid; };
static const float SAT_EPS_TEMP = 0.05f;
static uint8_t g_satShadowEpoch = 0;   // bumped by the real satRepublishCoreState(); the stub helpers do not use it
enum SATPreset { SAT_PRESET_NONE = 0, SAT_PRESET_COMFORT = 1 };
static const char* satGetPresetName(SATPreset p) { return p == SAT_PRESET_COMFORT ? "comfort" : "none"; }
struct MqttS { bool bEnable = true; };
struct SatS { float fTargetTemp = 20.0f; bool bEnabled = false; bool bUseExternalTemp = false; };
struct Settings { MqttS mqtt; SatS sat; };
struct MqttT { bool bConnected = true; };
struct SatT { bool bActive = false, bSafetyTripped = false, bFallbackActive = false, bPushRefused = false, bPm25NoSlot = false,
  bRoomTempLost = false, bOtLinkLost = false, bOutsideTempMissing = false,
  bNoIgnition = false, bHeatingFailure = false;   // TASK-1882 diagnostic events, published by the same pass
  int iShVerdict = 0, eControlMode = 0;
  uint8_t iRequestedControlMode = 0; SATPreset eActivePreset = SAT_PRESET_NONE; };
struct StateT { MqttT mqtt; SatT sat; };
static Settings settings; static StateT state;
struct OTState { float Tr = NAN; float TrSet = NAN; }; static OTState OTcurrentSystemState;
static bool g_satOwns = false; static float g_room = NAN;
static bool satOwnsControlSetpoint() { return g_satOwns; }
static bool satIsEnabled() { return g_satOwns; }
static float satGetRoomTemp() { return g_room; }
static int g_burstOpen = 0, g_burstWindows = 0, g_burstSends = 0, g_outsideBurst = 0;
static void beginStatusBurst() { g_burstOpen++; g_burstWindows++; }
static void endStatusBurst() { g_burstOpen--; }
static void incrementStatusBurstPublishCount() { g_burstSends++; }
void satClimateBridgeForce();
// Same rule as the real helpers for first-seen / changed (SATmqttPublish.cpp); heartbeat and outbox are not modelled.
static bool publishIfChangedF(const char* t, float v, SATShadowF& s, float tol, int dec, bool retained) {
  if (!g_burstOpen) g_outsideBurst++;
  if (s.valid && fabsf(v - s.last) < tol) return false;
  s.last = v; s.valid = true;
  char b[32]; snprintf(b, sizeof b, "%s=%.*f%s", t, dec, v, retained ? "" : " (not retained)"); g_out.push_back(b); return true;
}
static bool publishIfChangedB(const char* t, bool v, SATShadowB& s, bool) {
  if (s.valid && s.last == (int8_t)v) return false;
  s.last = (int8_t)v; s.valid = true; g_out.push_back(std::string(t) + "=" + (v ? "true" : "false")); return true;
}
static bool publishIfChangedS(const char* t, const char* v, SATShadowS& s, bool) {
  if (s.valid && strcmp(s.last, v) == 0) return false;
  snprintf(s.last, sizeof s.last, "%s", v); s.valid = true; g_out.push_back(std::string(t) + "=" + v); return true;
}
""" + tracked + core_seg + core_fn + "\n" + repub_fn + "\n" + seg + r"""
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i]; size_t eq = a.find('=');
    std::string k = a.substr(0, eq), v = eq == std::string::npos ? "" : a.substr(eq + 1);
    if (k == "sat") g_satOwns = atoi(v.c_str()) != 0;
    else if (k == "target") settings.sat.fTargetTemp = (float)atof(v.c_str());
    else if (k == "room") g_room = (float)atof(v.c_str());
    else if (k == "tr") OTcurrentSystemState.Tr = (float)atof(v.c_str());
    else if (k == "trset") OTcurrentSystemState.TrSet = (float)atof(v.c_str());
    else if (k == "mqtt") state.mqtt.bConnected = atoi(v.c_str()) != 0;
    else if (k == "preset") state.sat.eActivePreset = (SATPreset)atoi(v.c_str());
    else if (k == "t") g_ms = (uint32_t)atol(v.c_str());
    else if (k == "force") satClimateBridgeForce();
    else if (k == "republish") satRepublishCoreState();
    else if (k == "run") { g_out.clear(); satPublishClimateBridge(); printf("run:"); for (auto& s : g_out) printf(" %s", s.c_str()); printf("\n"); }
    else if (k == "core") { g_out.clear(); satPublishCoreState(); satPublishClimateBridge(); printf("core:"); for (auto& s : g_out) printf(" %s", s.c_str()); printf("\n"); }
  }
  printf("windows=%d sends=%d outside=%d open=%d\n", g_burstWindows, g_burstSends, g_outsideBurst, g_burstOpen);
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "pub", code)

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def runs(self, *steps):
        self.assertIsNotNone(self.seg, "satPublishClimateBridge() does not exist")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        lines = run(self.exe, *steps).strip().split("\n")
        return [ln.split() for ln in lines]

    def test_sat_enabled_publishes_sat_target_and_room(self):
        out = self.runs("sat=1", "target=21.5", "room=20.25", "tr=19", "trset=18", "run")
        self.assertEqual(out[0], ["run:", "sat/climate_target=21.50", "sat/climate_current=20.25"])

    def test_sat_disabled_publishes_trset_and_tr(self):
        out = self.runs("sat=0", "target=21.5", "room=20.25", "tr=19.5", "trset=18", "run")
        self.assertEqual(out[0], ["run:", "sat/climate_target=18.00", "sat/climate_current=19.50"])

    def test_unchanged_values_publish_once(self):
        out = self.runs("sat=1", "target=21", "room=20", "run", "run")
        self.assertEqual(out[0][0], "run:")
        self.assertEqual(len(out[0]), 3)
        self.assertEqual(out[1], ["run:"])

    def test_change_publishes_only_the_changed_topic(self):
        out = self.runs("sat=1", "target=21", "room=20", "run", "target=22", "run")
        self.assertEqual(out[1], ["run:", "sat/climate_target=22.00"])

    def test_toggle_with_force_republishes_both_with_the_new_meaning(self):
        out = self.runs("sat=1", "target=21", "room=20", "tr=19", "trset=18", "run",
                        "sat=0", "force", "run")
        self.assertEqual(out[1], ["run:", "sat/climate_target=18.00", "sat/climate_current=19.00"])

    def test_force_republishes_unchanged_values(self):
        out = self.runs("sat=1", "target=21", "room=20", "run", "run", "force", "run")
        self.assertEqual(out[1], ["run:"])
        self.assertEqual(out[2], ["run:", "sat/climate_target=21.00", "sat/climate_current=20.00"])

    def test_toggle_without_force_would_stay_stale(self):
        # Same values on both sides of the toggle: only the force flag makes the topics go out again.
        out = self.runs("sat=1", "target=21", "room=20", "tr=20", "trset=21", "run", "sat=0", "run")
        self.assertEqual(out[1], ["run:"])

    def test_no_value_no_publish(self):
        out = self.runs("sat=0", "run")   # Tr and TrSet unknown (NAN) until the first OpenTherm frame
        self.assertEqual(out[0], ["run:"])
        out = self.runs("sat=1", "target=21", "run")   # SAT has no room temperature
        self.assertEqual(out[0], ["run:", "sat/climate_target=21.00"])

    def test_mqtt_down_publishes_nothing(self):
        out = self.runs("mqtt=0", "sat=1", "target=21", "room=20", "run")
        self.assertEqual(out[0], ["run:"])

    def test_every_publish_is_inside_a_burst_window(self):
        out = self.runs("sat=1", "target=21", "room=20", "run")
        self.assertEqual(out[1], ["windows=1", "sends=1", "outside=0", "open=0"])

    # --- heartbeat, like hvac_mode / hvac_action (300 s) ---

    def test_heartbeat_not_at_299_s_but_at_300_s(self):
        out = self.runs("sat=1", "target=21", "room=20", "t=1000", "run",
                        "t=300999", "run", "t=301000", "run")
        self.assertEqual(len(out[0]), 3)
        self.assertEqual(out[1], ["run:"])   # 299.999 s after the send
        self.assertEqual(out[2], ["run:", "sat/climate_target=21.00", "sat/climate_current=20.00"])   # 300 s

    def test_heartbeat_restarts_after_the_send(self):
        out = self.runs("sat=1", "target=21", "room=20", "t=0", "run", "t=300000", "run",
                        "t=599000", "run", "t=600000", "run")
        self.assertEqual(out[1][0], "run:")
        self.assertEqual(len(out[1]), 3)
        self.assertEqual(out[2], ["run:"])
        self.assertEqual(len(out[3]), 3)

    def test_heartbeat_per_topic_after_a_change(self):
        # target changes at 100 s (published, its window restarts); current keeps its stamp from t=0
        out = self.runs("sat=1", "target=21", "room=20", "t=0", "run", "target=22", "t=100000", "run",
                        "t=300000", "run")
        self.assertEqual(out[1], ["run:", "sat/climate_target=22.00"])
        self.assertEqual(out[2], ["run:", "sat/climate_current=20.00"])

    # --- ADR-174: HA online -> requestMQTTRepublishAll() -> satRepublishCoreState() -> satClimateBridgeForce() ---

    def test_core_republish_brings_both_climate_topics_and_preset_back_while_sat_is_off(self):
        out = self.runs("sat=0", "tr=19.5", "trset=18", "preset=1", "t=1000", "core",
                        "t=2000", "core", "republish", "t=3000", "core")
        first, quiet, again = out[0], out[1], out[2]
        self.assertIn("sat/preset=comfort", first)
        self.assertIn("sat/climate_target=18.00", first)
        self.assertEqual(quiet, ["core:"])   # nothing changed, nothing sent (and well inside the heartbeat)
        for topic in ("sat/enabled=false", "sat/mode=off", "sat/preset=comfort",
                      "sat/climate_target=18.00", "sat/climate_current=19.50"):
            self.assertIn(topic, again)

    def test_core_republish_with_sat_on(self):
        out = self.runs("sat=1", "target=21", "room=20", "preset=1", "core", "republish", "core")
        for topic in ("sat/preset=comfort", "sat/climate_target=21.00", "sat/climate_current=20.00"):
            self.assertIn(topic, out[1])


class T4Wiring(unittest.TestCase):
    """Source-level wiring that the compiled cuts above cannot see."""

    def test_publisher_runs_every_pass_not_only_while_enabled(self):
        sat = read("SATcontrol.ino")
        loop = body(sat, "void satControlLoop()")
        self.assertIsNotNone(loop)
        self.assertIn("satPublishClimateBridge();", loop)
        # not inside satPublishMQTT(), which returns while SAT is off
        self.assertNotIn("satPublishClimateBridge", body(sat, "void satPublishMQTT()") or "")

    def test_toggle_and_republish_raise_the_force_flag(self):
        settings = read("settingStuff.ino")
        branch = segment(settings, 'strcasecmp_P(field, PSTR("SATenabled")) == 0', 'strcasecmp_P(field, PSTR("SATsystem"))')
        self.assertIsNotNone(branch)
        self.assertIn("satClimateBridgeForce();", branch)
        sat = read("SATcontrol.ino")
        self.assertIn("satClimateBridgeForce();", body(sat, "void satRepublishCoreState()") or "")
        mq = read("MQTTstuff.ino")
        online = segment(mq, "Home Assistant went online!", "} else") or ""
        self.assertIn("requestMQTTRepublishAll();", online, "HA going online no longer calls requestMQTTRepublishAll()")
        core = read("OTGW-Core.ino")
        self.assertIn("satRepublishCoreState();", body(core, "void requestMQTTRepublishAll()") or "")


if __name__ == "__main__":
    unittest.main(verbosity=2)
