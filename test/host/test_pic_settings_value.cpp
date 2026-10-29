//=======================================================================
// test_pic_settings_value.cpp
//
// TASK-1406 host harness, driven by test_pic_settings_value.py. Compiles the
// REAL handlePRresponse() from OTGW-Core.ino and the REAL PicSettingsSection
// from PICtypes.h, sliced by anchor from the revision under test, plus the
// real PICreturnSensor.h and picSettingValue.h. Only I/O is doubled here:
// debug output, MQTT (captured), WebSocket.
//
// The defect: after a PIC reprogram PR=Q answered with the single byte 0xFF.
// handlePRresponse() stored and published it as is, so the MQTT payload and
// /api/v2/pic/settings carried a byte that is not valid UTF-8.
//
// Cases (CASE <id> pass|FAIL):
//   D1  PR: Q=<0xFF>  -> stored and published value is printable ASCII
//   C1  PR: Q=E       -> stored and published "E" (normal letter unchanged)
//   C2  PR: B=17:52 12-03-2023 -> build date with spaces unchanged
//   C3  PR: Q=<0xFF> twice -> the second identical answer publishes nothing
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
static inline size_t strlcpy(char* d, const char* s, size_t n) {
  size_t l = std::strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; std::memcpy(d, s, c); d[c] = '\0'; }
  return l;
}
#define snprintf_P snprintf

static void OTDebugTf(const char*, ...) {}
static void DebugTf(const char*, ...) {}

static std::string g_lastTopic, g_lastPayload;
static int g_publishCount = 0;
bool sendMQTTData(const __FlashStringHelper* topic, const char* value, const bool = false) {
  g_lastTopic = reinterpret_cast<const char*>(topic);
  g_lastPayload = value;
  g_publishCount++;
  return true;
}
void sendMQTTDataPic(const __FlashStringHelper*, const char*) {}
static bool hasWebSocketClients() { return false; }
static void sendEventToWebSocket(char, const char*) {}
static void applyFramelessTret(float) {}

#include "../../src/OTGW-firmware/PICreturnSensor.h"
#include "../../src/OTGW-firmware/picSettingValue.h"

// ---- sliced firmware (revision under test) -----------------------------
#include "pic_settings_struct.inc"

struct OtBusDouble { bool bGatewayMode = false; bool bGatewayModeKnown = false; };
struct StateDouble { PicSettingsSection picSettings; OtBusDouble otBus; };
static StateDouble state;

#include "pic_settings_handler.inc"

// ---- cases -------------------------------------------------------------
static int g_fail = 0;

static bool printableAscii(const std::string& s) {
  for (unsigned char c : s) if (c < 0x20 || c > 0x7E) return false;
  return true;
}
static std::string shown(const std::string& s) {
  std::string out;
  char b[8];
  for (unsigned char c : s) {
    if (c >= 0x20 && c <= 0x7E) out += (char)c;
    else { std::snprintf(b, sizeof(b), "\\x%02X", c); out += b; }
  }
  return out;
}
static void report(const char* id, bool ok, const char* what, const std::string& detail) {
  std::printf("CASE %s %s  %s [%s]\n", id, ok ? "pass" : "FAIL", what, shown(detail).c_str());
  if (!ok) g_fail++;
}
static void feed(const char* line) { handlePRresponse(line, std::strlen(line)); }

int main() {
  // D1: the GH #688 / bench 2026-10-26 answer.
  g_publishCount = 0;
  feed("PR: Q=\xFF");
  report("D1", g_publishCount == 1 && printableAscii(g_lastPayload)
                 && printableAscii(state.picSettings.sResetCause)
                 && state.picSettings.sResetCause[0] != '\0',
         "PR=Q 0xFF is stored and published as printable ASCII", g_lastPayload);

  // C3: the same raw answer again must not publish a second time.
  g_publishCount = 0;
  feed("PR: Q=\xFF");
  report("C3", g_publishCount == 0, "a repeated identical PR=Q answer publishes nothing",
         std::to_string(g_publishCount) + " publish(es)");

  // C1: a normal reset cause letter is unchanged.
  g_publishCount = 0;
  feed("PR: Q=E");
  report("C1", g_publishCount == 1 && g_lastPayload == "E"
                 && std::strcmp(state.picSettings.sResetCause, "E") == 0,
         "PR=Q E is stored and published unchanged", g_lastPayload);

  // C2: a printable value with spaces on another register is unchanged.
  g_publishCount = 0;
  feed("PR: B=17:52 12-03-2023");
  report("C2", g_publishCount == 1 && g_lastPayload == "17:52 12-03-2023"
                 && std::strcmp(state.picSettings.sBuilddate, "17:52 12-03-2023") == 0,
         "PR=B build date is stored and published unchanged", g_lastPayload);

  std::printf("%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
