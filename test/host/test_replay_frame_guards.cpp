// test/host/test_replay_frame_guards.cpp  (TASK-1649, TASK-1650)
//
// test_replay_frame_guards.py slices two REAL blocks out of processOT() in OTGW-Core.ino:
//   outside27.inc : the MsgID 27 outside-temperature stamp (TASK-1390 block)
//   banner.inc    : the statements between the OTGW_BANNER test and the PIC-recovery block
// Only the state fields they touch and millis()/isOTDirectEnabled()/satNotifyPicRestart() are test doubles.

#include <cstdio>
#include <cstdint>
#include <cmath>

enum { OTGW_BOILER = 1, OTGW_THERMOSTAT = 2, OTGW_ANSWER_THERMOSTAT = 3, OTGW_REQUEST_BOILER = 4 };
enum { OT_READ_ACK = 1, OT_WRITE_DATA = 2, OT_INVALID_DATA = 3 };
enum { OT_TrSet = 16 };
enum { HW_MODE_UNKNOWN = 0, HW_MODE_PIC = 1 };
struct OpenthermData_t { int id; int rsptype; int type; bool skipthis; bool bGatewaySubstituted; bool bReplayed;
                         bool bLocalAnswer; float value;
                         float f88() const { return value; } };
OpenthermData_t OTdata;
struct { struct { uint32_t iToutsideBoilerMs; } otBus; struct { bool bAvailable; } pic;
         struct { int eMode; bool bClassicPro; } hw; } state;
struct { int iBoardMode; } settings;
static uint32_t g_ms = 5000;
static uint32_t millis() { return g_ms; }
static bool g_otDirect = false;
static bool isOTDirectEnabled() { return g_otDirect; }
static int g_restarts = 0;
static void satNotifyPicRestart() { g_restarts++; }
static int g_writes = 0;
static void writeSettings(bool) { g_writes++; }
#define F(x) x
#define DebugTln(x) ((void)(x))
#define HAS_RUNTIME_HW_DETECT 1
static int g_satSetpoints = 0;
static float g_satLast = 0.0f;
static void satOnThermostatSetpoint(float v) { g_satSetpoints++; g_satLast = v; }
// processOT()'s own parameter (TASK-1185): true for a line from the /otgw_simulation.log replay.
static bool replayed = false;

#include "generated/replay_guard_outside27_pred.inc"
static void outsideStamp() {
#include "generated/replay_guard_outside27.inc"
}
static void bannerSeen() {
#include "generated/replay_guard_banner.inc"
}
static void bannerRecovery() {
#include "generated/replay_guard_banner_recovery.inc"
}
static void trsetFeed() {
#include "generated/replay_guard_trset.inc"
}

static int g_failures = 0;
static void check(const char* name, long got, long want) {
  bool ok = got == want;
  std::printf("  %s  %-58s got %ld want %ld\n", ok ? "PASS" : "FAIL", name, got, want);
  if (!ok) g_failures++;
}

static void outside(const char* name, int rsp, int type, bool replayed, uint32_t want, bool local = false) {
  OTdata = {27, rsp, type, false, false, replayed, local};
  state.otBus.iToutsideBoilerMs = 111;
  outsideStamp();
  check(name, state.otBus.iToutsideBoilerMs, want);
}

int main() {
  std::printf("TASK-1649 MsgID 27 stamp\n");
  outside("live boiler Read-Ack stamps", OTGW_BOILER, OT_READ_ACK, false, 5000);
  outside("replayed boiler Read-Ack leaves the stamp", OTGW_BOILER, OT_READ_ACK, true, 111);
  outside("thermostat frame leaves the stamp", OTGW_THERMOSTAT, OT_WRITE_DATA, false, 111);
  std::printf("TASK-1896 follow-up 1903.2: gateway-made B frame (OT-Direct loopback)\n");
  outside("B Read-Ack 27 with bLocalAnswer=1 does not stamp", OTGW_BOILER, OT_READ_ACK, false, 111, true);
  std::printf("TASK-1896 gateway Write-Data 27 (PIC DS18B20 / OT=)\n");
  outside("gateway Write-Data to the boiler stamps", OTGW_REQUEST_BOILER, OT_WRITE_DATA, false, 5000);
  outside("replayed gateway Write-Data leaves the stamp", OTGW_REQUEST_BOILER, OT_WRITE_DATA, true, 111);
  outside("gateway Invalid-Data (failed sensor) leaves the stamp", OTGW_REQUEST_BOILER, OT_INVALID_DATA, false, 111);
  outside("gateway answer to the thermostat leaves the stamp", OTGW_ANSWER_THERMOSTAT, OT_READ_ACK, false, 111);

  std::printf("TASK-1650 banner and satNotifyPicRestart\n");
  g_otDirect = false; g_restarts = 0; bannerSeen();
  check("PIC board banner calls satNotifyPicRestart", g_restarts, 1);
  g_otDirect = true; g_restarts = 0; bannerSeen();
  check("OT-Direct banner (replayed) does not call it", g_restarts, 0);

  std::printf("TASK-1902 banner PIC recovery (auto board mode, no PIC, OT-Direct off)\n");
  g_otDirect = false;
  state.pic.bAvailable = false; state.hw.eMode = HW_MODE_UNKNOWN; state.hw.bClassicPro = false;
  settings.iBoardMode = 0; g_writes = 0; replayed = true; bannerRecovery();
  check("replayed banner leaves iBoardMode at 0", settings.iBoardMode, 0);
  check("replayed banner does not re-enable the PIC", state.pic.bAvailable, 0);
  check("replayed banner writes no settings", g_writes, 0);
  g_restarts = 0; bannerSeen();
  check("replayed banner on a PIC board still calls satNotifyPicRestart", g_restarts, 1);
  state.pic.bAvailable = false; state.hw.eMode = HW_MODE_UNKNOWN;
  settings.iBoardMode = 0; g_writes = 0; replayed = false; bannerRecovery();
  check("real banner sets iBoardMode 1 (Classic)", settings.iBoardMode, 1);
  check("real banner re-enables the PIC", state.pic.bAvailable, 1);
  check("real banner persists once", g_writes, 1);
  replayed = false;

  std::printf("TASK-1901 thermostat MsgID 16 into SAT\n");
  OTdata = {16, OTGW_THERMOSTAT, OT_WRITE_DATA, false, false, true, false, 19.5f};
  g_satSetpoints = 0; trsetFeed();
  check("replayed T Write-Data 16 does not reach satOnThermostatSetpoint", g_satSetpoints, 0);
  OTdata = {16, OTGW_THERMOSTAT, OT_WRITE_DATA, false, false, false, false, 20.5f};
  g_satSetpoints = 0; g_satLast = 0.0f; trsetFeed();
  check("real T Write-Data 16 reaches it once", g_satSetpoints, 1);
  check("with its value x100 (20.5)", (long)lroundf(g_satLast * 100.0f), 2050);

  std::printf(g_failures ? "FAIL: %d\n" : "PASS\n", g_failures);
  return g_failures ? 1 : 0;
}
