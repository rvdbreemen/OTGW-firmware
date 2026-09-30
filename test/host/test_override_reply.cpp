/*
***************************************************************************
**  Program  : test/host/test_override_reply.cpp
**
**  Host harness for the OT-Direct reply to a forwarded thermostat frame
**  (TASK-1178). The contract: a thermostat frame that goes to the boiler is
**  answered once, after the boiler's reply, also when an override changed
**  its data and also when the boiler answered DATA-INVALID. A boiler timeout
**  or a corrupt reply gets no answer. The UI/SR tables, monitor mode and the
**  reply to the gateway's own requests keep their behaviour. Each case also
**  checks the state the exchange leaves: state.otBus.bOnline and, where the
**  case names it, every valid otBoilerCache[] entry (a DATA-INVALID, a timeout
**  or a corrupt reply caches nothing).
**
**  The code under test is NOT copied here. build_and_run_override_reply.ps1
**  slices it by anchor, verbatim, into generated/override_reply/<old|fix>/:
**    lib_enums.inc    OpenThermResponseStatus, OpenThermMessageType and
**                     OpenThermMessageID                    (OpenTherm.h)
**    lib_statics.inc  OpenTherm::parity, getMessageType, buildRequest,
**                     isValidResponse and isValidRequest    (OpenTherm.cpp)
**    otd_types.inc    OTDirectRequestOrigin, OTDirectMode   (OTDirecttypes.h)
**    otd_slice.inc    OTDirect.ino: mode, override, response and UI tables,
**                     setOTParityBit, applyOverrides, handleSlaveRequest,
**                     everything from otLoopbackData[] through
**                     handleMasterResponse(), loopOTDirect(), buildOTResponse()
**                     and applyResponseModifiers()
**  The runner compiles this same file once per revision, with that
**  revision's directory on the include path.
**
**  Test doubles, for hardware and I/O only:
**    FakeOpenTherm        otMaster/otSlave. A boiler frame's status comes from
**                         the sliced OpenTherm::isValidResponse() and a
**                         thermostat frame's from isValidRequest(), as the
**                         library's process() computes them (OpenTherm.cpp:444).
**                         busy makes isReady() false: a master that cannot
**                         send yet (C21), a slave that refuses sendResponse() (C28).
**    bridgeFrameToParser  records the T/R/B/A log.
**    stubs                SAT, flame ratio, websocket, debug output, and the
**                         functions loopOTDirect() calls only in master mode or
**                         from its timer blocks. DUE() is false, so the
**                         scheduler and the 1 s and 30 s blocks never run.
**    stand-ins            otSchedule[] (read only after a third UNKNOWN_DATA_ID
**                         for one MsgID, which no case reaches) and
**                         onThermostatMsgID16() (records its calls; the TT/TC
**                         honour logic is not under test).
**  Not modelled: the library's 50 ms RESPONSE_TIME gate, the ISRs and wire
**  timing. The harness proves the reply logic, not delivery on the bus.
**
**  Usage: test_override_reply.exe [dump-dir]
**    With a dump-dir it writes <dump-dir>/cases.txt, one line per case, so
**    two revisions can be compared byte for byte.
**  Exit 0 when every case holds, 1 otherwise.
**
**  Run: powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_override_reply.ps1 -OldVsFix
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <initializer_list>

// ---- platform surface the slices need ---------------------------------------
typedef uint8_t byte;
#define PROGMEM
#define PSTR(s) (s)
class __FlashStringHelper;                        // incomplete: pointer-only, as on Arduino
#define F(s) ((const __FlashStringHelper*)(s))
#define snprintf_P snprintf
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#define HAS_SAT 1

// Debug output goes to telnet on the device; the harness drops it.
#define OTDDebugTf(...)  ((void)0)
#define OTDDebugTln(...) ((void)0)
#define OTDDebugf(...)   ((void)0)
#define DebugTf(...)     ((void)0)
#define DebugTln(...)    ((void)0)

// safeTimers.h: DUE() stays false, so no timer block in loopOTDirect() runs.
#define DECLARE_TIMER_MS(name, ...)  static int name = 0
#define DECLARE_TIMER_SEC(name, ...) static int name = 0
#define DUE(name) ((void)(name), false)

static unsigned long g_millis = 1000000UL;
static unsigned long millis() { return g_millis; }

// ---- OpenTherm library: enums and static helpers, sliced ----------------------
#include "lib_enums.inc"
class OpenTherm {        // declares only the statics that lib_statics.inc defines
public:
  static bool parity(unsigned long frame);
  static OpenThermMessageType getMessageType(unsigned long message);
  static unsigned long buildRequest(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  static bool isValidRequest(unsigned long request);
  static bool isValidResponse(unsigned long response);
};
#include "lib_statics.inc"
#include "otd_types.inc"

// ---- test doubles -------------------------------------------------------------
struct LogEntry { char prefix; unsigned long frame; };
static std::vector<LogEntry> g_log;               // every bridgeFrameToParser() call
static void bridgeFrameToParser(char prefix, unsigned long frame) { g_log.push_back({ prefix, frame }); }

// Stand-in for one OpenTherm library instance (the GPIO/ISR driver).
class FakeOpenTherm {
public:
  std::vector<unsigned long> sent;   // master: sendRequestAsync() frames; slave: sendResponse() frames
  bool busy = false;                 // bus busy: isReady() is false while set

  void reset() {
    sent.clear(); busy = false; waiting = false; answered = false;
    response = 0; status = OpenThermResponseStatus::NONE;
  }
  void process() {}
  bool isReady() { return !busy && (!waiting || answered); }
  bool sendRequestAsync(unsigned long request) {
    if (!isReady()) return false;
    sent.push_back(request);
    waiting = true; answered = false;
    response = 0; status = OpenThermResponseStatus::NONE;   // as OpenTherm::sendRequestAsync()
    return true;
  }
  bool sendResponse(unsigned long frame) {
    if (!isReady()) return false;
    sent.push_back(frame);
    return true;
  }
  unsigned long getLastResponse() { return response; }
  OpenThermResponseStatus getLastResponseStatus() { return status; }

  // The boiler side, reported the way OpenTherm::process() reports it.
  void boilerFrame(unsigned long frame) {      // a complete frame with its stop bit (DATA_READY)
    response = frame;
    status = OpenTherm::isValidResponse(frame) ? OpenThermResponseStatus::SUCCESS
                                               : OpenThermResponseStatus::INVALID;
    answered = true;
  }
  void boilerRxError(unsigned long bits) {     // rx DATA_INVALID: the bits received so far
    response = bits;
    status = OpenThermResponseStatus::INVALID;
    answered = true;
  }
  void boilerTimeout() {                       // no reply within 1 s; response stays 0
    status = OpenThermResponseStatus::TIMEOUT;
    answered = true;
  }

private:
  bool waiting = false;
  bool answered = false;
  unsigned long response = 0;
  OpenThermResponseStatus status = OpenThermResponseStatus::NONE;
};
static FakeOpenTherm otMaster;
static FakeOpenTherm otSlave;

// state / settings: only the fields the slices touch.
static struct { struct { bool bOnline; } otBus; struct { bool bThermostatConnected; } otd; } state;
static struct {
  struct {
    uint8_t iCHMode; float fFlowTemp; float fFlowMax; float fRoomSetpoint;   // types as in OTDirecttypes.h
    float fGradient; float fExponent; float fOffset; bool bRoomCompEnabled;
  } otd;
} settings;

static void sendWebSocketJSON(const char*) {}
static bool satSimulationBlocksBusTx(const char*, const __FlashStringHelper*) { return false; }
static void satNotifyBoilerFrameSeen() {}
static bool satOwnsControlSetpoint() { return false; }
static std::string g_flame;                       // every flameRatioSet() call: '1' flame on, '0' off
static void flameRatioSet(bool on) { g_flame += on ? '1' : '0'; }
static void updateWriteCache(uint8_t, uint16_t) {}   // the scheduler's write cache, fed by master mode

static int      g_m16Calls = 0;   // onThermostatMsgID16() stand-in: the TT/TC observer's input
static uint8_t  g_m16Type  = 0;
static uint16_t g_m16Data  = 0;
static void onThermostatMsgID16(uint8_t msgType, uint16_t data) { g_m16Calls++; g_m16Type = msgType; g_m16Data = data; }

// handleMasterModeSlaveFrame() is sliced after loopOTDirect(), which calls it.
// The firmware gets this prototype from the Arduino .ino preprocessor.
static void handleMasterModeSlaveFrame(unsigned long frame);

// Called by loopOTDirect() only from its timer blocks.
static void scheduleMasterRequest() {}
static void emitSummaryLine() {}
static void updateOTDirectStatus() {}
static void checkThermostatTimeout() {}
static void loopFlameRatio() {}
static void clearWriteOverride(uint8_t) {}
static void loopPiCtrl() {}
static bool enqueueWriteCommand(uint8_t, uint16_t, const char*) { return false; }
static void loopCHHysteresis() {}
static uint32_t otCSLastCommandMs = 0;
static uint32_t otC2LastCommandMs = 0;
static uint32_t otSCLastCommandMs = 0;
static constexpr uint32_t OT_CSC2_EXPIRY_MS = 60000;
static constexpr uint32_t OT_SC_EXPIRY_MS   = 61000;

// Stand-in for the schedule table: handleMasterResponse() reads it only after a
// third UNKNOWN_DATA_ID for one MsgID, and the per-case reset keeps every count below 3.
struct OTScheduleEntry { uint8_t msgId; bool disabled; };
static OTScheduleEntry otSchedule[] = { { 0, false } };
static constexpr uint8_t OT_SCHEDULE_SIZE = 1;

// ---- code under test, sliced by anchor from OTDirect.ino ------------------------
#include "otd_slice.inc"

#ifndef OTD_SLICE_ORIGIN
#define OTD_SLICE_ORIGIN "(unknown)"
#endif

// ---- harness --------------------------------------------------------------------
enum : unsigned { kReadData = 0, kWriteData = 1, kReadAck = 4, kWriteAck = 5, kDataInvalid = 6, kUnknownId = 7 };

// Parity by counting bits, independent of setOTParityBit().
static unsigned long withEvenParity(unsigned long f) {
  f &= 0x7FFFFFFFUL;
  int n = 0;
  for (int b = 0; b < 31; b++) n += (int)((f >> b) & 1UL);
  return (n & 1) ? (f | 0x80000000UL) : f;
}
static unsigned long otFrame(unsigned type, unsigned id, uint16_t data) {
  return withEvenParity(((unsigned long)type << 28) | ((unsigned long)id << 16) | data);
}
static uint16_t degC(double c) { return (uint16_t)(int16_t)(c * 256.0); }

static std::string hex8(unsigned long v) {
  char b[16];
  snprintf(b, sizeof(b), "%08lX", v);
  return b;
}
static std::string framesText(const std::vector<unsigned long>& v) {
  if (v.empty()) return "-";
  std::string s;
  for (size_t i = 0; i < v.size(); i++) { if (i) s += ','; s += hex8(v[i]); }
  return s;
}
static std::string logText(const std::vector<LogEntry>& v) {
  if (v.empty()) return "-";
  std::string s;
  for (size_t i = 0; i < v.size(); i++) { if (i) s += ','; s += v[i].prefix; s += ':'; s += hex8(v[i].frame); }
  return s;
}
static std::string logOf(std::initializer_list<LogEntry> e) { return logText(std::vector<LogEntry>(e)); }

// Every valid otBoilerCache[] entry as "id:data" in hex, "-" when there is none.
static std::string cacheText() {
  std::string s;
  for (size_t i = 0; i < sizeof(otBoilerCacheValid) / sizeof(otBoilerCacheValid[0]); i++) {
    if (!otBoilerCacheValid[i]) continue;
    char b[16];
    snprintf(b, sizeof(b), "%s%02X:%04X", s.empty() ? "" : ",", (unsigned)i, (unsigned)otBoilerCache[i]);
    s += b;
  }
  return s.empty() ? "-" : s;
}

// Optional expectations of the TASK-1177 cases, checked by verdict() and reset by resetAll().
static const char* g_wantFlame   = nullptr;   // expected flameRatioSet() calls, "-" for none
static int         g_wantPresent = -1;        // expected otDirectBoilerPresent(); -1: not checked
static int         g_wantVent    = -1;        // expected otIsVentSlave(); -1: not checked
static bool        g_checkFlow   = false;     // TASK-1184: check getFlowTemp() against g_wantFlow
static float       g_wantFlow    = 0.0f;

static void resetAll() {
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) otOverrides[i].active = false;
  for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) otResponseOverrides[i].active = false;
  for (uint8_t i = 0; i < OT_RESPONSE_MODIFY_MAX; i++) otResponseModifiers[i].active = false;
  otUnknownIdCount = 0;
  memset(otUnknownCounters, 0, sizeof(otUnknownCounters));
  memset(otBoilerCache, 0, sizeof(otBoilerCache));
  memset(otBoilerCacheValid, 0, sizeof(otBoilerCacheValid));
  otSlaveFramePending   = false;
  otSlaveFrame          = 0;
  otMasterRequestActive = false;
  otLastSentRequest     = 0;
  otLastRequestOrigin   = OT_DIRECT_ORIGIN_GATEWAY;
  otCurrentMode         = OTD_MODE_GATEWAY;
  otDHWPushState        = PUSH_IDLE;
  otSummaryMode         = false;
  otSummaryPending      = false;
  state.otBus.bOnline   = true;
  otMaster.reset();
  otSlave.reset();
  g_log.clear();
  g_m16Calls = 0; g_m16Type = 0; g_m16Data = 0;
  g_flame.clear();
  g_wantFlame = nullptr; g_wantPresent = -1; g_wantVent = -1; g_checkFlow = false;
  memset(&settings, 0, sizeof(settings));
}

// A thermostat frame as the library's slave process() delivers it (OpenTherm.cpp:444-447).
static void thermostatSends(unsigned long f) {
  handleSlaveRequest(f, OpenTherm::isValidRequest(f) ? OpenThermResponseStatus::SUCCESS
                                                     : OpenThermResponseStatus::INVALID);
}

struct CaseResult { std::string id; bool pass; size_t wantReplies; size_t gotReplies; bool repliesOk; bool logOk; bool boilerOk; bool stateOk; };
static std::vector<CaseResult> g_results;
static FILE* g_dump = nullptr;

// Compare what the code under test did with the contract and report the case.
// wantCache is the expected cacheText() (nullptr: not checked); wantOnline the
// expected state.otBus.bOnline after the exchange.
static void verdict(const char* id, const char* title,
                    const std::vector<unsigned long>& wantBoiler,
                    const std::vector<unsigned long>& wantReplies,
                    const std::string& wantLog,
                    const char* wantCache = nullptr,
                    bool wantOnline = true) {
  const std::string gotBoiler  = framesText(otMaster.sent);
  const std::string gotReplies = framesText(otSlave.sent);
  const std::string gotLog     = logText(g_log);
  const std::string gotCache   = cacheText();
  const bool gotOnline = state.otBus.bOnline;
  const std::string gotFlame = g_flame.empty() ? "-" : g_flame;
  const int  gotPresent = otDirectBoilerPresent() ? 1 : 0;
  const int  gotVent    = otIsVentSlave() ? 1 : 0;
  const float gotFlow   = getFlowTemp();
  const bool extraOk   = (g_wantFlame == nullptr || gotFlame == g_wantFlame) &&
                         (g_wantPresent < 0 || gotPresent == g_wantPresent) &&
                         (g_wantVent < 0 || gotVent == g_wantVent) &&
                         (!g_checkFlow || fabsf(gotFlow - g_wantFlow) < 0.01f);
  const bool boilerOk  = gotBoiler  == framesText(wantBoiler);
  const bool repliesOk = gotReplies == framesText(wantReplies);
  const bool logOk     = gotLog     == wantLog;
  const bool stateOk   = (wantCache == nullptr || gotCache == wantCache) && gotOnline == wantOnline && extraOk;
  const bool pass      = boilerOk && repliesOk && logOk && stateOk;

  g_results.push_back({ id, pass, wantReplies.size(), otSlave.sent.size(), repliesOk, logOk, boilerOk, stateOk });
  printf("CASE %s %s want_replies=%u got_replies=%u replies_ok=%c log_ok=%c boiler_ok=%c state_ok=%c\n",
         id, pass ? "pass" : "FAIL", (unsigned)wantReplies.size(), (unsigned)otSlave.sent.size(),
         repliesOk ? 'y' : 'n', logOk ? 'y' : 'n', boilerOk ? 'y' : 'n', stateOk ? 'y' : 'n');
  printf("     %s\n", title);
  printf("     to boiler : %s%s\n", gotBoiler.c_str(), boilerOk ? "" : ("   WANT " + framesText(wantBoiler)).c_str());
  printf("     replies   : %s%s\n", gotReplies.c_str(), repliesOk ? "" : ("   WANT " + framesText(wantReplies)).c_str());
  printf("     log       : %s\n", gotLog.c_str());
  if (!logOk) printf("     WANT log  : %s\n", wantLog.c_str());
  printf("     state     : cache=%s online=%d%s\n", gotCache.c_str(), gotOnline ? 1 : 0,
         stateOk ? "" : ("   WANT cache=" + std::string(wantCache ? wantCache : "(not checked)") +
                         " online=" + (wantOnline ? "1" : "0")).c_str());
  if (g_wantFlame || g_wantPresent >= 0 || g_wantVent >= 0 || g_checkFlow) {
    char flowText[16];
    snprintf(flowText, sizeof(flowText), "%.2f", g_wantFlow);
    printf("     extra     : flame=%s present=%d vent=%d flow=%.2f%s\n", gotFlame.c_str(), gotPresent, gotVent, gotFlow,
           extraOk ? "" : ("   WANT flame=" + std::string(g_wantFlame ? g_wantFlame : "(any)") +
                           " present=" + (g_wantPresent < 0 ? "(any)" : std::to_string(g_wantPresent)) +
                           " vent=" + (g_wantVent < 0 ? "(any)" : std::to_string(g_wantVent)) +
                           " flow=" + (g_checkFlow ? std::string(flowText) : "(any)")).c_str());
  }
  if (g_dump) {
    fprintf(g_dump, "%s boiler=%s replies=%s log=%s cache=%s online=%d m16=%d:%u:%04X flame=%s present=%d vent=%d flow=%.2f\n",
            id, gotBoiler.c_str(), gotReplies.c_str(), gotLog.c_str(), gotCache.c_str(), gotOnline ? 1 : 0,
            g_m16Calls, (unsigned)g_m16Type, (unsigned)g_m16Data, gotFlame.c_str(), gotPresent, gotVent, gotFlow);
  }
}

// One overridden WRITE-DATA exchange in gateway mode: the thermostat writes `own`,
// the override substitutes `ovr`, the boiler answers `boilerReply`.
struct OverrideRun { unsigned long tstat; unsigned long toBoiler; };
static OverrideRun runOverriddenWrite(unsigned id, uint16_t own, uint16_t ovr) {
  resetAll();
  setOverride((uint8_t)id, ovr);
  const unsigned long tstat = otFrame(kWriteData, id, own);
  thermostatSends(tstat);
  loopOTDirect();                         // forwards the frame to the boiler
  return { tstat, otFrame(kWriteData, id, ovr) };
}

// ---- TASK-1177 suite: replies for OEM data-ids 128-255 and the boiler cache ---------
// otBoilerCache holds data-ids 0-127. Masking a reply's data-id with 0x7F put a
// MsgID 128+n reply in MsgID n's slot. K1-K4 pin that it no longer does; K5-K7
// are controls that must behave the same with and without the fix.
static void clearTraffic() { otMaster.reset(); otSlave.reset(); g_log.clear(); g_flame.clear(); }

static void suite1177() {
  const uint16_t cfgVent = 0xC012;   // Slave Config HB bits 6-7 = 11: ventilation/HRV
  {
    resetAll();
    const unsigned long t = otFrame(kReadData, 131, 0), b = otFrame(kReadAck, 131, cfgVent);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    g_wantPresent = 0; g_wantVent = 0;
    verdict("K1", "gateway mode, READ(131) answered READ-ACK(131, C012): relayed, not cached; MsgID 3's slot, otDirectBoilerPresent() and otIsVentSlave() untouched",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "-");
  }
  {
    resetAll();
    const unsigned long q = otFrame(kReadData, 128, 0), b = otFrame(kReadAck, 128, 0x0008);
    sendMasterRequestAsync(q, OT_DIRECT_ORIGIN_GATEWAY);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    g_wantFlame = "-";
    verdict("K2", "gateway command READ(128) answered with bit 3 set: not cached, flameRatioSet() not called",
            { q }, {}, logOf({ { 'R', q }, { 'B', b } }), "-");
  }
  {
    resetAll();
    otCurrentMode = OTD_MODE_LOOPBACK;
    const unsigned long t = otFrame(kReadData, 131, 0), b = otFrame(kUnknownId, 131, 0);
    thermostatSends(t); loopOTDirect(); loopOTDirect();
    verdict("K3", "loopback mode, READ(131): the simulated UNKNOWN-DATAID is relayed and not cached in MsgID 3's slot",
            {}, { b }, logOf({ { 'T', t }, { 'B', b } }), "-");
  }
  {
    resetAll();
    const unsigned long t3 = otFrame(kReadData, 3, 0), b3 = otFrame(kReadAck, 3, 0x1234);
    thermostatSends(t3); loopOTDirect();
    otMaster.boilerFrame(b3); loopOTDirect(); loopOTDirect();   // a genuine MsgID 3 reply in the cache
    clearTraffic();
    otCurrentMode = OTD_MODE_MASTER;
    const unsigned long t = otFrame(kReadData, 131, 0), a = otFrame(kUnknownId, 131, 0);
    thermostatSends(t); loopOTDirect();
    verdict("K4", "master mode with MsgID 3 cached, READ(131): answered UNKNOWN-DATAID, not READ-ACK with MsgID 3's data",
            {}, { a }, logOf({ { 'T', t }, { 'A', a } }), "03:1234");
  }
  {
    resetAll();
    const unsigned long t = otFrame(kReadData, 3, 0), b = otFrame(kReadAck, 3, cfgVent);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    g_wantPresent = 1; g_wantVent = 1;
    verdict("K5", "control: a genuine READ-ACK(3, C012) is cached and drives otDirectBoilerPresent() and otIsVentSlave()",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "03:C012");
  }
  {
    resetAll();
    const unsigned long q = otFrame(kReadData, 0, 0x0300), b = otFrame(kReadAck, 0, 0x0308);
    sendMasterRequestAsync(q, OT_DIRECT_ORIGIN_GATEWAY);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    g_wantFlame = "1";
    verdict("K6", "control: a genuine READ-ACK(0) with the flame bit is cached and calls flameRatioSet(true)",
            { q }, {}, logOf({ { 'R', q }, { 'B', b } }), "00:0308");
  }
  {
    resetAll();
    const unsigned long t3 = otFrame(kReadData, 3, 0), b3 = otFrame(kReadAck, 3, 0x1234);
    thermostatSends(t3); loopOTDirect();
    otMaster.boilerFrame(b3); loopOTDirect(); loopOTDirect();
    clearTraffic();
    otCurrentMode = OTD_MODE_MASTER;
    const unsigned long t = otFrame(kReadData, 3, 0), a = otFrame(kReadAck, 3, 0x1234);
    thermostatSends(t); loopOTDirect();
    verdict("K7", "control: master mode with MsgID 3 cached, READ(3): answered READ-ACK(3, 1234) from the cache",
            {}, { a }, logOf({ { 'T', t }, { 'A', a } }), "03:1234");
  }
}

// ---- TASK-1184 suite: a boiler UNKNOWN-DATA-ID reply and the boiler cache --------
// The library reports UNKNOWN-DATA-ID as SUCCESS, and the reply's data bytes
// (0x0000) used to be cached as a valid value: the AUTO heating curve then ran
// at 0.0 C outside and master mode answered READ-ACK(0). U1-U4 pin the fix;
// U5 and U6 are controls that must behave the same with and without it.
static void curveSettings() {                   // AUTO heating curve, the OTDirecttypes.h defaults
  settings.otd.iCHMode = 2; settings.otd.fFlowTemp = 45.0f; settings.otd.fFlowMax = 75.0f;
  settings.otd.fRoomSetpoint = 20.0f; settings.otd.fGradient = 1.5f; settings.otd.fExponent = 1.0f;
  settings.otd.fOffset = 0.0f; settings.otd.bRoomCompEnabled = false;
}
static void gatewayExchange(unsigned long q, unsigned long b) {   // a scheduler request and its reply
  sendMasterRequestAsync(q, OT_DIRECT_ORIGIN_GATEWAY);
  otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
}

static void suite1184() {
  const unsigned long q27 = otFrame(kReadData, 27, 0), u27 = otFrame(kUnknownId, 27, 0);
  const unsigned long a27 = otFrame(kReadAck, 27, degC(5.0));
  {
    resetAll(); curveSettings(); otCurrentMode = OTD_MODE_MASTER;
    gatewayExchange(q27, u27);
    g_checkFlow = true; g_wantFlow = 45.0f;     // fixed flow: no outside temperature
    verdict("U1", "master mode, AUTO curve, READ(27) answered UNKNOWN-DATAID: nothing cached, getFlowTemp() falls back to the fixed 45.0",
            { q27 }, {}, logOf({ { 'R', q27 }, { 'B', u27 } }), "-");
  }
  {
    resetAll(); curveSettings(); otCurrentMode = OTD_MODE_MASTER;
    gatewayExchange(q27, u27);
    clearTraffic();
    const unsigned long t = otFrame(kReadData, 27, 0), a = otFrame(kUnknownId, 27, 0);
    thermostatSends(t); loopOTDirect();
    verdict("U2", "master mode after the boiler answered MsgID 27 UNKNOWN-DATAID, thermostat READ(27): answered UNKNOWN-DATAID, not READ-ACK(0)",
            {}, { a }, logOf({ { 'T', t }, { 'A', a } }), "-");
  }
  {
    resetAll(); curveSettings(); otCurrentMode = OTD_MODE_MASTER;
    gatewayExchange(q27, a27);                  // 5.0 C cached
    gatewayExchange(q27, u27);                  // then the boiler stops supporting MsgID 27
    g_checkFlow = true; g_wantFlow = 45.0f;
    verdict("U3", "master mode, AUTO curve, READ-ACK(27, 5.0) then UNKNOWN-DATAID: the slot is cleared, getFlowTemp() uses the fixed 45.0",
            { q27, q27 }, {}, logOf({ { 'R', q27 }, { 'B', a27 }, { 'R', q27 }, { 'B', u27 } }), "-");
  }
  {
    resetAll();
    const unsigned long q = otFrame(kReadData, 0, 0x0300), b = otFrame(kUnknownId, 0, 0);
    gatewayExchange(q, b);
    g_wantFlame = "-";
    verdict("U4", "READ(0) answered UNKNOWN-DATAID: nothing cached, flameRatioSet() not called",
            { q }, {}, logOf({ { 'R', q }, { 'B', b } }), "-");
  }
  {
    resetAll(); curveSettings(); otCurrentMode = OTD_MODE_MASTER;
    gatewayExchange(q27, a27);
    g_checkFlow = true; g_wantFlow = 42.5f;     // 20 + 1.5 * (20 - 5)
    verdict("U5", "control: READ-ACK(27, 5.0) is cached and drives the AUTO curve to 42.5",
            { q27 }, {}, logOf({ { 'R', q27 }, { 'B', a27 } }), "1B:0500");
  }
  {
    resetAll();
    const unsigned long q = otFrame(kWriteData, 1, degC(40.0)), b = otFrame(kWriteAck, 1, degC(40.0));
    gatewayExchange(q, b);
    verdict("U6", "control: a WRITE-ACK(1, 40.0) reply is cached as before",
            { q }, {}, logOf({ { 'R', q }, { 'B', b } }), "01:2800");
  }
}

// Close the dump and print the summary line the runner parses.
static int finish() {
  if (g_dump) fclose(g_dump);

  unsigned passed = 0;
  std::string failed, zeroReply;
  for (const CaseResult& c : g_results) {
    if (c.pass) passed++;
    else { if (!failed.empty()) failed += ","; failed += c.id; }
    if (c.wantReplies > 0 && c.gotReplies == 0) { if (!zeroReply.empty()) zeroReply += ","; zeroReply += c.id; }
  }
  printf("\nSUMMARY cases=%u passed=%u failed=%s zero_reply=%s\n", (unsigned)g_results.size(), passed,
         failed.empty() ? "none" : failed.c_str(), zeroReply.empty() ? "none" : zeroReply.c_str());
  const bool ok = passed == g_results.size();
  printf("%s\n", ok ? "harness: contract holds" : "harness: contract VIOLATED");
  return ok ? 0 : 1;
}

// argv[1]: directory for the case dump (cases-<suite>.txt); argv[2]: suite, 1178 (default), 1177 or 1184.
int main(int argc, char** argv) {
  const std::string suite = argc > 2 ? argv[2] : "1178";
  if (suite != "1178" && suite != "1177" && suite != "1184") { printf("unknown suite %s\n", suite.c_str()); return 3; }
  printf("%s\n", suite == "1177" ? "== OT-Direct boiler cache and OEM data-ids 128-255 (TASK-1177) =="
               : suite == "1184" ? "== OT-Direct boiler cache and UNKNOWN-DATA-ID replies (TASK-1184) =="
                                 : "== OT-Direct reply to a forwarded thermostat frame (TASK-1178) ==");
  printf("slice origin: %s\n\n", OTD_SLICE_ORIGIN);
  if (argc > 1) {
    std::string path = std::string(argv[1]) + "/cases-" + suite + ".txt";
    g_dump = fopen(path.c_str(), "wb");
    if (!g_dump) { printf("cannot open %s\n", path.c_str()); return 3; }
  }
  if (suite == "1177") { suite1177(); return finish(); }
  if (suite == "1184") { suite1184(); return finish(); }

  const uint16_t t30 = degC(30.0), t31 = degC(31.0), t40 = degC(40.0);

  // ---- C1-C5: CS= override on MsgID 1, every kind of boiler reply ----------------
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    const unsigned long b = otFrame(kWriteAck, 1, t40), a = otFrame(kWriteAck, 1, t30);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C1", "WRITE(1, 30.0) with CS=40 active, boiler WRITE-ACK(40.0): thermostat gets WRITE-ACK(1, own 30.0); the cache keeps the boiler's 40.0",
            { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "01:2800");
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    const unsigned long b = otFrame(kUnknownId, 1, 0), a = otFrame(kWriteAck, 1, t30);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C2", "as C1, boiler UNKNOWN-DATAID: thermostat gets WRITE-ACK(1, own 30.0), nothing cached",
            { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "-");
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    const unsigned long b = otFrame(kDataInvalid, 1, t40), a = otFrame(kWriteAck, 1, t30);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C3", "as C1, boiler DATA-INVALID (library status INVALID, good parity, same ID): WRITE-ACK(1, own 30.0), nothing cached",
            { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "-");
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    otMaster.boilerTimeout(); loopOTDirect(); loopOTDirect();
    verdict("C4", "as C1, boiler TIMEOUT: no reply (the thermostat retries), nothing cached",
            { r.toBoiler }, {}, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler } }), "-");
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    otMaster.boilerFrame(otFrame(kWriteAck, 1, t40) ^ 0x80000000UL);   // parity error
    loopOTDirect(); loopOTDirect();
    verdict("C5", "as C1, boiler frame with a parity error (status INVALID): no reply, nothing cached",
            { r.toBoiler }, {}, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler } }), "-");
  }

  // ---- C6: WRITE overrides on MsgID 7 (CC), 8 (C2), 14 (MM), 16 (TT/TC/BS) --------
  {
    struct { const char* id; unsigned msg; double own; double ovr; } c6[] = {
      { "C6a",  7,  0.0, 30.0 },
      { "C6b",  8, 45.0, 55.0 },
      { "C6c", 14, 100.0, 50.0 },
      { "C6d", 16, 20.5, 21.5 },
    };
    for (auto& c : c6) {
      OverrideRun r = runOverriddenWrite(c.msg, degC(c.own), degC(c.ovr));
      const unsigned long b = otFrame(kWriteAck, c.msg, degC(c.ovr)), a = otFrame(kWriteAck, c.msg, degC(c.own));
      otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
      char title[160];
      snprintf(title, sizeof(title), "WRITE(%u, %.1f) with override %.1f, boiler WRITE-ACK: WRITE-ACK(%u, own %.1f)%s",
               c.msg, c.own, c.ovr, c.msg, c.own, c.msg == 16 ? "; TT/TC observer sees the own value" : "");
      verdict(c.id, title, { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }));
      if (c.msg == 16 && !(g_m16Calls == 1 && g_m16Type == kWriteData && g_m16Data == degC(c.own))) {
        printf("     NOTE onThermostatMsgID16 saw %d call(s), type %u, data %04X\n", g_m16Calls, g_m16Type, g_m16Data);
      }
    }
  }

  // ---- C7: SW= / SH= on MsgID 56 / 57, the firmware's three-case rule --------------
  // One rule for every frame, whatever its parity bit. C7a-C7f send thermostat
  // frames with parity bit 1 (0x90383200, 0x90395000), C7g and C7h frames with
  // parity bit 0. The PIC gives the same answers for C7b, C7e, C7g and C7h. For
  // the parity-1 frames of C7a, C7c, C7d and C7f its setbyte1 (gateway.asm:2672-2677)
  // marks the frame rewritten, and it answers WRITE-ACK(SW/SH) or WRITE-ACK with
  // the MsgID 48/49 upper bound instead (TASK-1178 open question (b)).
  {
    if (!(otFrame(kWriteData, 56, degC(50.0)) >> 31) || !(otFrame(kWriteData, 57, degC(80.0)) >> 31) ||
        (otFrame(kWriteData, 56, degC(51.0)) >> 31) || (otFrame(kWriteData, 57, degC(81.0)) >> 31)) {
      printf("HARNESS ERROR: a C7 thermostat frame does not carry the parity bit the comment names\n");
      return 3;
    }
    char title[220];
    struct { const char* idAck; const char* idUnk; const char* idInv; unsigned msg; double own; double ovr; double clamp; } c7[] = {
      { "C7a", "C7b", "C7c", 56, 50.0, 60.0, 55.0 },
      { "C7d", "C7e", "C7f", 57, 80.0, 70.0, 65.0 },
    };
    for (auto& c : c7) {
      {   // boiler WRITE-ACK with a clamped echo: the thermostat gets that echo
        OverrideRun r = runOverriddenWrite(c.msg, degC(c.own), degC(c.ovr));
        const unsigned long b = otFrame(kWriteAck, c.msg, degC(c.clamp));
        otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
        snprintf(title, sizeof(title), "WRITE(%u, %.1f), parity bit %u, with override %.1f, boiler WRITE-ACK(%.1f): WRITE-ACK with the boiler's echo, no A (equals B)",
                 c.msg, c.own, (unsigned)(r.tstat >> 31), c.ovr, c.clamp);
        verdict(c.idAck, title, { r.toBoiler }, { b }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b } }));
      }
      {   // boiler UNKNOWN-DATAID: the thermostat gets the override value
        OverrideRun r = runOverriddenWrite(c.msg, degC(c.own), degC(c.ovr));
        const unsigned long b = otFrame(kUnknownId, c.msg, 0), a = otFrame(kWriteAck, c.msg, degC(c.ovr));
        otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
        snprintf(title, sizeof(title), "WRITE(%u, %.1f), parity bit %u, with override %.1f, boiler UNKNOWN-DATAID: WRITE-ACK(%u, override %.1f), nothing cached",
                 c.msg, c.own, (unsigned)(r.tstat >> 31), c.ovr, c.msg, c.ovr);
        verdict(c.idUnk, title, { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "-");
      }
      {   // boiler DATA-INVALID: the thermostat gets its own value back
        OverrideRun r = runOverriddenWrite(c.msg, degC(c.own), degC(c.ovr));
        const unsigned long b = otFrame(kDataInvalid, c.msg, degC(c.ovr)), a = otFrame(kWriteAck, c.msg, degC(c.own));
        otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
        snprintf(title, sizeof(title), "WRITE(%u, %.1f), parity bit %u, with override %.1f, boiler DATA-INVALID: WRITE-ACK(%u, own %.1f), nothing cached",
                 c.msg, c.own, (unsigned)(r.tstat >> 31), c.ovr, c.msg, c.own);
        verdict(c.idInv, title, { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "-");
      }
    }
    {   // C7g: as C7a with a parity-0 frame
      OverrideRun r = runOverriddenWrite(56, degC(51.0), degC(60.0));
      const unsigned long b = otFrame(kWriteAck, 56, degC(55.0));
      otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
      snprintf(title, sizeof(title), "WRITE(56, 51.0), parity bit %u, with override 60.0, boiler WRITE-ACK(55.0): as C7a, the boiler's echo, no A (equals B)",
               (unsigned)(r.tstat >> 31));
      verdict("C7g", title, { r.toBoiler }, { b }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b } }));
    }
    {   // C7h: as C7f with a parity-0 frame
      OverrideRun r = runOverriddenWrite(57, degC(81.0), degC(70.0));
      const unsigned long b = otFrame(kDataInvalid, 57, degC(70.0)), a = otFrame(kWriteAck, 57, degC(81.0));
      otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
      snprintf(title, sizeof(title), "WRITE(57, 81.0), parity bit %u, with override 70.0, boiler DATA-INVALID: as C7f, WRITE-ACK(57, own 81.0), nothing cached",
               (unsigned)(r.tstat >> 31));
      verdict("C7h", title, { r.toBoiler }, { a }, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "-");
    }
  }

  // ---- C8: a READ of an overridden MsgID is forwarded unmodified and relayed -------
  {
    resetAll();
    setOverride(56, degC(60.0));                               // SW=60
    const unsigned long t = otFrame(kReadData, 56, 0), b = otFrame(kReadAck, 56, degC(50.0));
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C8a", "READ(56) with SW=60 active: forwarded unmodified (no R), boiler READ-ACK relayed",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }));
  }
  {
    resetAll();
    setOverride(100, 0x0002);                                  // TT flags on MsgID 100
    const unsigned long t = otFrame(kReadData, 100, 0), b = otFrame(kUnknownId, 100, 0);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C8b", "READ(100) with the TT flag override active: forwarded unmodified, boiler reply relayed, nothing cached",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "-");
  }

  // ---- C9-C11: no override; a gateway command ------------------------------------
  {
    resetAll();
    const unsigned long t = otFrame(kWriteData, 24, degC(20.5)), b = otFrame(kWriteAck, 24, degC(20.5));
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C9", "no override, WRITE(24): the boiler's WRITE-ACK is relayed unchanged",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }));
  }
  {
    resetAll();
    const unsigned long t = otFrame(kReadData, 25, 0), b = otFrame(kDataInvalid, 25, 0);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C10", "no override, READ(25), boiler DATA-INVALID: relayed unchanged, nothing cached",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "-");
  }
  {
    resetAll();
    const unsigned long q = otFrame(kWriteData, 14, degC(50.0)), b = otFrame(kWriteAck, 14, degC(50.0));
    sendMasterRequestAsync(q, OT_DIRECT_ORIGIN_GATEWAY);       // as scheduleMasterRequest() sends a queued command
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C11", "the boiler's reply to a queued gateway command: no reply to the thermostat",
            { q }, {}, logOf({ { 'R', q }, { 'B', b } }));
  }

  // ---- C12-C13: monitor mode and the UI/SR tables stay as they are ------------------
  {
    resetAll();
    otCurrentMode = OTD_MODE_MONITOR;
    setOverride(1, t40);
    const unsigned long t = otFrame(kWriteData, 1, t30), b = otFrame(kWriteAck, 1, t30);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C12", "monitor mode with CS=40 active: forwarded unmodified, reply relayed",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }));
  }
  {
    resetAll();
    otUnknownIds[0] = 70; otUnknownIdCount = 1;               // UI=70
    const unsigned long t = otFrame(kReadData, 70, 0), a = otFrame(kUnknownId, 70, 0);
    thermostatSends(t); loopOTDirect(); loopOTDirect();
    verdict("C13a", "UI table hit (UI=70): answered UNKNOWN-DATAID at once, nothing to the boiler",
            {}, { a }, logOf({ { 'T', t }, { 'A', a } }));
  }
  {
    resetAll();
    otResponseOverrides[0] = OTResponseOverride{ 26, true, 0x3200 };   // SR=26:50.0
    const unsigned long t = otFrame(kReadData, 26, 0), a = otFrame(kReadAck, 26, 0x3200);
    thermostatSends(t); loopOTDirect(); loopOTDirect();
    verdict("C13b", "SR table hit (SR=26): answered READ-ACK(26, 50.0) at once, nothing to the boiler",
            {}, { a }, logOf({ { 'T', t }, { 'A', a } }));
  }

  // ---- C14-C16 ----------------------------------------------------------------------
  {
    resetAll();
    setOverride(1, t30);                                       // the thermostat already writes 30.0
    const unsigned long t = otFrame(kWriteData, 1, t30), b = otFrame(kWriteAck, 1, t30);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C14", "override value equal to the thermostat's: plain relay, no R and no A",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }));
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    const unsigned long t2 = otFrame(kWriteData, 1, t31);
    thermostatSends(t2);                                       // arrives while the first exchange is in flight
    const unsigned long b = otFrame(kWriteAck, 1, t40);
    otMaster.boilerFrame(b); loopOTDirect();                   // replies to the first, forwards the second
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    const unsigned long a1 = otFrame(kWriteAck, 1, t30), a2 = otFrame(kWriteAck, 1, t31);
    verdict("C15", "a second thermostat frame arrives before the boiler replies: each reply built from its own frame",
            { r.toBoiler, r.toBoiler }, { a1, a2 },
            logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a1 },
                    { 'T', t2 }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a2 } }));
  }
  {
    resetAll();
    otCurrentMode = OTD_MODE_LOOPBACK;
    setOverride(1, t40);
    const unsigned long t = otFrame(kWriteData, 1, t30), rq = otFrame(kWriteData, 1, t40);
    const unsigned long b = otFrame(kWriteAck, 1, t40), a = otFrame(kWriteAck, 1, t30);
    thermostatSends(t); loopOTDirect(); loopOTDirect();
    verdict("C16", "loopback mode with CS=40 active: simulated WRITE-ACK(40.0), thermostat gets WRITE-ACK(1, own 30.0)",
            {}, { a }, logOf({ { 'T', t }, { 'R', rq }, { 'B', b }, { 'A', a } }));
  }

  // ---- C17-C23: further checks ---------------------------------------------------------
  {
    resetAll();
    otResponseModifiers[0] = OTResponseModify{ 26, true, 0x2800 };    // RM=26:40.0
    const unsigned long t = otFrame(kReadData, 26, 0), b = otFrame(kReadAck, 26, 0x3000), a = otFrame(kReadAck, 26, 0x2800);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C17", "RM=26 response modifier, gateway mode: the thermostat gets the modified READ-ACK, logged as A; the cache keeps the boiler's value",
            { t }, { a }, logOf({ { 'T', t }, { 'B', b }, { 'A', a } }), "1A:3000");
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    otMaster.boilerFrame(otFrame(kDataInvalid, 2, 0));         // good parity, type DATA-INVALID, wrong MsgID
    loopOTDirect(); loopOTDirect();
    verdict("C18", "as C1, a DATA-INVALID frame for another MsgID: treated as garbage, no reply, nothing cached",
            { r.toBoiler }, {}, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler } }), "-");
  }
  {
    resetAll();
    const unsigned long q = otFrame(kWriteData, 14, degC(50.0)), b = otFrame(kDataInvalid, 14, degC(50.0));
    sendMasterRequestAsync(q, OT_DIRECT_ORIGIN_GATEWAY);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C19", "a queued gateway command answered DATA-INVALID: logged as B, no reply to the thermostat, nothing cached",
            { q }, {}, logOf({ { 'R', q }, { 'B', b } }), "-");
  }
  {
    resetAll();
    otCurrentMode = OTD_MODE_MONITOR;
    const unsigned long t = otFrame(kReadData, 25, 0), b = otFrame(kDataInvalid, 25, 0);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C20", "monitor mode, READ(25), boiler DATA-INVALID: relayed unchanged, nothing cached",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "-");
  }
  {
    resetAll();
    setOverride(1, t40);
    otMaster.busy = true;                                      // e.g. the master still in its post-reply delay
    const unsigned long t = otFrame(kWriteData, 1, t30), rq = otFrame(kWriteData, 1, t40);
    thermostatSends(t); loopOTDirect(); loopOTDirect();        // the frame waits
    otMaster.busy = false;
    loopOTDirect();                                            // forwarded now
    const unsigned long b = otFrame(kWriteAck, 1, t40), a = otFrame(kWriteAck, 1, t30);
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C21", "overridden frame while the master bus is busy for two passes: one T/R pair, then the reply",
            { rq }, { a }, logOf({ { 'T', t }, { 'R', rq }, { 'B', b }, { 'A', a } }));
  }
  {
    resetAll();
    otCurrentMode = OTD_MODE_LOOPBACK;
    const unsigned long t = otFrame(kReadData, 25, 0), b = otFrame(kReadAck, 25, 0x2A80);
    thermostatSends(t); loopOTDirect(); loopOTDirect();
    verdict("C22", "loopback mode, no override, READ(25): the simulated READ-ACK is relayed unchanged",
            {}, { b }, logOf({ { 'T', t }, { 'B', b } }));
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    otMaster.boilerRxError(otFrame(kWriteAck, 1, t40));      // 32 data bits, no stop bit
    loopOTDirect(); loopOTDirect();
    verdict("C23", "as C1, reception error after 32 good bits (status INVALID, WRITE-ACK bits): no reply, nothing cached",
            { r.toBoiler }, {}, logOf({ { 'T', r.tstat }, { 'R', r.toBoiler } }), "-");
  }

  // ---- C24-C28: the DATA-INVALID parity term, RM= in monitor mode, the bus state, a refused reply ----
  {
    resetAll();
    const unsigned long t = otFrame(kReadData, 25, 0), b = otFrame(kDataInvalid, 25, 0) ^ 0x80000000UL;   // parity error
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C24", "as C10, the DATA-INVALID(25) frame with a parity error: treated as garbage, no reply, nothing cached",
            { t }, {}, logOf({ { 'T', t } }), "-");
  }
  {
    resetAll();
    otCurrentMode = OTD_MODE_MONITOR;
    otResponseModifiers[0] = OTResponseModify{ 26, true, 0x2800 };    // RM=26:40.0
    const unsigned long t = otFrame(kReadData, 26, 0), b = otFrame(kReadAck, 26, 0x3000);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C25", "monitor mode with RM=26 active: the boiler's READ-ACK is relayed unmodified, no A",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "1A:3000");
  }
  {
    resetAll();
    const unsigned long t = otFrame(kReadData, 0, 0x0300), b = otFrame(kDataInvalid, 0, 0x0300);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C26", "READ(0) status, boiler DATA-INVALID: relayed unchanged, the bus stays online, nothing cached",
            { t }, { b }, logOf({ { 'T', t }, { 'B', b } }), "-", true);
  }
  {
    resetAll();
    const unsigned long t = otFrame(kReadData, 0, 0x0300);
    thermostatSends(t); loopOTDirect();
    otMaster.boilerTimeout(); loopOTDirect(); loopOTDirect();
    verdict("C27", "READ(0) status, boiler TIMEOUT: no reply, the bus goes offline, nothing cached",
            { t }, {}, logOf({ { 'T', t } }), "-", false);
  }
  {
    OverrideRun r = runOverriddenWrite(1, t30, t40);
    const unsigned long b = otFrame(kWriteAck, 1, t40), a = otFrame(kWriteAck, 1, t30);
    otSlave.busy = true;                                       // the slave cannot send when the boiler answers
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();   // the reply is refused: nothing sent, no A
    otSlave.busy = false;
    thermostatSends(r.tstat); loopOTDirect();                  // the thermostat retries
    otMaster.boilerFrame(b); loopOTDirect(); loopOTDirect();
    verdict("C28", "as C1, the slave refuses the first reply: no reply and no A for it, the retry gets WRITE-ACK(1, own 30.0)",
            { r.toBoiler, r.toBoiler }, { a },
            logOf({ { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b },
                    { 'T', r.tstat }, { 'R', r.toBoiler }, { 'B', b }, { 'A', a } }), "01:2800");
  }

  return finish();
}
