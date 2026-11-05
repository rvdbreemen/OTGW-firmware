// test/host/test_replay_frame_guards.cpp  (TASK-1649, TASK-1650)
//
// test_replay_frame_guards.py slices two REAL blocks out of processOT() in OTGW-Core.ino:
//   outside27.inc : the MsgID 27 outside-temperature stamp (TASK-1390 block)
//   banner.inc    : the statements between the OTGW_BANNER test and the PIC-recovery block
// Only the state fields they touch and millis()/isOTDirectEnabled()/satNotifyPicRestart() are test doubles.

#include <cstdio>
#include <cstdint>

enum { OTGW_BOILER = 1, OTGW_THERMOSTAT = 2 };
enum { OT_READ_ACK = 1, OT_WRITE_DATA = 2 };
struct { int id; int rsptype; int type; bool skipthis; bool bGatewaySubstituted; bool bReplayed; } OTdata;
struct { struct { uint32_t iToutsideBoilerMs; } otBus; } state;
static uint32_t g_ms = 5000;
static uint32_t millis() { return g_ms; }
static bool g_otDirect = false;
static bool isOTDirectEnabled() { return g_otDirect; }
static int g_restarts = 0;
static void satNotifyPicRestart() { g_restarts++; }

static void outsideStamp() {
#include "generated/replay_guard_outside27.inc"
}
static void bannerSeen() {
#include "generated/replay_guard_banner.inc"
}

static int g_failures = 0;
static void check(const char* name, long got, long want) {
  bool ok = got == want;
  std::printf("  %s  %-58s got %ld want %ld\n", ok ? "PASS" : "FAIL", name, got, want);
  if (!ok) g_failures++;
}

static void outside(const char* name, int rsp, int type, bool replayed, uint32_t want) {
  OTdata = {27, rsp, type, false, false, replayed};
  state.otBus.iToutsideBoilerMs = 111;
  outsideStamp();
  check(name, state.otBus.iToutsideBoilerMs, want);
}

int main() {
  std::printf("TASK-1649 MsgID 27 stamp\n");
  outside("live boiler Read-Ack stamps", OTGW_BOILER, OT_READ_ACK, false, 5000);
  outside("replayed boiler Read-Ack leaves the stamp", OTGW_BOILER, OT_READ_ACK, true, 111);
  outside("thermostat frame leaves the stamp", OTGW_THERMOSTAT, OT_WRITE_DATA, false, 111);

  std::printf("TASK-1650 banner and satNotifyPicRestart\n");
  g_otDirect = false; g_restarts = 0; bannerSeen();
  check("PIC board banner calls satNotifyPicRestart", g_restarts, 1);
  g_otDirect = true; g_restarts = 0; bannerSeen();
  check("OT-Direct banner (replayed) does not call it", g_restarts, 0);

  std::printf(g_failures ? "FAIL: %d\n" : "PASS\n", g_failures);
  return g_failures ? 1 : 0;
}
