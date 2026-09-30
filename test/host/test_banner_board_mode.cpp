// test/host/test_banner_board_mode.cpp  (TASK-1180)
//
// processOT()'s banner branch re-enables a PIC that boot detection missed and,
// on the combo board (HAS_RUNTIME_HW_DETECT), may persist the board mode. That
// branch is only reachable while the PIC UART is open after a missed probe,
// i.e. from a forced Classic mode (1, or 3 for the S3 Mini Pro). It must never
// rewrite 3 to 1. test_banner_board_mode.py slices the REAL recovery block out
// of the banner branch into generated/banner_board_mode.inc (wrapped in a
// function body below); only the state/settings fields it touches, the debug
// macro and a recording writeSettings() are test doubles.

#include <cstdio>

#define HAS_RUNTIME_HW_DETECT 1
#define F(s) (s)
#define DebugTln(...) ((void)0)

enum HardwareMode { HW_MODE_PIC, HW_MODE_OT_DIRECT, HW_MODE_DEGRADED };
struct { struct { bool bAvailable; } pic; struct { HardwareMode eMode; bool bClassicPro; } hw; } state;
struct { int iBoardMode; } settings;
static int g_writes = 0;
static void writeSettings(bool) { g_writes++; }
static bool g_otDirect = false;                 // hardware state double (TASK-1181)
static bool isOTDirectEnabled() { return g_otDirect; }

static void bannerRecovery() {
#include "generated/banner_board_mode.inc"
}

static int g_failures = 0;
static void run(const char* name, int mode, bool pro, int wantMode, int wantWrites) {
  state.pic.bAvailable = false;
  state.hw.eMode = HW_MODE_DEGRADED;
  state.hw.bClassicPro = pro;
  settings.iBoardMode = mode;
  g_writes = 0;
  bannerRecovery();
  bool ok = state.pic.bAvailable && state.hw.eMode == HW_MODE_PIC &&
            settings.iBoardMode == wantMode && g_writes == wantWrites;
  std::printf("  %s  %-42s mode %d -> %d (want %d), writes %d (want %d)\n", ok ? "PASS" : "FAIL",
              name, mode, settings.iBoardMode, wantMode, g_writes, wantWrites);
  if (!ok) g_failures++;
}

int main() {
  run("forced S3 Mini Pro (3) keeps its variant", 3, true, 3, 0);
  run("forced S3 Mini (1) stays 1", 1, false, 1, 0);
  run("auto (0) on a Pro learns 3", 0, true, 3, 1);
  run("auto (0) on an S3 Mini learns 1", 0, false, 1, 1);

  // TASK-1181: OT-Direct active (combo on OTGW32, auto mode): a banner line can
  // only come from the frame replay, so nothing may change or be persisted.
  g_otDirect = true;
  state.pic.bAvailable = false; state.hw.eMode = HW_MODE_OT_DIRECT;
  state.hw.bClassicPro = false; settings.iBoardMode = 0; g_writes = 0;
  bannerRecovery();
  bool ok = !state.pic.bAvailable && state.hw.eMode == HW_MODE_OT_DIRECT &&
            settings.iBoardMode == 0 && g_writes == 0;
  std::printf("  %s  %-42s bAvailable %d, mode %d, writes %d (want 0, 0, 0)\n", ok ? "PASS" : "FAIL",
              "replayed banner while OT-Direct runs", (int)state.pic.bAvailable, settings.iBoardMode, g_writes);
  if (!ok) g_failures++;
  g_otDirect = false;
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
