//=======================================================================
// test_pr_m_otd_letters.cpp
//
// TASK-1951 host harness, driven by test_pr_m_otd_letters.py. Compiles the
// REAL handlePRresponse() from OTGW-Core.ino and the REAL PicSettingsSection
// from PICtypes.h, sliced by anchor from the revision under test, plus the
// real PICreturnSensor.h and picSettingValue.h. Only I/O is doubled here:
// debug output (captured), MQTT (captured), WebSocket.
//
// The defect: OT-Direct answers PR=M with its own modes (OTDirect.ino, case
// 'M': G, M, P = bypass, S = standalone master, L = loopback). The handler
// accepted only G/M, logged "PR=M unexpected value" for S/P/L and left the
// gateway-mode state as it was.
//
// Cases (CASE <id> pass|FAIL), each from a cleared state:
//   D1  PR: M=S -> gateway mode ON, known, otgw-pic/gateway_mode ON, no warning
//   D2  PR: M=L -> gateway mode ON, known, published ON, no warning
//   D3  PR: M=P -> gateway mode OFF, known, published OFF, no warning
//   C1  PR: M=G -> ON (PIC letter unchanged)
//   C2  PR: M=M -> OFF (PIC letter unchanged); C2b the same after a G
//   C3  PR: M=X -> warning logged, state stays unknown, nothing published
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <string>

// ---- platform doubles -------------------------------------------------
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define CCONOFF(x) (x ? "ON" : "OFF")
#define strlcpy otgw_test_strlcpy   // musl declares strlcpy; keep the harness copy under its own name
static inline size_t strlcpy(char* d, const char* s, size_t n) {
  size_t l = std::strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; std::memcpy(d, s, c); d[c] = '\0'; }
  return l;
}
#define snprintf_P snprintf

static std::string g_debug;
static void OTDebugTf(const char* fmt, ...) {
  char b[256];
  va_list ap; va_start(ap, fmt); std::vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
  g_debug += b;
}
static void DebugTf(const char*, ...) {}

bool sendMQTTData(const __FlashStringHelper*, const char*, const bool = false) { return true; }
static std::string g_picTopic, g_picPayload;
static int g_picPublishes = 0;
void sendMQTTDataPic(const __FlashStringHelper* topic, const char* value) {
  g_picTopic = reinterpret_cast<const char*>(topic);
  g_picPayload = value;
  g_picPublishes++;
}
static bool hasWebSocketClients() { return false; }
static void sendEventToWebSocket(char, const char*) {}
static void applyFramelessTret(float) {}
static uint32_t g_millis = 0;
static uint32_t millis() { return g_millis; }
static uint32_t picBoardTretMs = 0;
static bool     picBoardTretSeen = false;
static uint16_t picTretPollResults = 0;

#include "../../src/OTGW-firmware/PICreturnSensor.h"
#include "../../src/OTGW-firmware/picSettingValue.h"

// ---- sliced firmware (revision under test) -----------------------------
#include "pic_settings_struct.inc"

struct OtBusDouble { bool bGatewayMode = false; bool bGatewayModeKnown = false; };
struct StateDouble { PicSettingsSection picSettings; OtBusDouble otBus; };
static StateDouble state;
struct PicBootDouble { bool bTempSensor = false; };
struct SettingsDouble { PicBootDouble picBoot; };
static SettingsDouble settings;
static const uint8_t OTGWpictempid = 239;
static bool picTempSensorHasData() { return false; }
static void setMQTTConfigPending(uint8_t) {}

#include "pic_settings_handler.inc"

// ---- cases -------------------------------------------------------------
static int g_fail = 0;

// Each case alternates the expected mode with the one before it, so the
// handler's own change detection (static prev state) always publishes.
static void runCase(const char* id, const char* line, bool expectKnown, bool expectGateway) {
  state.otBus.bGatewayMode = false;
  state.otBus.bGatewayModeKnown = false;
  g_debug.clear(); g_picTopic.clear(); g_picPayload.clear(); g_picPublishes = 0;
  handlePRresponse(line, std::strlen(line));
  const bool warned = g_debug.find("unexpected value") != std::string::npos;
  bool ok;
  if (expectKnown) {
    ok = state.otBus.bGatewayModeKnown && state.otBus.bGatewayMode == expectGateway && !warned
         && g_picPublishes == 1 && g_picTopic == "gateway_mode"
         && g_picPayload == (expectGateway ? "ON" : "OFF");
  } else {
    ok = !state.otBus.bGatewayModeKnown && warned && g_picPublishes == 0;
  }
  std::printf("CASE %s %s  [%s] known=%d gateway=%d published=%d payload=%s warned=%d\n", id,
              ok ? "pass" : "FAIL", line, state.otBus.bGatewayModeKnown, state.otBus.bGatewayMode,
              g_picPublishes, g_picPayload.c_str(), warned);
  if (!ok) g_fail++;
}

int main() {
  // The PIC letters first, so the controls see the same history on OLD and FIX.
  runCase("C2", "PR: M=M", true, false);   // first call: prev unknown, publishes OFF
  runCase("C1", "PR: M=G", true, true);
  runCase("C2b", "PR: M=M", true, false);
  runCase("D1", "PR: M=S", true, true);
  runCase("D3", "PR: M=P", true, false);
  runCase("D2", "PR: M=L", true, true);
  runCase("C3", "PR: M=X", false, false);
  std::printf("%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
