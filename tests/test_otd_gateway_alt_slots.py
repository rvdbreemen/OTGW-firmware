#!/usr/bin/env python3
"""TASK-1707: OT-Direct gateway mode with a thermostat sends the boiler one frame per thermostat frame.

Field report (tjfs, 2026-11-07): in 6 minutes of OT-Direct gateway mode the boiler got 634 frames of the
gateway's own against 369 thermostat frames. TASK-1701 stopped the gateway's own MsgID 0; the rest of the
schedule still ran between the thermostat's frames. The PIC sends one boiler frame per thermostat frame.
It uses a slot for a request of its own only when the thermostat asks a Data-ID the boiler does not support,
and answers the thermostat Unknown-DataID for that slot (gateway.asm:2384-2414 CreateRequest/SendAltRequest,
3390-3471 Alternative/CreateMessage). It sends frames on its own timer only without a thermostat
(gateway.asm:1770-1816 StandAloneMsg).

The real code is cut from src/OTGW-firmware/OTDirect.ino and compiled with a host C++ compiler: the schedule
table and command queue, applyOverrides(), the SR/UI/RM tables and unknown counters, handleSlaveRequest(),
buildStatusRequest() with the ADR-208 helpers, everything from otLoopbackData[] through scheduleMasterRequest()
(and the TASK-1707 slot helpers after it), loopOTDirect(), buildOTResponse() and applyResponseModifiers().
The enums come from OTDirecttypes.h and the OpenTherm statics from the library sources. Test doubles cover
only the hardware (otMaster/otSlave), the parser bridge, millis(), the safeTimers macros and the SAT,
websocket and flame-ratio hooks. A boiler model answers every request 40 ms after it was sent.

The thermostat stream is synthetic, modelled on tjfs's log: one frame per second, MsgID 0 with master status
0x02 every second frame, and three Data-IDs this boiler does not support (WRITE 2 master
configuration, READ 9 and 127). tjfs's attachment is
not in the tree.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino (e.g. `git show lane-1701:...`). On the old
code the defect cases must fail. OTGW_OTD_REF points at a reference copy for the standalone case: its
boiler frame sequence without a thermostat must be identical to the one of the code under test.

Run: python tests/test_otd_gateway_alt_slots.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
FEED_H = REPO / "src" / "OTGW-firmware" / "SATboilerFeed.h"
REF_INO = os.environ.get("OTGW_OTD_REF")
TYPES_H = REPO / "src" / "OTGW-firmware" / "OTDirecttypes.h"
LIB_H = REPO / "src" / "libraries" / "OpenTherm" / "src" / "OpenTherm.h"
LIB_CPP = REPO / "src" / "libraries" / "OpenTherm" / "src" / "OpenTherm.cpp"


def _find(src, regex, start=0):
    m = re.compile(regex, re.M).search(src, start)
    if not m:
        raise AssertionError(f"anchor not found: {regex}")
    return m


def construct(src, regex):
    """From the anchor line to the matching closing brace (or ';' when no brace comes first)."""
    m = _find(src, regex)
    i = m.start()
    depth = 0
    seen = False
    while i < len(src):
        c = src[i]
        if c == "{":
            depth += 1
            seen = True
        elif c == "}":
            depth -= 1
            if seen and depth == 0:
                j = src.find("\n", i)
                return src[m.start():(j if j >= 0 else len(src))] + "\n"
        elif c == ";" and not seen:
            j = src.find("\n", i)
            return src[m.start():j] + "\n"
        i += 1
    raise AssertionError(f"construct not closed: {regex}")


def span(src, start_re, end_re):
    """From the start anchor to the end of the construct at the end anchor."""
    s = _find(src, start_re).start()
    e = _find(src, end_re, s)
    rest = construct(src[e.start():], r"\A")
    return src[s:e.start()] + rest


def slices(src):
    parts = [
        span(src, r"^static constexpr uint32_t OT_STATUS_INTERVAL_MS\b", r"^static constexpr uint32_t OT_VENT_FAST_INTERVAL_MS\b"),
        span(src, r"^static uint16_t otBoilerCache\[128\];", r"^static void handleSlaveRequest\("),
        span(src, r"^static bool otdCsOverrideActive\(\)", r"^static unsigned long buildStatusRequest\(\)"),
    ]
    if "// --- otdAltSlot end" in src:          # TASK-1707 slot helpers follow scheduleMasterRequest()
        s = _find(src, r"^static const uint16_t PROGMEM otLoopbackData\[").start()
        e = src.index("// --- otdAltSlot end")
        parts.append(src[s:e])
    else:
        parts.append(span(src, r"^static const uint16_t PROGMEM otLoopbackData\[", r"^static void scheduleMasterRequest\(\)"))
    parts.append(construct(src, r"^void loopOTDirect\(\)\s*\{"))
    parts.append(construct(src, r"^static unsigned long buildOTResponse\([^)]*\)\s*\{"))
    parts.append(construct(src, r"^static unsigned long applyResponseModifiers\(unsigned long response\)\s*\{"))
    return "\n".join(parts)


def lib_and_types():
    h = LIB_H.read_text(encoding="utf-8")
    c = LIB_CPP.read_text(encoding="utf-8")
    t = TYPES_H.read_text(encoding="utf-8")
    out = [construct(h, r"^enum class OpenThermResponseStatus\b"),
           construct(h, r"^enum class OpenThermMessageType\b"),
           construct(h, r"^enum class OpenThermMessageID\b")]
    statics = [construct(c, r"^bool OpenTherm::parity\(unsigned long frame\)"),
               construct(c, r"^OpenThermMessageType OpenTherm::getMessageType\(unsigned long message\)"),
               construct(c, r"^unsigned long OpenTherm::buildRequest\("),
               construct(c, r"^bool OpenTherm::isValidResponse\(unsigned long response\)"),
               construct(c, r"^bool OpenTherm::isValidRequest\(unsigned long request\)")]
    types = [construct(t, r"^enum OTDirectRequestOrigin\b"), construct(t, r"^enum OTDirectMode\b"),
             construct(t, r"^enum OTRemoteOverrideMode\b"), construct(t, r"^struct OTRemoteOverrideState\b")]
    return "\n".join(out), "\n".join(statics), "\n".join(types)


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
typedef uint8_t byte;
#define PROGMEM
#define PSTR(s) (s)
class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper*)(s))
#define snprintf_P snprintf
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#define HAS_SAT 1
#define OTDDebugTf(...)  ((void)0)
#define OTDDebugTln(...) ((void)0)
#define OTDDebugf(...)   ((void)0)
#define DebugTf(...)     ((void)0)
#define DebugTln(...)    ((void)0)

static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
// safeTimers.h: the 100 ms scheduler timer runs on simulated time; the 1 s and 30 s blocks never run.
static uint32_t g_schedLast = 0;
static bool dueFor(const char* name) {
  if (strcmp(name, "timerOTSchedule") != 0) return false;
  if (g_now - g_schedLast < 100) return false;
  g_schedLast = g_now;
  return true;
}
#define DECLARE_TIMER_MS(name, ...)  static int name = 0
#define DECLARE_TIMER_SEC(name, ...) static int name = 0
#define DUE(name) ((void)(name), dueFor(#name))

@LIB_ENUMS@
class OpenTherm {
public:
  static bool parity(unsigned long frame);
  static OpenThermMessageType getMessageType(unsigned long message);
  static unsigned long buildRequest(OpenThermMessageType type, OpenThermMessageID id, unsigned int data);
  static bool isValidRequest(unsigned long request);
  static bool isValidResponse(unsigned long response);
};
@LIB_STATICS@
@TYPES@

// otMaster/otSlave stand-in. Master: a request is answered by the boiler model 40 ms later.
class FakeOT {
public:
  std::vector<unsigned long> sent;
  std::vector<uint32_t> sentAt;   // TASK-1759: when each request went out (R8 flow-gap check)
  bool waiting = false, answered = false;
  uint32_t answerAt = 0;
  unsigned long response = 0;
  OpenThermResponseStatus status = OpenThermResponseStatus::NONE;
  void process() {}
  bool isReady() { return !waiting || answered; }
  bool sendRequestAsync(unsigned long r) {
    if (!isReady()) return false;
    sent.push_back(r); sentAt.push_back(g_now); waiting = true; answered = false; answerAt = g_now + 40;
    status = OpenThermResponseStatus::NONE; response = 0;
    return true;
  }
  bool sendResponse(unsigned long f) { sent.push_back(f); return true; }
  unsigned long getLastResponse() { return response; }
  OpenThermResponseStatus getLastResponseStatus() { return status; }
};
static FakeOT otMaster, otSlave;

@FEED_H@
// TASK-1759: the MsgID 28 slot reads SAT's on-state and the return feed. Defaults keep it inert (SAT off),
// so the TASK-1707 cases above behave exactly as before; tests/test_otd_return_temp_slot.py turns it on.
static struct { struct { bool bOnline; SatBoilerFeed feedTret; } otBus; struct { bool bThermostatConnected; } otd;
                struct { bool bActive; } sat; } state;
static struct { struct { bool bEnableSlave; int iMode; } otd; struct { bool bSimulation; } sat; } settings;
static bool g_satEnabled = false, g_ret28Unsupported = false;
// TASK-1759 R9: from g_invalidFrom on (0 = never), the boiler answers DATA_INVALID 0xDEAD for MsgID 17 and 25.
static uint32_t g_invalidFrom = 0;
static bool satIsEnabled() { return g_satEnabled; }
static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 28 && g_ret28Unsupported; }
// TASK-1702: the sliced mode-apply head of loopOTDirect() needs these; its flag stays false here.
#ifndef constrain
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#endif
#ifdef _MSC_VER
#define __ATOMIC_RELEASE 0
#define __ATOMIC_ACQ_REL 0
#define __ATOMIC_RELAXED 0
#define __atomic_load_n(p, o) (*(p))
#define __atomic_store_n(p, v, o) (*(p) = (v))
template <typename T> static T __atomic_exchange_n(T* p, T v, int) { T o = *p; *p = v; return o; }
#endif
static void applyOTDirectMode(OTDirectMode) {}

struct LogEntry { char prefix; unsigned long frame; };
static std::vector<LogEntry> g_log;
static void bridgeFrameToParser(char prefix, unsigned long frame) { g_log.push_back({ prefix, frame }); }
static void sendWebSocketJSON(const char*) {}
static bool satSimulationBlocksBusTx(const char*, const __FlashStringHelper*) { return false; }
static void satNotifyBoilerFrameSeen() {}
static bool satOwnsControlSetpoint() { return false; }
static void flameRatioSet(bool) {}
static void onThermostatMsgID16(uint8_t, uint16_t) {}
static void handleMasterModeSlaveFrame(unsigned long) {}
static void emitSummaryLine() {}
static void updateOTDirectStatus() {}
static void checkThermostatTimeout() {}
static void loopFlameRatio() {}
static void clearWriteOverride(uint8_t) {}
// Prototypes the Arduino .ino preprocessor generates for the firmware.
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data);
static unsigned long otdRelayedStatusFrame(unsigned long frame);
static uint16_t otdStatusReplyData(uint16_t thermostatData, uint16_t boilerData);
static unsigned long applyResponseModifiers(unsigned long response);
"""

