/*
***************************************************************************
**  Program  : test/host/test_local_frame_consumers.cpp
**
**  Host harness for TASK-1185: a frame the gateway made itself (TASK-1086's
**  local answer: every OT-Direct A, and every B of OT-Direct loopback mode) is
**  not boiler evidence. TASK-1086 kept such frames out of the two unsupported
**  bitmaps; this harness covers the other consumers:
**    A  boilerAckedRead / boilerAckedWrite (the "acknowledged" column of
**       /api/v2/otgw/ot-support)
**    L  the OT log suffix on a type-7 slave frame
**    N  satNotifyBoilerFrameSeen(), the edge hook that switches SAT simulation
**       off when a real boiler appears
**    H  satBoilerHardwarePresent(), SAT's availability gate: on a combo build
**       (HAS_PIC and HAS_DIRECT_OT) it also read state.otBus.bBoilerState,
**       which counts loopback B frames on purpose (TASK-1138)
**  A genuine boiler B, and on the PIC path every frame (ADR-103 proxy A
**  included), keep counting.
**
**  The code under test is NOT copied here. test_local_frame_consumers.py slices
**  it by anchor, verbatim, into generated/local_frame_consumers/<model>-<old|fix>/:
**    types.inc  OTGW-Core.h, as test_boiler_unsupported_origin.py slices it
**    core.inc   OTGW-Core.ino: the frame queue, dispatchOTGWInputLine(), the
**               bitmaps, evaluateOTBusLiveness() and processOT() (same slices)
**    otd.inc    OTDirect.ino: otCurrentMode and the IS_*_MODE() macros,
**               otHideReports, bridgeFrameToParser() (same slices), plus
**               otBoilerCacheValid[]
**    hw.inc     Hardwaretypes.h: OTGWHardwareMode
**    gate.inc   OTGW-firmware.h: isPICEnabled(), isOTDirectEnabled().
**               OTDirect.ino: otDirectBoilerPresent(). SATcontrol.ino:
**               satDebugForceBoilerPresent, satBoilerHardwarePresent(),
**               satNotifyBoilerFrameSeen()
**  The runner compiles this file once per board model and revision:
**    /DMODEL=1  combo    HAS_PIC 1, HAS_DIRECT_OT 1 (runtime transport choice)
**    /DMODEL=2  classic  HAS_PIC 1, HAS_DIRECT_OT 0
**    /DMODEL=3  otgw32   HAS_PIC 0, HAS_DIRECT_OT 1
**  The model flags apply to gate.inc only. core.inc is built with HAS_PIC 0, as
**  in test_boiler_unsupported_origin.cpp (the PIC task's RX-error report is not
**  modelled); the frame chain does not depend on the board.
**
**  Test doubles, for hardware, the OS and output only (as in
**  test_boiler_unsupported_origin.cpp), except that AddLog() records the OT log
**  text of the case so the L cases can read the suffix.
**
**  Usage: test_local_frame_consumers.exe [dump-dir]
**    With a dump-dir it writes <dump-dir>/cases.txt, one line per case.
**  Exit 0 when every case holds, 1 otherwise.
**
**  Run: python test/host/test_local_frame_consumers.py --old-rev HEAD
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <deque>
#include <initializer_list>
#include <string>
#include <type_traits>

#ifndef MODEL
#define MODEL 1
#endif
#define MODEL_COMBO   1
#define MODEL_CLASSIC 2
#define MODEL_OTGW32  3

// ---- platform surface the slices need -------------------------------------------
typedef uint8_t byte;
typedef const char* PGM_P;
#define PROGMEM
#define PSTR(s) (s)
class __FlashStringHelper;                        // incomplete: pointer-only, as on Arduino
#define F(s) ((const __FlashStringHelper*)(s))
#define snprintf_P snprintf
#define strstr_P strstr
#define strcasecmp_P _stricmp
#define memcpy_P memcpy
#define PROGMEM_readAnything(src, dest) memcpy_P(&(dest), (src), sizeof(dest))   // helperStuff.h
#define HAS_PIC 0                                  // core.inc: the PIC task's RX-error report is not modelled
#define LED2 2

// Debug output is dropped. The OT log keeps its text for the L cases.
#define Debugln(...)      ((void)0)
#define OTDebugln(...)    ((void)0)
#define OTDebugT(...)     ((void)0)
#define OTDebugTf(...)    ((void)0)
#define OTDebugFlush()    ((void)0)
#define ClrLog()          ((void)0)
static std::string g_log;
static void AddLog(const char* s) { g_log += s; }
static void AddLog(const __FlashStringHelper* s) { g_log += reinterpret_cast<const char*>(s); }
#define AddLogf(...)      ((void)0)
#define AddLogln()        (g_log += '\n')
static const char* getOTLogTimestamp() { return ""; }

static unsigned long g_millis = 1000000UL;
static unsigned long millis() { return g_millis; }

// FreeRTOS handles (platform_esp32.h). One mutex, never contended here.
using PlatformQueue = void*;
using PlatformMutex = void*;
static bool platformMutexLock(PlatformMutex, uint32_t = 0) { return true; }
static void platformMutexUnlock(PlatformMutex) {}

// ---- OTGW-Core.h and Hardwaretypes.h, sliced ----------------------------------------
#include "types.inc"
#include "hw.inc"

// FreeRTOS value queue, as platformQueueSend/platformQueueReceive copy it. Every
// item sent is also written to the queue trace as "<prefix>:<source>".
static std::deque<OTFrameMsg> g_queue;
static std::string g_trace;
static bool platformQueueSend(PlatformQueue, const void* item) {
  const OTFrameMsg* m = static_cast<const OTFrameMsg*>(item);
  g_queue.push_back(*m);
  if (!g_trace.empty()) g_trace += ',';
  g_trace += m->line[0];
  g_trace += ':';
  g_trace += std::to_string((unsigned)m->source);
  return true;
}
static bool platformQueueReceive(PlatformQueue, void* item, uint32_t = 0) {
  if (g_queue.empty()) return false;
  *static_cast<OTFrameMsg*>(item) = g_queue.front();
  g_queue.pop_front();
  return true;
}

// state / settings: only the fields the slices touch.
static struct {
  struct {
    bool bPSmode; time_t tBoilerLastSeen; time_t tThermostatLastSeen;
    bool bBoilerState; bool bThermostatState; bool bOnline;
  } otBus;
  struct { bool bAvailable; } pic;                 // isPICEnabled()
  struct { OTGWHardwareMode eMode; } hw;           // isOTDirectEnabled()
  struct { bool bBoilerDetectedFlag; } sat;        // satNotifyBoilerFrameSeen()
} state;
static struct {
  struct { bool bOTmessage; bool bEnable; } mqtt;
  struct { bool bSimulation; } sat;                // satNotifyBoilerFrameSeen()
} settings;
static struct {
  float OpenThermVersionSlave; float OpenThermVersionMaster;          // read by useV4xReservedIdRules()
  uint16_t error01, error02, error03, error04;                        // the non-OT "Error 0x" branches
} OTcurrentSystemState;
static uint32_t lastOTmsgMs = 0;
static char cMsg[128];
static char ot_log_buffer[8];

// Output: MQTT, WebSocket, telnet events, port 25238, LED, watchdog.
static bool sendMQTTData(const __FlashStringHelper*, const char*, bool = false) { return true; }
static void sendLogToWebSocket(const char*) {}
static void sendEventToWebSocket(char, const char*, int = -1) {}
static void reportOTGWEvent(const char*, char, bool = false) {}
static void publishBoilerConnectedState() {}
static void publishThermostatConnectedState() {}
static void publishOTGWConnectedState() {}
static void publishHvacMode(bool) {}
static void publishHvacAction(bool) {}
static void drainOTRawQueue() {}
static void blinkLEDnow(uint8_t) {}
static void feedWatchDog() {}
static void otDirectBridgeWriteLine(const char*, size_t) {}
static bool isFlashing() { return false; }
void satNotifyBoilerFrameSeen();                   // the real one, from gate.inc

// Value decoding and publishing after the bitmap block, and the non-OT branches.
static uint32_t mqttSendSuccessCount = 0;
static struct { bool pending; } mqttPendingSlot;
struct OTPublishGate { explicit OTPublishGate(bool) {} };
static bool shouldPublishMQTTForID(byte, byte, uint16_t) { return false; }
static void decodeAndPublishOTValue() {}
static void confirmMQTTPublishSlot() {}
static bool getMQTTConfigDone(uint8_t) { return true; }
static void setMQTTConfigPending(uint8_t) {}
static uint16_t currentTrackedSeconds() { return 0; }
static void setMsgLastUpdated(uint8_t, uint16_t) {}
static void stampThermostatRoomTemp() {}
static void leavePSMode(PGM_P, PGM_P) {}
static void enterPSMode(PGM_P, PGM_P, bool) {}
static void checkCommandResponse(const char*, unsigned int) {}
static void handlePRresponse(const char*, size_t) {}
static bool handlePICStatusToken(const char*) { return false; }
static bool isOTGWStartupQuietPeriodActive() { return false; }
static void processPSSummary(const char*, int) {}

// ---- code under test: OTGW-Core.ino and OTDirect.ino, sliced ---------------------
#include "core.inc"
#include "otd.inc"

// ---- code under test: the SAT gate, sliced, with this build's board flags -----------
#undef HAS_PIC
#if MODEL == MODEL_CLASSIC
  #define HAS_PIC       1
  #define HAS_DIRECT_OT 0
#elif MODEL == MODEL_OTGW32
  #define HAS_PIC       0
  #define HAS_DIRECT_OT 1
#else
  #define HAS_PIC       1
  #define HAS_DIRECT_OT 1
#endif
#include "gate.inc"

// ---- harness ------------------------------------------------------------------------
enum : unsigned { kReadData = 0, kWriteData = 1, kReadAck = 4, kWriteAck = 5, kUnknownId = 7 };

// Parity by counting bits, independent of the firmware's parity helpers.
static unsigned long withEvenParity(unsigned long f) {
  f &= 0x7FFFFFFFUL;
  int n = 0;
  for (int b = 0; b < 31; b++) n += (int)((f >> b) & 1UL);
  return (n & 1) ? (f | 0x80000000UL) : f;
}
static unsigned long otFrame(unsigned type, unsigned id, uint16_t data) {
  return withEvenParity(((unsigned long)type << 28) | ((unsigned long)id << 16) | data);
}

// One frame, as a producer hands it to the parser.
struct Step { bool pic; char prefix; unsigned long frame; };
static Step otd(char prefix, unsigned long frame) { return { false, prefix, frame }; }
static Step pic(char prefix, unsigned long frame) { return { true, prefix, frame }; }

static void emit(const Step& s) {
  if (s.pic) {
    char line[16];
    snprintf(line, sizeof(line), "%c%08lX", s.prefix, s.frame);   // the PIC's line format
    dispatchOTGWInputLine(line, 9);
  } else {
    bridgeFrameToParser(s.prefix, s.frame);
  }
}

static void flushFrame() { dispatchOTGWInputLine("T00000000", 9); }

// The world a case starts from: which transport runs, in which OT-Direct mode,
// whether a boiler has answered MsgID 3, and whether SAT simulation is on.
struct World {
  OTGWHardwareMode hw;
  OTDirectMode mode;
  bool picAvailable;
  bool cache3;
  bool satSim;
};
static const World kOtdGateway  = { HW_MODE_OT_DIRECT, OTD_MODE_GATEWAY,  false, true,  true };
static const World kOtdMaster   = { HW_MODE_OT_DIRECT, OTD_MODE_MASTER,   false, true,  true };
static const World kOtdLoopback = { HW_MODE_OT_DIRECT, OTD_MODE_LOOPBACK, false, true,  true };
static const World kPic         = { HW_MODE_PIC,       OTD_MODE_GATEWAY,  true,  false, true };

static void resetAll(const World& w) {
  memset(boilerLastMasterWasWrite, 0, sizeof(boilerLastMasterWasWrite));
  memset(boilerUnsupportedRead, 0, sizeof(boilerUnsupportedRead));
  memset(boilerUnsupportedWrite, 0, sizeof(boilerUnsupportedWrite));
  memset(boilerAckedRead, 0, sizeof(boilerAckedRead));
  memset(boilerAckedWrite, 0, sizeof(boilerAckedWrite));
  memset(thermostatSentRead, 0, sizeof(thermostatSentRead));
  memset(thermostatSentWrite, 0, sizeof(thermostatSentWrite));
  boilerUnsupportedDirty = boilerFileDirty = thermostatFileDirty = false;
  // The liveness window runs on time(nullptr); clear it so no case inherits the
  // previous case's boiler.
  state.otBus.tBoilerLastSeen = state.otBus.tThermostatLastSeen = 0;
  state.otBus.bBoilerState = state.otBus.bThermostatState = false;
  state.sat.bBoilerDetectedFlag = false;
  memset(otBoilerCacheValid, 0, sizeof(otBoilerCacheValid));
  satDebugForceBoilerPresent = false;
  state.hw.eMode = w.hw;
  state.pic.bAvailable = w.picAvailable;
  otCurrentMode = w.mode;
  otBoilerCacheValid[3] = w.cache3;
  settings.sat.bSimulation = w.satSim;
}

static void runFrames(std::initializer_list<Step> steps) {
  g_millis += 10000;
  g_trace.clear();
  g_log.clear();
  for (const Step& s : steps) emit(s);
  const std::string trace = g_trace;
  flushFrame();
  drainOTFrameQueue();
  g_trace = trace;                                // the case's own frames, without the flush
}

static bool bitOf(const uint8_t* bitmap, int id) { return (bitmap[id >> 3] & (1u << (id & 7))) != 0; }
static std::string hex(const uint8_t* bitmap) {
  std::string s;
  char b[4];
  for (int i = 0; i < 32; i++) { snprintf(b, sizeof(b), "%02X", bitmap[i]); s += b; }
  return s;
}
static std::string suffixes() {
  std::string s;
  if (g_log.find(" (boiler does not implement)") != std::string::npos) s += "impl;";
  if (g_log.find(" (boiler rejected write)") != std::string::npos) s += "rej;";
  if (g_log.find(" (gateway answer)") != std::string::npos) s += "gw;";
  return s.empty() ? "none" : s;
}

static FILE* g_dump = nullptr;
static int g_failures = 0;

// Every case writes the same dump line: the acked bitmaps (A), the log suffixes
// (L), the SAT edge flag (N), the SAT gate (H), and for comparison everything else
// the frames touched, so the runner can check that a fix changes only its target.
static void finish(const char* id, const char* title, bool ok, const char* got) {
  if (!ok) g_failures++;
  std::printf("CASE %s %s\n     %s\n     got : %s\n     queue: %s\n", id, ok ? "pass" : "FAIL", title, got,
              g_trace.c_str());
  if (g_dump) {
    std::fprintf(g_dump, "%s A=ar:%s,aw:%s L=%s N=%d H=%d other=ur:%s,uw:%s,tr:%s,tw:%s,lw:%s,boiler:%d trace=%s\n",
                 id, hex(boilerAckedRead).c_str(), hex(boilerAckedWrite).c_str(), suffixes().c_str(),
                 (int)state.sat.bBoilerDetectedFlag, (int)satBoilerHardwarePresent(),
                 hex(boilerUnsupportedRead).c_str(), hex(boilerUnsupportedWrite).c_str(),
                 hex(thermostatSentRead).c_str(), hex(thermostatSentWrite).c_str(),
                 hex(boilerLastMasterWasWrite).c_str(), (int)state.otBus.bBoilerState, g_trace.c_str());
  }
}

static void caseAcked(const char* id, const char* title, const World& w, std::initializer_list<Step> steps,
                      bool write, int checkId, bool want) {
  resetAll(w);
  runFrames(steps);
  const bool got = bitOf(write ? boilerAckedWrite : boilerAckedRead, checkId);
  char g[96];
  snprintf(g, sizeof g, "MsgID %d acknowledged %s: want %d, got %d", checkId, write ? "write" : "read",
           (int)want, (int)got);
  finish(id, title, got == want, g);
}

static void caseLog(const char* id, const char* title, const World& w, std::initializer_list<Step> steps,
                    const char* want) {
  resetAll(w);
  runFrames(steps);
  const std::string got = suffixes();
  char g[96];
  snprintf(g, sizeof g, "log suffix: want %s, got %s", want, got.c_str());
  finish(id, title, got == want, g);
}

static void caseEdge(const char* id, const char* title, const World& w, std::initializer_list<Step> steps,
                     bool want) {
  resetAll(w);
  runFrames(steps);
  const bool got = state.sat.bBoilerDetectedFlag;
  char g[96];
  snprintf(g, sizeof g, "SAT boiler-detected flag: want %d, got %d", (int)want, (int)got);
  finish(id, title, got == want, g);
}

static void caseGate(const char* id, const char* title, const World& w, std::initializer_list<Step> steps,
                     bool force, bool want) {
  resetAll(w);
  satDebugForceBoilerPresent = force;
  runFrames(steps);
  const bool got = satBoilerHardwarePresent();
  char g[128];
  snprintf(g, sizeof g, "satBoilerHardwarePresent(): want %d, got %d (bBoilerState %d, MsgID 3 answered %d)",
           (int)want, (int)got, (int)state.otBus.bBoilerState, (int)otBoilerCacheValid[3]);
  finish(id, title, got == want, g);
}

int main(int argc, char** argv) {
  if (argc > 1) {
    std::string path = std::string(argv[1]) + "/cases.txt";
    g_dump = std::fopen(path.c_str(), "w");
  }
  static int queueHandle, mutexHandle;
  otFrameQueue = &queueHandle;                    // setupOTConcurrency()
  otStateMutex = &mutexHandle;
  flushFrame();                                   // the very first frame is only stored (processOT)
  drainOTFrameQueue();

  const uint16_t c40 = 0x2800, c48 = 0x3000, c50 = 0x3200, c80 = 0x5000;

#if MODEL == MODEL_COMBO
  std::printf("== A: acknowledged bitmaps ==\n");
  caseAcked("A1", "gateway mode, SR=26, thermostat READ(26): T + A READ-ACK(50.0) from the SR table "
                  "(loopOTDirect; override_reply C13b) is not a boiler Ack",
            kOtdGateway, { otd('T', otFrame(kReadData, 26, 0)), otd('A', otFrame(kReadAck, 26, c50)) }, false, 26, false);
  caseAcked("A2", "master mode, thermostat WRITE(1, 40.0): T + A WRITE-ACK(40.0), the echo "
                  "handleMasterModeSlaveFrame() makes without the boiler, is not a boiler Ack",
            kOtdMaster, { otd('T', otFrame(kWriteData, 1, c40)), otd('A', otFrame(kWriteAck, 1, c40)) }, true, 1, false);
  caseAcked("A3", "loopback mode, thermostat READ(25): T + B READ-ACK(42.5) from the simulated table "
                  "(override_reply C22) is not a boiler Ack",
            kOtdLoopback, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, 25, false);
  caseAcked("A4", "loopback mode, the gateway's WRITE(1, 40.0): R + B WRITE-ACK echo (override_reply C16) "
                  "is not a boiler Ack",
            kOtdLoopback, { otd('R', otFrame(kWriteData, 1, c40)), otd('B', otFrame(kWriteAck, 1, c40)) }, true, 1, false);
  caseAcked("A5", "master mode, thermostat READ(3) with MsgID 3 cached: T + A READ-ACK(3) replayed from the cache "
                  "(override_reply K7) is not itself a boiler Ack; the B that filled the cache was",
            kOtdMaster, { otd('T', otFrame(kReadData, 3, 0)), otd('A', otFrame(kReadAck, 3, 0x04D2)) }, false, 3, false);
  caseAcked("AC1", "control: gateway mode, real boiler, thermostat READ(25): T + B READ-ACK counts",
            kOtdGateway, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, 25, true);
  caseAcked("AC2", "control: gateway mode, real boiler, thermostat WRITE(24): T + B WRITE-ACK counts (override_reply C9)",
            kOtdGateway, { otd('T', otFrame(kWriteData, 24, 0x1433)), otd('B', otFrame(kWriteAck, 24, 0x1433)) }, true, 24, true);
  caseAcked("AC3", "control: gateway mode, RM=26: T + B READ-ACK(48.0) + A READ-ACK(50.0); the boiler's B counts "
                   "(override_reply C17)",
            kOtdGateway, { otd('T', otFrame(kReadData, 26, 0)), otd('B', otFrame(kReadAck, 26, c48)),
                           otd('A', otFrame(kReadAck, 26, c50)) }, false, 26, true);
  caseAcked("AC4", "control: PIC, boiler READ(25): T + B READ-ACK counts",
            kPic, { pic('T', otFrame(kReadData, 25, 0)), pic('B', otFrame(kReadAck, 25, 0x2A80)) }, false, 25, true);
  caseAcked("AC5", "control: PIC proxy answer without a B (ADR-103), READ(57): T + A READ-ACK(80.0) counts",
            kPic, { pic('T', otFrame(kReadData, 57, 0)), pic('A', otFrame(kReadAck, 57, c80)) }, false, 57, true);
  caseAcked("AC6", "control: PIC proxy answer without a B (ADR-103), WRITE(24): T + A WRITE-ACK counts",
            kPic, { pic('T', otFrame(kWriteData, 24, 0x1433)), pic('A', otFrame(kWriteAck, 24, 0x1433)) }, true, 24, true);

  std::printf("== L: OT log suffix on a type-7 slave frame ==\n");
  caseLog("L1", "master mode, thermostat READ(27) with nothing cached: T + A UNKNOWN-DATAID "
                "(handleMasterModeSlaveFrame; override_reply U2)",
          kOtdMaster, { otd('T', otFrame(kReadData, 27, 0)), otd('A', otFrame(kUnknownId, 27, 0)) }, "gw;");
  caseLog("L2", "gateway mode, UI=71, thermostat WRITE(71): T + A UNKNOWN-DATAID (loopOTDirect UI table; override_reply C13a)",
          kOtdGateway, { otd('T', otFrame(kWriteData, 71, c50)), otd('A', otFrame(kUnknownId, 71, 0)) }, "gw;");
  caseLog("L3", "loopback mode, thermostat READ(40), not in the simulated table: T + B UNKNOWN-DATAID (override_reply K3)",
          kOtdLoopback, { otd('T', otFrame(kReadData, 40, 0)), otd('B', otFrame(kUnknownId, 40, 0)) }, "gw;");
  caseLog("LC1", "control: gateway mode, real boiler, READ(40): T + B UNKNOWN-DATAID keeps the boiler suffix",
          kOtdGateway, { otd('T', otFrame(kReadData, 40, 0)), otd('B', otFrame(kUnknownId, 40, 0)) }, "impl;");
  caseLog("LC2", "control: gateway mode, real boiler rejects WRITE(71): T + B UNKNOWN-DATAID keeps the write suffix",
          kOtdGateway, { otd('T', otFrame(kWriteData, 71, c50)), otd('B', otFrame(kUnknownId, 71, c50)) }, "rej;");
  caseLog("LC3", "control: PIC proxy answer without a B (ADR-103), READ(70): T + A UNKNOWN-DATAID keeps the boiler suffix",
          kPic, { pic('T', otFrame(kReadData, 70, 0)), pic('A', otFrame(kUnknownId, 70, 0)) }, "impl;");

  std::printf("== N: SAT simulation edge hook (simulation on) ==\n");
  caseEdge("N1", "loopback mode, thermostat READ(40): T + B UNKNOWN-DATAID does not raise the boiler-detected flag",
           kOtdLoopback, { otd('T', otFrame(kReadData, 40, 0)), otd('B', otFrame(kUnknownId, 40, 0)) }, false);
  caseEdge("N2", "loopback mode, the gateway's WRITE(1, 40.0): R + B WRITE-ACK does not raise it",
           kOtdLoopback, { otd('R', otFrame(kWriteData, 1, c40)), otd('B', otFrame(kWriteAck, 1, c40)) }, false);
  caseEdge("NC1", "control: gateway mode, real boiler READ(25): T + B READ-ACK raises it",
           kOtdGateway, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, true);
  caseEdge("NC2", "control: PIC, boiler READ(40): T + B UNKNOWN-DATAID raises it",
           kPic, { pic('T', otFrame(kReadData, 40, 0)), pic('B', otFrame(kUnknownId, 40, 0)) }, true);
  {
    World w = kOtdGateway;
    w.satSim = false;
    caseEdge("NC3", "control: simulation off, real boiler READ(25): the hook raises nothing",
             w, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false);
  }

  std::printf("== H: SAT availability gate, combo build ==\n");
  caseGate("H1", "combo in OT-Direct loopback: loopback B frames set bBoilerState (TASK-1138), "
                 "but a simulated boiler is not a real one",
           kOtdLoopback, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, false);
  {
    World w = kOtdGateway;
    w.cache3 = false;
    caseGate("H2", "combo in OT-Direct gateway mode, boiler B frames but MsgID 3 not answered yet: "
                   "the OT-Direct rule decides, as on an OTGW32 (case O2)",
             w, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, false);
  }
  caseGate("HC1", "control: combo in OT-Direct gateway mode, boiler answered MsgID 3: present",
           kOtdGateway, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, true);
  caseGate("HC2", "control: combo in PIC mode, boiler B frames on the PIC: present",
           kPic, { pic('T', otFrame(kReadData, 25, 0)), pic('B', otFrame(kReadAck, 25, 0x2A80)) }, false, true);
  caseGate("HC3", "control: combo in PIC mode, no boiler frames: absent",
           kPic, {}, false, false);
  caseGate("HC4", "control: combo in OT-Direct loopback, the TASK-802 debug override: present",
           kOtdLoopback, {}, true, true);
#elif MODEL == MODEL_CLASSIC
  std::printf("== H: SAT availability gate, classic build (PIC only) ==\n");
  caseGate("K1", "control: classic, boiler B frames on the PIC: present",
           kPic, { pic('T', otFrame(kReadData, 25, 0)), pic('B', otFrame(kReadAck, 25, 0x2A80)) }, false, true);
  caseGate("K2", "control: classic, no boiler frames: absent", kPic, {}, false, false);
  {
    World w = kPic;
    w.picAvailable = false;
    caseGate("K3", "control: classic, PIC not available but boiler B frames seen: present (the gate reads bBoilerState)",
             w, { pic('T', otFrame(kReadData, 25, 0)), pic('B', otFrame(kReadAck, 25, 0x2A80)) }, false, true);
  }
#else
  std::printf("== H: SAT availability gate, OTGW32 build (OT-Direct only) ==\n");
  caseGate("O1", "control: OTGW32 in loopback: absent",
           kOtdLoopback, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, false);
  {
    World w = kOtdGateway;
    w.cache3 = false;
    caseGate("O2", "control: OTGW32 in gateway mode, boiler B frames but MsgID 3 not answered yet: absent",
             w, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, false);
  }
  caseGate("O3", "control: OTGW32 in gateway mode, boiler answered MsgID 3: present",
           kOtdGateway, { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, false, true);
#endif

  if (g_dump) std::fclose(g_dump);
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
