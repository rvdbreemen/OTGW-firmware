// test/host/test_override_modlevel.cpp  (TASK-1389, ADR-118)
//
// The MaxRelModLevelSetting/override topic through the REAL print_f88(), recordOTOverride() and
// publishActiveOverrides(), sliced by test_override_modlevel.py. Frames are filled in the way
// processOT() leaves them after its one-frame delay: the T of a (T,R) pair carries
// bGatewaySubstituted, the B of a (B,A) pair carries it too, and the A carries bAnswerOverride.
#include "dhw_host_shim.h"

#define HAS_DIRECT_OT 1

#include "gen_types.inc"

struct {
  struct { uint32_t iTrThermostatMs = 0; } otBus;
  struct { OTGWHardwareMode eMode = HW_MODE_PIC; } hw;
  struct { bool bESPactive = false; bool bPICactive = false; } flash;
} state;
struct { struct { bool bEnable = true; } mqtt; } settings;
static OTdataStruct OTcurrentSystemState;
OpenthermData_t OTdata;
OTlookup_t      OTlookupitem;
OTOverrideEntry_t otOverrideStore[OVERRIDE_STORE_MAX] = {};

// ---- doubles ------------------------------------------------------------------------------
static void AddLogf(const char*, ...) {}
static void feedWatchDog() {}
static const char* messageIDToString(OTLibMessageID id) {
  switch ((int)id) {
    case 14: return "MaxRelModLevelSetting";
    case 27: return "Toutside";
    default: return "Other";
  }
}
static std::map<std::string, std::string> g_sent;      // topic -> last payload
static bool sendMQTTData(const char* t, const char* v, const bool = false) { g_sent[t] = v; return true; }
static void publishToSourceTopic(const char*, const char*, byte) {}
static void updateDHWWaterMeter(float, uint32_t) {}

#include "gen_fw_h.inc"
#include "gen_core.inc"

// ---- harness ------------------------------------------------------------------------------
static const OTlookup_t kMM  { 14, OT_WRITE, ot_f88, "MaxRelModLevelSetting", "Maximum relative modulation level setting", "%", false };
static const OTlookup_t kOut { 27, OT_RW,    ot_f88, "Toutside", "Outside temperature", "C", true };

static void fresh() {
  g_ms = 100000;
  std::memset(otOverrideStore, 0, sizeof otOverrideStore);
  OTcurrentSystemState = OTdataStruct();
  g_sent.clear();
}

// One frame reaching print_f88(); the flags are the ones processOT() sets for the pairing.
static void frame(const OTlookup_t& lk, OTGW_response_type src, OTLibMessageType type, float value,
                  bool gatewaySubstituted, bool answerOverride, float& state_field) {
  OpenthermData_t d{};
  d.type = (byte)type; d.masterslave = (type & 0x4) ? 1 : 0; d.id = lk.id;
  d.f88(value);
  d.rsptype = (byte)src;
  d.bGatewaySubstituted = gatewaySubstituted ? 1 : 0;
  d.bAnswerOverride = answerOverride ? 1 : 0;
  d.time = (time_t)g_ms;
  OTdata = d;
  OTlookupitem = lk;
  print_f88(state_field);
}

static const char* published(const char* topic) {
  g_sent.clear();
  publishActiveOverrides();
  auto it = g_sent.find(topic);
  return it == g_sent.end() ? "(not published)" : it->second.c_str();
}

static char g_got[256];
static const char* kMMTopic = "MaxRelModLevelSetting/override";

static void caseO1() {
  // MM=100 with a thermostat that writes 0: T, R, B, A as the PIC sends them.
  fresh();
  float& f = OTcurrentSystemState.MaxRelModLevelSetting;
  frame(kMM, OTGW_THERMOSTAT,        OT_WRITE_DATA, 0.0f,   true,  false, f);
  frame(kMM, OTGW_REQUEST_BOILER,    OT_WRITE_DATA, 100.0f, false, false, f);
  frame(kMM, OTGW_BOILER,            OT_WRITE_ACK,  0.0f,   true,  false, f);
  frame(kMM, OTGW_ANSWER_THERMOSTAT, OT_WRITE_ACK,  0.0f,   false, true,  f);
  const std::string got = published(kMMTopic);
  std::snprintf(g_got, sizeof g_got, "%s = %s, canonical state = %.2f", kMMTopic, got.c_str(), f);
  verdict("O1", "after MM=100 (T 0, R 100, B ack, A ack) the override topic is 100.00", got == "100.00", g_got);
}

static void caseO2() {
  // The PIC's Write-Ack alone: its data byte is undefined for MaxRelModLevelSetting, so it is no override.
  fresh();
  float& f = OTcurrentSystemState.MaxRelModLevelSetting;
  frame(kMM, OTGW_ANSWER_THERMOSTAT, OT_WRITE_ACK, 0.0f, false, true, f);
  const std::string got = published(kMMTopic);
  std::snprintf(g_got, sizeof g_got, "%s = %s", kMMTopic, got.c_str());
  verdict("O2", "a Write-Ack A frame (data undefined, not echoed) records no override", got == "(not published)", g_got);
}

static void caseO3() {
  // Control: the headline ADR-118 case. An answer-override Read-Ack for Toutside is the injected value.
  fresh();
  float& f = OTcurrentSystemState.Toutside;
  frame(kOut, OTGW_ANSWER_THERMOSTAT, OT_READ_ACK, 20.5f, false, true, f);
  const std::string got = published("Toutside/override");
  std::snprintf(g_got, sizeof g_got, "Toutside/override = %s", got.c_str());
  verdict("O3", "control: answer-override Read-Ack for Toutside still publishes 20.50", got == "20.50", g_got);
}

static void caseO4() {
  // Control: a substituted T with no R after it still records the thermostat's value, as before.
  fresh();
  float& f = OTcurrentSystemState.MaxRelModLevelSetting;
  frame(kMM, OTGW_THERMOSTAT, OT_WRITE_DATA, 50.0f, true, false, f);
  const std::string got = published(kMMTopic);
  std::snprintf(g_got, sizeof g_got, "%s = %s", kMMTopic, got.c_str());
  verdict("O4", "control: a substituted T alone still records its value (50.00)", got == "50.00", g_got);
}

int main() {
  std::printf("== MaxRelModLevelSetting/override through the real code (TASK-1389) ==\n");
  caseO1(); caseO2(); caseO3(); caseO4();
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