# TASK-1759: the real SatBoilerFeed / satFeedFresh, filled in here so every user of PRE gets it
PRE = PRE.replace("@FEED_H@", FEED_H.read_text(encoding="utf-8"))

MAIN = r"""
// ---- boiler model --------------------------------------------------------------
static bool g_no24 = false;   // T2 (TASK-1733): tjfs's boiler answers MsgID 24 Unknown-DataID
static bool boilerSupports(uint8_t id) {
  if (g_no24 && id == 24) return false;
  switch (id) { case 2: case 4: case 9: case 127: return false; default: return id <= 57 || (id >= 116 && id <= 123); }
}
static unsigned long frameOf(unsigned type, unsigned id, uint16_t data) {
  unsigned long f = ((unsigned long)type << 28) | ((unsigned long)id << 16) | data;
  setOTParityBit(f);
  return f;
}
static void boilerTick() {
  if (!otMaster.waiting || otMaster.answered || g_now < otMaster.answerAt) return;
  unsigned long q = otMaster.sent.back();
  uint8_t type = (q >> 28) & 7, id = (q >> 16) & 0xFF;
  unsigned long r;
  if (!boilerSupports(id)) r = frameOf(7, id, 0);
  else if (type == 1) r = frameOf(5, id, q & 0xFFFF);
  else if (id == 0) r = frameOf(4, 0, (q & 0xFF00) | 0x0A);
  else if (g_invalidFrom && g_now >= g_invalidFrom && (id == 17 || id == 25)) r = frameOf(6, id, 0xDEAD);
  else r = frameOf(4, id, (uint16_t)(0x1000 + id));
  // TASK-1759: a boiler READ_ACK for MsgID 28 is what processOT() stamps into feedTret (OTGW-Core.ino 'B' frame path)
  if (id == 28 && ((r >> 28) & 7) == 4) state.otBus.feedTret.sampleMs = g_now ? g_now : 1;
  otMaster.response = r;
  otMaster.status = OpenThermResponseStatus::SUCCESS;
  otMaster.answered = true;
}

// ---- thermostat stream (synthetic, one frame per second) -----------------------
static std::vector<unsigned long> g_stream;
static void buildStream(uint8_t statusHB) {
  const unsigned ids[][3] = {   // {type, id, data}
    {0, 0, 0}, {1, 1, 0x2800}, {0, 0, 0}, {0, 25, 0}, {0, 0, 0}, {0, 17, 0}, {0, 0, 0}, {1, 16, 0x1400},
    {0, 0, 0}, {1, 24, 0x1499}, {0, 0, 0}, {1, 2, 0x0112}, {0, 0, 0}, {0, 3, 0}, {0, 0, 0}, {0, 28, 0},
    {0, 0, 0}, {0, 127, 0}, {0, 0, 0}, {0, 9, 0}, {0, 0, 0}, {0, 4, 0} };
  g_stream.clear();
  for (auto& e : ids) g_stream.push_back(frameOf(e[0], e[1], e[1] == 0 ? (uint16_t)(statusHB << 8) : (uint16_t)e[2]));
}

static void resetAll() {
  otCurrentMode = OTD_MODE_GATEWAY;
  state.otBus.bOnline = true; state.otd.bThermostatConnected = false;
  otMaster = FakeOT(); otSlave = FakeOT(); g_log.clear();
  otMasterRequestActive = false; otSlaveFramePending = false; otLastAnySendMs = 0; otScheduleIdx = 0;
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) { otSchedule[i].lastSentMs = 0; otSchedule[i].disabled = false; }
  memset(otUnknownCounters, 0, sizeof(otUnknownCounters));
  g_now = 1000; g_schedLast = 0;
}

struct Run {
  int tframes = 0, bframes = 0, replies = 0, minPer = 1 << 30, maxPer = 0, status0 = 0, bit0Set = 0;
  int altSlots = 0, badSlotReply = 0, w14 = 0, w14InSlot = 0;
  long w14Delay = -1;
  std::string seq;   // every boiler frame, for the standalone comparison
  int perId[256] = {0};
};

static uint8_t idOf(unsigned long f) { return (uint8_t)((f >> 16) & 0xFF); }
static uint8_t typeOf(unsigned long f) { return (uint8_t)((f >> 28) & 7); }

static Run simulate(bool thermostat, uint32_t ms, uint32_t enqueueAt = 0) {
  Run r;
  size_t streamPos = 0;
  int sinceLast = -1;
  size_t seenSent = 0;
  for (uint32_t end = g_now + ms; g_now < end; g_now += 2) {
    if (thermostat && g_now % 1000 == 0) {
      if (sinceLast >= 0) { if (sinceLast < r.minPer) r.minPer = sinceLast; if (sinceLast > r.maxPer) r.maxPer = sinceLast; }
      sinceLast = 0;
      handleSlaveRequest(g_stream[streamPos++ % g_stream.size()], OpenThermResponseStatus::SUCCESS);
      r.tframes++;
      state.otd.bThermostatConnected = true;   // checkThermostatTimeout() sets it on the first second tick
    }
    if (enqueueAt && g_now == enqueueAt) {
      otCmdEnqueue(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (OpenThermMessageID)14, 0x3200));
    }
    boilerTick();
    loopOTDirect();
    while (seenSent < otMaster.sent.size()) {
      unsigned long q = otMaster.sent[seenSent++];
      r.bframes++; if (sinceLast >= 0) sinceLast++;
      r.perId[idOf(q)]++;
      char b[12]; snprintf(b, sizeof b, "%08lX,", q); r.seq += b;
      if (idOf(q) == 0) { r.status0++; if ((q >> 8) & 1) r.bit0Set++; }
      if (idOf(q) == 14 && typeOf(q) == 1) {
        r.w14++;
        if (r.w14Delay < 0) r.w14Delay = (long)(g_now - enqueueAt);
        // in a slot: the log just before the R is the thermostat's T and the gateway's Unknown-DataID A
        size_t n = g_log.size();
        if (n >= 3 && g_log[n - 1].prefix == 'R' && g_log[n - 2].prefix == 'A' && g_log[n - 3].prefix == 'T' &&
            typeOf(g_log[n - 2].frame) == 7 && !boilerSupports(idOf(g_log[n - 3].frame))) r.w14InSlot++;
      }
    }
  }
  r.replies = (int)otSlave.sent.size();
  // every thermostat request for an unsupported Data-ID after the third strike: answered Unknown-DataID
  for (size_t i = 0; i + 1 < g_log.size(); i++) {
    if (g_log[i].prefix == 'T' && g_log[i + 1].prefix == 'A' && !boilerSupports(idOf(g_log[i].frame))) {
      r.altSlots++;
      // CreateMessage (gateway.asm:3464-3471): Unknown-DataID, the thermostat's Data-ID and data bytes, even parity
      const unsigned long a = g_log[i + 1].frame, t = g_log[i].frame;
      if (typeOf(a) != 7 || idOf(a) != idOf(t) || (a & 0xFFFF) != (t & 0xFFFF) || OpenTherm::parity(a)) r.badSlotReply++;
    }
  }
  return r;
}

static void print(const char* name, const Run& r) {
  printf("%s tframes=%d bframes=%d replies=%d minper=%d maxper=%d status0=%d bit0set=%d altslots=%d badslotreply=%d w14=%d w14inslot=%d w14delay=%ld\n",
         name, r.tframes, r.bframes, r.replies, r.minPer == (1 << 30) ? -1 : r.minPer, r.maxPer, r.status0, r.bit0Set,
         r.altSlots, r.badSlotReply, r.w14, r.w14InSlot, r.w14Delay);
  printf("%s-ids", name);
  for (int i = 0; i < 256; i++) if (r.perId[i]) printf(" %d:%d", i, r.perId[i]);
  printf("\n");
}

int main() {
  // A1: tjfs pattern, 6 minutes, thermostat CH off (0x02)
  resetAll(); buildStream(0x02); print("A1", simulate(true, 360000));
  // Q1: a queued write (MM=, MsgID 14) at 2 minutes reaches the boiler in a slot
  resetAll(); buildStream(0x02); print("Q1", simulate(true, 360000, 121000));
  // A30: the same stream for 30 minutes: how often the gateway's own reads reach the boiler
  resetAll(); buildStream(0x02); print("A30", simulate(true, 1800000));
  // S1: no thermostat: the gateway runs its own schedule
  resetAll(); { Run r = simulate(false, 360000); print("S1", r); printf("S1-seq %s\n", r.seq.c_str()); }
  // H1: ADR-208, thermostat CH on (0x03) with a CS override and CH=0: the boiler's MsgID 0 has bit 0 off
  resetAll(); buildStream(0x03); setOverride(1, 0x2800); otChModeOff = true;
  print("H1", simulate(true, 60000)); clearOverride(1); otChModeOff = false;
  // W1 (review of TASK-1707): a cached TSet write (the setback value left after a reconnect, or a
  // master-mode write) must not go out in a slot while the thermostat writes MsgID 1 itself.
  resetAll(); buildStream(0x02); updateWriteCache(1, 0x0A00);
  { Run r = simulate(true, 360000); print("W1", r);
    int stale = 0;
    for (size_t i = 0; i < otMaster.sent.size(); i++) { unsigned long q = otMaster.sent[i]; if (idOf(q) == 1 && (q & 0xFFFF) == 0x0A00) stale++; }
    printf("W1stale n=%d%c", stale, 10); }
  // T1 (TASK-1718, tjfs): TT=20 active. The boiler answers MsgID 9 Unknown-DataID (boilerSupports),
  // so without the gateway's own answer the thermostat never reads the override setpoint.
  resetAll(); buildStream(0x02);
  otRemoteOverride.mode = OT_OVERRIDE_TEMPORARY; otRemoteOverride.f88Value = 0x1400;
  { simulate(true, 360000);
    int t9 = 0, ack = 0;
    for (unsigned long a : otSlave.sent) if (idOf(a) == 9) { t9++; if (typeOf(a) == 4 && (a & 0xFFFF) == 0x1400 && !OpenTherm::parity(a)) ack++; }
    printf("T1 t9=%d ack=%d%c", t9, ack, 10); }
  otRemoteOverride.mode = OT_OVERRIDE_NONE; otRemoteOverride.f88Value = 0;
@T1CLEAR@
  // T2 (TASK-1733, tjfs beta.146): TT=21.5 in gateway mode. tjfs's thermostat never reads MsgID 9 and
  // his boiler answers MsgID 24 Unknown-DataID. The bridge log is decoded as v2.js does
  // (case 24: room = data of a type 1/4/5 frame; case 16: roomSet = data of a type 1 frame).
  resetAll(); g_no24 = true;
  { const unsigned ids[][3] = { {0,0,0x0200}, {1,1,0x3200}, {0,0,0x0200}, {0,25,0}, {0,0,0x0200}, {0,17,0},
                                {0,0,0x0200}, {1,16,0x1400}, {0,0,0x0200}, {1,24,0x1499}, {0,0,0x0200}, {0,27,0} };
    g_stream.clear(); for (auto& e : ids) g_stream.push_back(frameOf(e[0], e[1], (uint16_t)e[2])); }
  simulate(true, 60000);   // 24 becomes an alternative slot (three Unknown-DataID answers)
  { const uint16_t tt = 0x1580;   // TT=21.5: what setRemoteOverride() does via enqueueWriteCommand()
    otCmdEnqueue(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (OpenThermMessageID)16, tt)); updateWriteCache(16, tt); setOverride(16, tt);
    otCmdEnqueue(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (OpenThermMessageID)100, 2)); updateWriteCache(100, 2); setOverride(100, 2);
    otRemoteOverride.mode = OT_OVERRIDE_TEMPORARY; otRemoteOverride.f88Value = tt; }
  size_t from = g_log.size();
  simulate(true, 120000);
  { uint16_t room = 0, roomSet = 0; int room215 = 0, set215 = 0;
    for (size_t i = from; i < g_log.size(); i++) {
      unsigned long f = g_log[i].frame; uint8_t t = typeOf(f), id = idOf(f); uint16_t d = (uint16_t)(f & 0xFFFF);
      bool isAck = (t == 4 || t == 5), isWrite = (t == 1 || t == 5);
      if (id == 24 && (isAck || isWrite)) { room = d; if (d == 0x1580) { room215++; if (room215 <= 3) printf("T2-room215 %c %08lX%c", g_log[i].prefix, f, 10); } }
      if (id == 16 && t == 1) { roomSet = d; if (d == 0x1580) set215++; }
    }
    int boiler16 = 0;
    for (unsigned long q : otMaster.sent) if (idOf(q) == 16 && typeOf(q) == 1 && (q & 0xFFFF) == 0x1580) boiler16++;
    printf("T2 room=%u roomset=%u room215=%d set215=%d boiler16=%d%c", room, roomSet, room215, set215, boiler16, 10); }
  g_no24 = false;
  return 0;
}
"""

