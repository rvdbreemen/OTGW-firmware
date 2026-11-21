//=======================================================================
// test_pic_sim_gate_pr.cpp
//
// TASK-1943 / ADR-244 host harness, driven by test_pic_sim_gate_pr.py.
// Compiles the REAL simulation gate block of sendPICSerial() (OTGW-Core.ino,
// ADR-117 path (a)) wrapped in a function, and the REAL
// satSimulationBlocksBusTx() (SATcontrol.ino), both sliced by anchor from the
// revision under test. Only the state and the trace output are doubled.
//
// The block returns false when it holds a command back and falls through when
// the command may go to the PIC, so the wrapper returns true for "sent".
//
// Cases (CASE <id> pass|FAIL):
//   D1  SAT sim on,  PR=A   -> sent, no blocked-command trace
//   D2  SAT sim on,  PR=E   -> sent (the TASK-1940 return-water poll)
//   D3  replay sim on, PR=M -> sent
//   C1  SAT sim on,  CS=40  -> held back, traced as last blocked command
//   C2  SAT sim on,  GW=1   -> held back
//   C3  replay sim on, CS=40 -> held back
//   C4  no sim,      CS=40  -> sent
//   C5  no sim,      PR=A   -> sent
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define strncmp_P strncmp
#define strlcpy otgw_test_strlcpy   // musl declares strlcpy; keep the harness copy under its own name
static inline size_t strlcpy(char* d, const char* s, size_t n) {
  size_t l = std::strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; std::memcpy(d, s, c); d[c] = '\0'; }
  return l;
}
static uint32_t millis() { return 1234; }
static void SATTraceTf(const char*, ...) {}

struct SatStateDouble {
  char     sLastBlockedCmd[24]  = {0};
  uint32_t iLastBlockedCmdMs    = 0;
  char     sSimTraceCmd[16][24] = {{0}};
  uint32_t iSimTraceMs[16]      = {0};
  uint8_t  iSimTraceHead        = 0;
  uint8_t  iSimTraceCount       = 0;
};
struct DebugDouble { bool bOTGWSimulation = false; };
struct StateDouble { SatStateDouble sat; DebugDouble debug; };
struct SatSettingsDouble { bool bSimulation = false; };
struct SettingsDouble { SatSettingsDouble sat; };
static StateDouble state;
static SettingsDouble settings;

#include "gate.inc"

static bool sendThroughGate(const char* buf, int len) {
#include "block.inc"
  return true;
}

static int g_fail = 0;
static void runCase(const char* id, bool satSim, bool replaySim, const char* cmd, bool expectSent) {
  state = StateDouble();
  settings.sat.bSimulation = satSim;
  state.debug.bOTGWSimulation = replaySim;
  std::string line = std::string(cmd) + "\r\n";
  const bool sent = sendThroughGate(line.c_str(), (int)line.size());
  const bool traced = state.sat.sLastBlockedCmd[0] != '\0';
  bool ok = (sent == expectSent);
  if (ok && satSim) ok = expectSent ? !traced : (std::strcmp(state.sat.sLastBlockedCmd, cmd) == 0);
  std::printf("CASE %s %s  sat=%d replay=%d cmd=%s sent=%d lastBlocked=[%s]\n", id, ok ? "pass" : "FAIL",
              satSim, replaySim, cmd, sent, state.sat.sLastBlockedCmd);
  if (!ok) g_fail++;
}

int main() {
  runCase("D1", true,  false, "PR=A",  true);
  runCase("D2", true,  false, "PR=E",  true);
  runCase("D3", false, true,  "PR=M",  true);
  runCase("C1", true,  false, "CS=40", false);
  runCase("C2", true,  false, "GW=1",  false);
  runCase("C3", false, true,  "CS=40", false);
  runCase("C4", false, false, "CS=40", true);
  runCase("C5", false, false, "PR=A",  true);
  std::printf("%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
