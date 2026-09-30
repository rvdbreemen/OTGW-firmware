/*
***************************************************************************
**  Program  : test/host/test_boiler_unsupported_origin.cpp
**
**  Host harness for TASK-1086: an answer the gateway made itself must not
**  count as boiler evidence in the boiler-unsupported bitmaps
**  (boilerUnsupportedRead / boilerUnsupportedWrite), in either direction. It
**  may not set a verdict (an UNKNOWN-DATAID the gateway made) and may not
**  retract one (a READ-ACK or WRITE-ACK the gateway made). A genuine boiler B
**  on the OT-Direct path and a PIC-mode proxy A (ADR-103) keep counting.
**
**  The code under test is NOT copied here. test_boiler_unsupported_origin.py
**  slices it by anchor, verbatim, into generated/boiler_unsupported_origin/<old|fix>/:
**    types.inc  OTGW-Core.h: MAX_BUFFER_READ, OTLibMessageType, OTLibMessageID,
**               OTlookup_t and OTmap[], OT_MSGID_MAX, the processOT()
**               declaration, OTFrameSource through the drainOTFrameQueue()
**               declaration (OTFrameMsg, the enqueueOTFrame() declaration),
**               OTStateLock, OTGW_response_type, OpenthermData_t
**    core.inc   OTGW-Core.ino: OTdata/delayedOTdata/tmpOTdata, the queue and
**               mutex handles, enqueueOTFrame(), drainOTFrameQueue(), the
**               reserved-id profile helpers, is_value_valid(),
**               dispatchOTGWInputLine(), isvalidotmsg(), the six bitmaps and
**               their accessors, evaluateOTBusLiveness(), processOT()
**    otd.inc    OTDirecttypes.h: OTDirectMode. OTDirect.ino: otCurrentMode and
**               the IS_*_MODE() macros, otHideReports, bridgeFrameToParser()
**  The runner compiles this same file once per revision, with that revision's
**  directory on the include path.
**
**  Test doubles, for hardware, the OS and output only:
**    FreeRTOS     the frame queue is a FIFO of OTFrameMsg items that also
**                 records the prefix and source byte of every item (the queue
**                 trace); the state mutex always locks.
**    PIC          the harness formats the PIC's lines ("T%08lX") and hands them
**                 to the real enqueueOTFrame() with source OTFRAME_SRC_PIC, as
**                 the PIC task does (dispatchOTGWInputLine() is the replay's
**                 entry point since TASK-1185).
**    output       MQTT, WebSocket, telnet/debug and the OT log macros (GCC
**                 statement expressions in the firmware), the port 25238
**                 mirror (otDirectBridgeWriteLine), the raw-byte queue, the LED
**                 and the watchdog.
**    publishing   what processOT() does after the bitmap block to decode and
**                 publish a value (decodeAndPublishOTValue(), the MQTT throttle,
**                 discovery and last-updated bookkeeping, the room-temperature
**                 stamp), and the command-response and PS=1 branches for
**                 non-OT lines, which no case reaches. None of them reads or
**                 writes a bitmap.
**
**  How a case runs: reset the six bitmaps and their dirty flags, preset any
**  earlier "unsupported" verdict the case needs, set the OT-Direct mode, emit
**  the frames the named producer emits, then one PIC flush frame
**  ('T' READ-DATA id 0): processOT() processes each frame one frame late, so
**  the flush pushes the case's last frame through. drainOTFrameQueue() then
**  runs once, as loop() does after the producers. The clock moves 10 s between
**  cases so no (T,R) or (B,A) pairing reaches across two cases.
**
**  Each OT-Direct sequence is what a producer in OTDirect.ino emits; the case
**  title names it, and the named test_override_reply.cpp case already proves
**  that producer's T/R/B/A log (build_and_run_override_reply.ps1).
**
**  Usage: test_boiler_unsupported_origin.exe [dump-dir]
**    With a dump-dir it writes <dump-dir>/cases.txt, one line per case, so two
**    revisions can be compared byte for byte.
**  Exit 0 when every case holds, 1 otherwise.
**
**  Run: python test/host/test_boiler_unsupported_origin.py --old-rev HEAD
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
#define HAS_PIC 0                                  // the PIC task's RX-error report is not modelled
#define LED2 2

// Debug and OT-log output (telnet and the WebSocket OT monitor). The firmware
// macros are GCC statement expressions; the harness drops the output.
#define Debugln(...)      ((void)0)
#define OTDebugln(...)    ((void)0)
#define OTDebugT(...)     ((void)0)
#define OTDebugTf(...)    ((void)0)
#define OTDebugFlush()    ((void)0)
#define ClrLog()          ((void)0)
#define AddLog(...)       ((void)0)
#define AddLogf(...)      ((void)0)
#define AddLogln()        ((void)0)

static unsigned long g_millis = 1000000UL;
static unsigned long millis() { return g_millis; }

// FreeRTOS handles (platform_esp32.h). One mutex, never contended here.
using PlatformQueue = void*;
using PlatformMutex = void*;
static bool platformMutexLock(PlatformMutex, uint32_t = 0) { return true; }
static void platformMutexUnlock(PlatformMutex) {}

// ---- OTGW-Core.h, sliced --------------------------------------------------------
#include "types.inc"

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
    time_t tRealBoilerLastSeen;                    // stamped by revisions that have it (TASK-1185)
  } otBus;
} state;
static struct { struct { bool bOTmessage; bool bEnable; } mqtt; } settings;
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
static void satNotifyBoilerFrameSeen() {}

// Publishing that follows the bitmap block in processOT(), and the handlers of
// non-OT lines. No case reaches the latter; none of them touches a bitmap.
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
    enqueueOTFrame(line, 9, false, OTFRAME_SRC_PIC);   // as the PIC task does
  } else {
    bridgeFrameToParser(s.prefix, s.frame);
  }
}

static void flushFrame() { enqueueOTFrame("T00000000", 9, false, OTFRAME_SRC_PIC); }

static void resetBitmaps() {
  memset(boilerLastMasterWasWrite, 0, sizeof(boilerLastMasterWasWrite));
  memset(boilerUnsupportedRead, 0, sizeof(boilerUnsupportedRead));
  memset(boilerUnsupportedWrite, 0, sizeof(boilerUnsupportedWrite));
  memset(boilerAckedRead, 0, sizeof(boilerAckedRead));
  memset(boilerAckedWrite, 0, sizeof(boilerAckedWrite));
  memset(thermostatSentRead, 0, sizeof(thermostatSentRead));
  memset(thermostatSentWrite, 0, sizeof(thermostatSentWrite));
  boilerUnsupportedDirty = boilerFileDirty = thermostatFileDirty = false;
}
static void setBit(uint8_t* bitmap, int id) { bitmap[id >> 3] |= (uint8_t)(1u << (id & 7)); }

static std::string hex(const uint8_t* bitmap) {
  std::string s;
  char b[4];
  for (int i = 0; i < 32; i++) { snprintf(b, sizeof(b), "%02X", bitmap[i]); s += b; }
  return s;
}

static FILE* g_dump = nullptr;
static int g_failures = 0;

static void runCase(const char* id, const char* title, OTDirectMode mode,
                    std::initializer_list<int> presetRead, std::initializer_list<int> presetWrite,
                    std::initializer_list<Step> steps, int checkId, bool wantRead, bool wantWrite) {
  resetBitmaps();
  for (int i : presetRead) setBit(boilerUnsupportedRead, i);
  for (int i : presetWrite) setBit(boilerUnsupportedWrite, i);
  otCurrentMode = mode;
  g_millis += 10000;
  g_trace.clear();
  for (const Step& s : steps) emit(s);
  const std::string trace = g_trace;
  flushFrame();
  drainOTFrameQueue();

  const bool gotRead = isBoilerMsgIdUnsupportedRead((uint8_t)checkId);
  const bool gotWrite = isBoilerMsgIdUnsupportedWrite((uint8_t)checkId);
  const bool ok = gotRead == wantRead && gotWrite == wantWrite;
  if (!ok) g_failures++;
  std::printf("CASE %s %s  MsgID %d unsupported read/write: want %d/%d, got %d/%d\n     %s\n     queue: %s\n",
              id, ok ? "pass" : "FAIL", checkId, (int)wantRead, (int)wantWrite, (int)gotRead, (int)gotWrite,
              title, trace.c_str());
  if (g_dump) {
    std::fprintf(g_dump, "%s verdict=ur:%s,uw:%s,ud:%d,fd:%d other=ar:%s,aw:%s,tr:%s,tw:%s,lw:%s,td:%d trace=%s\n",
                 id, hex(boilerUnsupportedRead).c_str(), hex(boilerUnsupportedWrite).c_str(),
                 (int)boilerUnsupportedDirty, (int)boilerFileDirty,
                 hex(boilerAckedRead).c_str(), hex(boilerAckedWrite).c_str(),
                 hex(thermostatSentRead).c_str(), hex(thermostatSentWrite).c_str(),
                 hex(boilerLastMasterWasWrite).c_str(), (int)thermostatFileDirty, trace.c_str());
  }
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

  const uint16_t c30 = 0x1E00, c40 = 0x2800, c48 = 0x3000, c50 = 0x3200, c80 = 0x5000;

  std::printf("== defect: an answer the gateway made itself (want: no verdict set, none retracted) ==\n");
  runCase("D1", "master mode, thermostat READ(27) with nothing cached: T + A UNKNOWN-DATAID "
                "(handleMasterModeSlaveFrame; override_reply U2, K4)",
          OTD_MODE_MASTER, {}, {},
          { otd('T', otFrame(kReadData, 27, 0)), otd('A', otFrame(kUnknownId, 27, 0)) }, 27, false, false);
  runCase("D2", "gateway mode, UI=70, thermostat READ(70): T + A UNKNOWN-DATAID "
                "(loopOTDirect UI table; override_reply C13a)",
          OTD_MODE_GATEWAY, {}, {},
          { otd('T', otFrame(kReadData, 70, 0)), otd('A', otFrame(kUnknownId, 70, 0)) }, 70, false, false);
  runCase("D3", "gateway mode, UI=71, thermostat WRITE(71): T + A UNKNOWN-DATAID "
                "(loopOTDirect UI table, any frame type; override_reply C13a)",
          OTD_MODE_GATEWAY, {}, {},
          { otd('T', otFrame(kWriteData, 71, c50)), otd('A', otFrame(kUnknownId, 71, 0)) }, 71, false, false);
  runCase("D4", "loopback mode, thermostat READ(40), not in the simulated table: T + B UNKNOWN-DATAID "
                "(sendMasterRequestAsync -> simulateLoopbackResponse; override_reply K3)",
          OTD_MODE_LOOPBACK, {}, {},
          { otd('T', otFrame(kReadData, 40, 0)), otd('B', otFrame(kUnknownId, 40, 0)) }, 40, false, false);
  runCase("D5", "loopback mode, the gateway's scheduled READ(50): R + B UNKNOWN-DATAID "
                "(bridgeSentRequest R for a gateway request; override_reply C11, K3)",
          OTD_MODE_LOOPBACK, {}, {},
          { otd('R', otFrame(kReadData, 50, 0)), otd('B', otFrame(kUnknownId, 50, 0)) }, 50, false, false);
  runCase("D6", "loopback mode, earlier real verdict on READ(25), thermostat READ(25): T + B READ-ACK(42.5) "
                "may not retract it (override_reply C22)",
          OTD_MODE_LOOPBACK, { 25 }, {},
          { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, 25, true, false);
  runCase("D7", "loopback mode, earlier real verdict on WRITE(1), the gateway's WRITE(1, 40.0): R + B WRITE-ACK "
                "may not retract it (simulateLoopbackResponse echoes a write; override_reply C16)",
          OTD_MODE_LOOPBACK, {}, { 1 },
          { otd('R', otFrame(kWriteData, 1, c40)), otd('B', otFrame(kWriteAck, 1, c40)) }, 1, false, true);

  std::printf("== control: boiler evidence (want: the same on both sides) ==\n");
  runCase("G1", "gateway mode, real boiler, thermostat READ(40): T + B UNKNOWN-DATAID sets the read verdict "
                "(handleMasterResponse; override_reply C10 shape)",
          OTD_MODE_GATEWAY, {}, {},
          { otd('T', otFrame(kReadData, 40, 0)), otd('B', otFrame(kUnknownId, 40, 0)) }, 40, true, false);
  runCase("G2", "gateway mode, real boiler, earlier verdict on READ(25): T + B READ-ACK retracts it",
          OTD_MODE_GATEWAY, { 25 }, {},
          { otd('T', otFrame(kReadData, 25, 0)), otd('B', otFrame(kReadAck, 25, 0x2A80)) }, 25, false, false);
  runCase("G3", "gateway mode, real boiler, earlier verdict on WRITE(24): T + B WRITE-ACK retracts it "
                "(override_reply C9)",
          OTD_MODE_GATEWAY, {}, { 24 },
          { otd('T', otFrame(kWriteData, 24, 0x1433)), otd('B', otFrame(kWriteAck, 24, 0x1433)) }, 24, false, false);
  runCase("G4", "gateway mode, real boiler rejects WRITE(71): T + B UNKNOWN-DATAID sets the write verdict",
          OTD_MODE_GATEWAY, {}, {},
          { otd('T', otFrame(kWriteData, 71, c50)), otd('B', otFrame(kUnknownId, 71, c50)) }, 71, false, true);
  runCase("G5", "master mode, the gateway's READ(27) to a real boiler: R + B UNKNOWN-DATAID sets the read verdict "
                "(override_reply U1)",
          OTD_MODE_MASTER, {}, {},
          { otd('R', otFrame(kReadData, 27, 0)), otd('B', otFrame(kUnknownId, 27, 0)) }, 27, true, false);
  runCase("G6", "gateway mode, CS=40 on WRITE(1, 30.0), boiler UNKNOWN-DATAID: T + R + B + A WRITE-ACK(30.0); "
                "the boiler's B sets the write verdict (replyToThermostat; override_reply C2)",
          OTD_MODE_GATEWAY, {}, {},
          { otd('T', otFrame(kWriteData, 1, c30)), otd('R', otFrame(kWriteData, 1, c40)),
            otd('B', otFrame(kUnknownId, 1, c40)), otd('A', otFrame(kWriteAck, 1, c30)) }, 1, false, true);
  runCase("G7", "gateway mode, RM=26, earlier verdict on READ(26): T + B READ-ACK(48.0) + A READ-ACK(50.0); "
                "the boiler's B retracts it (replyToThermostat; override_reply C17)",
          OTD_MODE_GATEWAY, { 26 }, {},
          { otd('T', otFrame(kReadData, 26, 0)), otd('B', otFrame(kReadAck, 26, c48)),
            otd('A', otFrame(kReadAck, 26, c50)) }, 26, false, false);
  runCase("S1", "gateway mode, SR=26, earlier verdict on READ(26): T + A READ-ACK(50.0) keeps it "
                "(loopOTDirect SR table; override_reply C13b)",
          OTD_MODE_GATEWAY, { 26 }, {},
          { otd('T', otFrame(kReadData, 26, 0)), otd('A', otFrame(kReadAck, 26, c50)) }, 26, true, false);
  runCase("P1", "PIC, proxy answer without a B (ADR-103), READ(70): T + A UNKNOWN-DATAID sets the read verdict",
          OTD_MODE_GATEWAY, {}, {},
          { pic('T', otFrame(kReadData, 70, 0)), pic('A', otFrame(kUnknownId, 70, 0)) }, 70, true, false);
  runCase("P2", "PIC, boiler READ(40): T + B UNKNOWN-DATAID sets the read verdict",
          OTD_MODE_GATEWAY, {}, {},
          { pic('T', otFrame(kReadData, 40, 0)), pic('B', otFrame(kUnknownId, 40, 0)) }, 40, true, false);
  runCase("P3", "PIC, earlier verdict on READ(25): T + B READ-ACK retracts it",
          OTD_MODE_GATEWAY, { 25 }, {},
          { pic('T', otFrame(kReadData, 25, 0)), pic('B', otFrame(kReadAck, 25, 0x2A80)) }, 25, false, false);
  runCase("P4", "PIC, earlier verdict on READ(57): a proxy T + A READ-ACK(80.0) does not retract it (TASK-1084)",
          OTD_MODE_GATEWAY, { 57 }, {},
          { pic('T', otFrame(kReadData, 57, 0)), pic('A', otFrame(kReadAck, 57, c80)) }, 57, true, false);
  runCase("P5", "PIC, READ(26): T + B UNKNOWN-DATAID + A READ-ACK (answer override): the B sets the read verdict",
          OTD_MODE_GATEWAY, {}, {},
          { pic('T', otFrame(kReadData, 26, 0)), pic('B', otFrame(kUnknownId, 26, 0)),
            pic('A', otFrame(kReadAck, 26, c50)) }, 26, true, false);
  runCase("P6", "PIC, earlier verdict on WRITE(24): T + B WRITE-ACK retracts it",
          OTD_MODE_GATEWAY, {}, { 24 },
          { pic('T', otFrame(kWriteData, 24, 0x1433)), pic('B', otFrame(kWriteAck, 24, 0x1433)) }, 24, false, false);
  runCase("P7", "PIC, earlier verdict on WRITE(24): a proxy T + A WRITE-ACK, the frames of P6 with an A for the B, "
                "does not retract it (TASK-1084)",
          OTD_MODE_GATEWAY, {}, { 24 },
          { pic('T', otFrame(kWriteData, 24, 0x1433)), pic('A', otFrame(kWriteAck, 24, 0x1433)) }, 24, false, true);

  if (g_dump) std::fclose(g_dump);
  std::printf("%s (%d failure(s))\n", g_failures ? "FAIL" : "PASS", g_failures);
  return g_failures ? 1 : 0;
}