# T1 clear part: only on code that has the TASK-1718 one-shot flag (old code lacks the symbol)
T1CLEAR = r"""
  // T1c: after TT=0 the first MsgID 9 read answers READ_ACK 0 once, then the boiler path returns
  resetAll(); buildStream(0x02); otRemoteOverrideClearPending = true;
  { simulate(true, 120000);
    int t9 = 0, zero = 0, first = -1;
    for (unsigned long a : otSlave.sent) if (idOf(a) == 9) {
      if (first < 0) first = (typeOf(a) == 4 && (a & 0xFFFF) == 0) ? 1 : 0;
      t9++; if (typeOf(a) == 4 && (a & 0xFFFF) == 0) zero++; }
    printf("T1c t9=%d zero=%d first=%d pending=%d%c", t9, zero, first, otRemoteOverrideClearPending ? 1 : 0, 10); }
"""


def build_and_run(ino_path):
    src = Path(ino_path).read_text(encoding="utf-8")
    enums, statics, types = lib_and_types()
    pre = PRE.replace("@LIB_ENUMS@", enums).replace("@LIB_STATICS@", statics).replace("@TYPES@", types)
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "t.cpp"
        main = MAIN.replace("@T1CLEAR@", T1CLEAR if "otRemoteOverrideClearPending" in src else "")
        cpp.write_text(pre + slices(src) + main, encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
        if exe is None:
            return None
        return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout


def parse(out):
    rows, ids, seq = {}, {}, {}
    for line in out.splitlines():
        name, _, rest = line.partition(" ")
        if name.endswith("-seq"):
            seq[name[:-4]] = rest
        elif name.endswith("-ids"):
            ids[name[:-4]] = {int(k): int(v) for k, v in (x.split(":") for x in rest.split())}
        else:
            rows[name] = {k: int(v) for k, v in (x.split("=") for x in rest.split())}
    return rows, ids, seq


class GatewayAltSlots(unittest.TestCase):
    rows = ids = seq = out = None
    err = None

    @classmethod
    def setUpClass(cls):
        out = build_and_run(OTD_INO)
        if out is None:
            cls.err = "SKIP no host compiler"
            return
        print("".join(l + "\n" for l in out.splitlines() if not l.startswith("S1-seq")), end="")
        cls.out = out
        cls.rows, cls.ids, cls.seq = parse(out)

    def setUp(self):
        if self.err:
            self.skipTest(self.err)

    def test_A1_one_boiler_frame_per_thermostat_frame(self):
        r = self.rows["A1"]
        self.assertEqual(r["minper"], 1, "a thermostat frame went by without a boiler frame")
        self.assertEqual(r["maxper"], 1, "the gateway sent frames of its own between the thermostat's")
        self.assertLessEqual(abs(r["bframes"] - r["tframes"]), 1)

    def test_A30_one_frame_per_thermostat_frame_and_slow_reads_still_served(self):
        r = self.rows["A30"]
        self.assertEqual((r["minper"], r["maxper"]), (1, 1))
        ids = self.ids["A30"]
        self.assertGreater(sum(ids.get(i, 0) for i in range(116, 124)), 0, "a 60 s schedule entry must get a slot within 30 minutes")

    def test_A1_every_thermostat_frame_answered(self):
        r = self.rows["A1"]
        self.assertGreaterEqual(r["replies"], r["tframes"] - 1)

    def test_A1_unsupported_ids_become_slots_answered_unknown(self):
        r = self.rows["A1"]
        self.assertGreater(r["altslots"], 30, "the slots of Data-IDs 2, 4 and 127 must carry the gateway's reads")
        self.assertEqual(r["badslotreply"], 0, "a slot must be answered Unknown-DataID with the thermostat's Data-ID and data")
        ids = self.ids["A1"]
        self.assertGreater(ids.get(116, 0) + ids.get(120, 0) + ids.get(26, 0), 0, "gateway reads must go out in the slots")

    def test_Q1_queued_write_goes_out_in_a_slot(self):
        r = self.rows["Q1"]
        self.assertEqual(r["w14"], 1, "the queued MsgID 14 write must reach the boiler once")
        self.assertEqual(r["w14inslot"], 1, "the queued write must use a slot of an unsupported Data-ID")
        self.assertEqual(r["maxper"], 1)

    def test_S1_standalone_runs_own_schedule(self):
        r = self.rows["S1"]
        self.assertGreater(r["status0"], 300, "without a thermostat the gateway is the only master and sends MsgID 0")
        self.assertGreater(r["bframes"], 1000)

    @unittest.skipUnless(REF_INO, "OTGW_OTD_REF not set")
    def test_S1_standalone_identical_to_reference(self):
        out = build_and_run(REF_INO)
        _, _, seq = parse(out)
        self.assertEqual(seq["S1"], self.seq["S1"], "the boiler frame sequence without a thermostat changed")

    def test_W1_no_own_write_for_an_id_the_thermostat_writes(self):
        m = re.search(r"^W1stale n=(\d+)$", self.out, re.M)
        self.assertIsNotNone(m, "W1 case missing")
        self.assertEqual(int(m.group(1)), 0, "the gateway wrote its own cached TSet next to the thermostat's")

    def test_T1_tt_override_reaches_the_thermostat_on_msgid9(self):
        # TASK-1718 (tjfs): every thermostat MsgID 9 read during TT=20 gets READ_ACK 20.0,
        # also when the boiler does not support MsgID 9 (no alternative slot takes it).
        r = self.rows["T1"]
        self.assertGreater(r["t9"], 10)
        self.assertEqual(r["ack"], r["t9"], "a MsgID 9 read during TT=20 was not answered READ_ACK 20.0")

    def test_T1c_clear_answers_zero_once(self):
        r = self.rows.get("T1c")
        self.assertIsNotNone(r, "T1c not run: the TASK-1718 clear one-shot is missing")
        self.assertEqual(r["first"], 1, "the first MsgID 9 read after TT=0 must be READ_ACK 0")
        self.assertEqual(r["pending"], 0)

    def test_T2_tt_lands_on_the_setpoint_not_on_the_room_temperature(self):
        # TASK-1733 (tjfs, beta.146): TT=21.5 in gateway mode, thermostat without MsgID 9 reads and a
        # boiler that answers MsgID 24 Unknown-DataID (so 24 is an alternative slot). Decoded as v2.js
        # does: 21.5 must reach the boiler on MsgID 16 and show as the setpoint, never as MsgID 24.
        r = self.rows["T2"]
        self.assertEqual(r["room215"], 0, "21.5 appeared on MsgID 24 (the room temperature)")
        self.assertEqual(r["room"], 0x1499, "the room temperature must stay the thermostat's own MsgID 24 value")
        self.assertEqual(r["roomset"], 0x1580, "the setpoint (MsgID 16) seen by the web UI must be the override")
        self.assertGreater(r["boiler16"], 0, "the boiler must get MsgID 16 = 21.5")

    def test_H1_relayed_status_keeps_adr208_rule(self):
        r = self.rows["H1"]
        self.assertGreater(r["status0"], 20)
        self.assertEqual(r["bit0set"], 0, "ADR-208: with a CS override and CH=0 the relayed MsgID 0 has CH enable off")


if __name__ == "__main__":
    unittest.main(verbosity=2)
