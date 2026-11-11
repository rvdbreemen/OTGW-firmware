#!/usr/bin/env python3
"""A mode switch (and GW=R) flushes the OT-Direct command ring; no frame queued under the previous mode
reaches the boiler (ADR-223: no gateway WRITE 16/100).

Defect: resetTransientState() cleared tables, caches and state but not the command ring. A WRITE 16 /
WRITE 100 that TT= queued in master mode still reached the boiler once after the switch to gateway mode.
A flush that writes otCmdHead/otCmdTail from resetTransientState() races with the consumer: the reset can
run on the async_tcp task (REST POST /api/v2/otdirect/mode, GW=R) between the loop task's peek of
otCmdQueue[otCmdTail] and its tail advance, which leaves tail=1, head=0 (depth 11) and replays stale slots.
The reset therefore only records a request and the consumer applies it (otCmdApplyPendingFlush).

REAL code, cut from OTDirect.ino with brace matching and compiled on the host:
  ring statics, otCmdQueueEmpty/Full/Depth, otCmdApplyPendingFlush (when present), otCmdEnqueue,
  scheduleMasterRequest (consumer 1), otdAltSlotTaken (consumer 2), resetTransientState,
  applyOTDirectMode.
STUBBED: the hardware (otSlave, otMaster), sendMasterRequestAsync (records the frame; a hook can run
resetTransientState() inside it to model the async_tcp task acting between peek and tail advance),
millis(), bridgeFrameToParser, buildOTResponse, otdIsAltSlot/otdAltScheduleIndex/otdNoteThermostatFrame,
otdOwnFramesAllowed, the schedule table (all entries disabled), settings/state data.

Run on older code: OTGW_OTD_INO=<path to an older OTDirect.ino> python tests/test_otd_mode_switch_queue_reset.py
Run: python tests/test_otd_mode_switch_queue_reset.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))

PRELUDE = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#define F(x) x
#define PSTR(x) x
#define OTDDebugTln(...) do {} while (0)
#define OTDDebugTf(...) do {} while (0)
#ifdef _MSC_VER   // the firmware uses the GCC __atomic builtins; MSVC gets plain single-thread stand-ins
#define __ATOMIC_RELEASE 0
#define __ATOMIC_ACQ_REL 0
#define __ATOMIC_RELAXED 0
#define __atomic_load_n(p, o) (*(p))
#define __atomic_store_n(p, v, o) (*(p) = (v))
template <typename T> static T __atomic_exchange_n(T* p, T v, int) { T o = *p; *p = v; return o; }
#endif
enum OTDirectMode : uint8_t { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2,
                              OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
enum DHWPushState : uint8_t { PUSH_IDLE = 0, PUSH_PENDING, PUSH_STARTED };
enum OTRemoteOverrideMode : uint8_t { OT_OVERRIDE_NONE = 0, OT_OVERRIDE_TEMPORARY, OT_OVERRIDE_CONSTANT };
struct OTScheduleEntry { uint8_t msgId; uint32_t intervalMs; uint32_t lastSentMs; bool disabled; bool isWrite;
                         bool valueSet; uint16_t cachedValue; };
struct OTFrameOverride { bool active; };
struct OTResponseOverride { bool active; };
struct OTRemoteOverrideState { OTRemoteOverrideMode mode; uint16_t f88Value; uint32_t setAtMs; uint32_t lastThermostatMs;
                               uint16_t lastThermostatVal; bool bHaveLastThermostat; uint8_t mismatchCount;
                               bool bAccepted; bool bMsgId9Answered; };
static constexpr uint8_t OT_SCHEDULE_SIZE = 4;
static constexpr uint8_t OT_OVERRIDE_COUNT = 3;
static constexpr uint8_t OT_RESPONSE_OVERRIDE_MAX = 4;
static OTScheduleEntry otSchedule[OT_SCHEDULE_SIZE];
static OTFrameOverride otOverrides[OT_OVERRIDE_COUNT];
static OTResponseOverride otResponseOverrides[OT_RESPONSE_OVERRIDE_MAX];
static OTRemoteOverrideState otRemoteOverride = { OT_OVERRIDE_NONE, 0, 0, 0, 0, false, 0, false, false };
static bool otRemoteOverrideClearPending = false;
static uint8_t otOperModeDHW, otOperModeCH1, otOperModeCH2, otMasterStatusFlags, otDHWOverride, otUnknownIdCount;
static uint8_t otUnknownCounters[32];
static DHWPushState otDHWPushState; static uint32_t otDHWPushStartMs, otLastThermostatMs;
static bool otSummaryMode, otHideReports, otSummaryPending, otThermostatSeen, otDHWBlocking, otCoolingEnable,
            otChModeOff, otSetbackEngaged;
static float otFakeRoomSetpoint;
static OTDirectMode otCurrentMode = OTD_MODE_MASTER;
struct { struct { bool bSummerMode = false; bool bEnableSlave = false; } otd; } settings;
struct { struct { bool bSetbackActive; bool bThermostatConnected = true; OTRemoteOverrideMode eOverrideMode;
                  uint16_t iOverrideF88; } otd;
         struct { bool bOnline = true; } otBus; } state;
static void clearAllResponseModifiers() {}
static void sendWebSocketJSON(const char*) {}
struct OtSlaveStub { void begin(void (*)(), void (*)()) {} void end() {} void sendResponse(unsigned long) {} } otSlave;
struct OtMasterStub { bool isReady() { return true; } } otMaster;
static void slaveISR() {}
static void handleSlaveRequest() {}

// --- stubs for the two consumers (scheduleMasterRequest, otdAltSlotTaken)
enum OTDirectRequestOrigin : uint8_t { OT_DIRECT_ORIGIN_GATEWAY = 0 };
enum class OpenThermMessageType : uint8_t { READ_DATA = 0, WRITE_DATA = 1 };
enum class OpenThermMessageID : uint8_t { Status = 0 };
struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType, OpenThermMessageID, uint16_t) { return 0; } };
static constexpr uint32_t OT_OFFLINE_RETRY_MS = 60000;
static constexpr uint32_t OT_VENT_FAST_INTERVAL_MS = 1000;
static bool otMasterRequestActive = false;
static uint32_t otMinIntervalMs = 100;
static uint32_t otLastAnySendMs = 0;
static uint8_t otScheduleIdx = 0;
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool otdOwnFramesAllowed(bool, bool) { return true; }
static bool otIsVentSlave() { return false; }
static unsigned long buildStatusRequest() { return 0; }
static bool otdIsAltSlot(uint8_t) { return true; }
static uint8_t otdAltScheduleIndex(uint32_t) { return OT_SCHEDULE_SIZE; }
static void otdNoteThermostatFrame(unsigned long, uint32_t) {}
static unsigned long buildOTResponse(uint8_t, uint8_t, uint16_t) { return 0; }
static void bridgeFrameToParser(char, unsigned long) {}
static bool otdSendReply(unsigned long) { return true; }   // TASK-1798 slave-gate holder, not under test here
static void resetTransientState();
static void (*g_onSend)() = nullptr;   // runs once inside the first send
static int g_sent = 0;
static uint32_t g_sentFrames[64];
static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin) {
  g_sentFrames[g_sent++] = (uint32_t)request;
  printf("  sent 0x%08lX\n", (unsigned long)request);
  if (g_onSend) { void (*h)() = g_onSend; g_onSend = nullptr; h(); }
  return true;
}
"""

MAIN = r"""
static uint32_t W16  = 0x10000000UL | (16UL  << 16) | 0x1580;   // WRITE_DATA MsgID 16 (TT=)
static uint32_t W100 = 0x10000000UL | (100UL << 16) | 0x0003;   // WRITE_DATA MsgID 100 (override flags)
static uint32_t W1   = 0x10000000UL | (1UL   << 16) | 0x2800;   // WRITE_DATA MsgID 1 (CS=)

static bool useAlt = false;
static unsigned maxDepth = 0;
// One consumer pass: the real scheduleMasterRequest() or the real otdAltSlotTaken()
static void consumeOnce() {
  g_now += 1000;                                       // clears the MI= gap every pass
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i].disabled = true;   // the reset re-enables them; only the ring is under test
  if (useAlt) otdAltSlotTaken(0x00010000UL); else scheduleMasterRequest();
  if (otCmdQueueDepth() > maxDepth) maxDepth = otCmdQueueDepth();
}
static void consume(int passes) { for (int i = 0; i < passes; i++) consumeOnce(); }
static void report() { printf("sent=%d depth=%u maxdepth=%u\n", g_sent, (unsigned)otCmdQueueDepth(), maxDepth); }
static void raceReset() { resetTransientState(); }     // the async_tcp task acting inside the consumer

int main(int argc, char** argv) {
  const char* c = argc > 1 ? argv[1] : "";
  useAlt = argc > 2 && !strcmp(argv[2], "alt");
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i].disabled = true;
  otCurrentMode = OTD_MODE_MASTER;
  if (!strncmp(c, "switch", 6)) {                       // switchN: master -> mode N with TT= frames queued
    otCmdEnqueue(W16); otCmdEnqueue(W100); otCmdEnqueue(W1);
    printf("before depth=%u\n", (unsigned)otCmdQueueDepth());
    applyOTDirectMode((OTDirectMode)atoi(c + 6));
    otCurrentMode = OTD_MODE_GATEWAY;                  // the consumers run in gateway mode
    consume(6);
    report();
  } else if (!strcmp(c, "reset")) {                     // GW=R path
    otCmdEnqueue(W16); otCmdEnqueue(W100);
    resetTransientState();
    otCurrentMode = OTD_MODE_GATEWAY;
    consume(6);
    report();
  } else if (!strcmp(c, "keep_after")) {                // a frame queued after the switch is sent exactly once
    otCmdEnqueue(W16); otCmdEnqueue(W100);
    applyOTDirectMode(OTD_MODE_GATEWAY);
    bool ok = otCmdEnqueue(W1);
    printf("enq=%d\n", (int)ok);
    consume(6);
    report();
  } else if (!strcmp(c, "race")) {                      // reset between the consumer's peek and its tail advance
    otCmdEnqueue(W16); otCmdEnqueue(W100); otCmdEnqueue(W1);
    otCurrentMode = OTD_MODE_GATEWAY;
    g_onSend = raceReset;
    consume(8);
    report();
  } else if (!strcmp(c, "race_keep")) {                 // same race, a frame enqueued right after the reset survives
    otCmdEnqueue(W16); otCmdEnqueue(W100);
    otCurrentMode = OTD_MODE_GATEWAY;
    g_onSend = raceReset;
    consumeOnce();                                      // peeks W16, the reset runs inside, the tail advances
    otCmdEnqueue(W1);
    consume(8);
    report();
  } else if (!strcmp(c, "wrapped_full")) {              // wrapped ring, full: the flush gives all slots back
    for (int i = 0; i < 7; i++) { otCmdEnqueue(W1); otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE; }  // head/tail mid-array
    int stored = 0;
    for (uint32_t id = 1; id < 40 && !otCmdQueueFull(); id++) {
      if (otCmdEnqueue(0x10000000UL | (id << 16))) stored++;
    }
    printf("full stored=%d depth=%u head=%u tail=%u\n", stored, (unsigned)otCmdQueueDepth(), (unsigned)otCmdHead, (unsigned)otCmdTail);
    applyOTDirectMode(OTD_MODE_GATEWAY);
    otCurrentMode = OTD_MODE_GATEWAY;
    consume(2);
    printf("after depth=%u head=%u tail=%u\n", (unsigned)otCmdQueueDepth(), (unsigned)otCmdHead, (unsigned)otCmdTail);
    int again = 0;
    for (uint32_t id = 1; id < 40 && !otCmdQueueFull(); id++) {
      if (otCmdEnqueue(0x10000000UL | (id << 16))) again++;
    }
    printf("refill stored=%d\n", again);
    report();
  } else if (!strcmp(c, "drained_past")) {             // the consumer is already past the snapshot when the flush applies
    resetTransientState();                              // empty ring: snapshot = head = tail
    otCmdEnqueue(W1);                                   // queued after the reset
    otCmdTail = (otCmdTail + 1) % OT_CMD_QUEUE_SIZE;    // W1 consumed in the window between a consumer's apply and its tail advance
    otCurrentMode = OTD_MODE_GATEWAY;
    consume(4);                                         // the flush must not move the tail back and replay W1
    report();
  } else if (!strcmp(c, "highwater")) {                 // the diagnostic peak survives the flush
    otCmdEnqueue(W16); otCmdEnqueue(W100); otCmdEnqueue(W1);
    unsigned before = otCmdQueueHighWater;
    applyOTDirectMode(OTD_MODE_GATEWAY);
    otCurrentMode = OTD_MODE_GATEWAY;
    consume(4);
    printf("highwater before=%u after=%u\n", before, (unsigned)otCmdQueueHighWater);
  }
  return 0;
}
"""


def cut_braced(src, signature):
    at = src.find(signature)
    if at < 0:
        raise AssertionError(signature + " not found in OTDirect.ino")
    start = src.find("{", at)
    depth = 0
    for i in range(start, len(src)):
        depth += {"{": 1, "}": -1}.get(src[i], 0)
        if depth == 0:
            return src[at:i + 1]
    raise AssertionError("unbalanced braces in " + signature)


def build_source():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    ring_at = src.index("static constexpr uint8_t OT_CMD_QUEUE_SIZE")
    ring = src[ring_at:src.index("// TASK-494: enqueue with coalesce-by-MsgID.", ring_at)]
    parts = [
        ring,
        cut_braced(src, "static bool otCmdEnqueue(uint32_t frame)"),
        cut_braced(src, "static void scheduleMasterRequest()"),
        cut_braced(src, "static bool otdAltSlotTaken(unsigned long thermostatFrame)"),
        cut_braced(src, "static void resetTransientState()"),
        cut_braced(src, "static void applyOTDirectMode(OTDirectMode newMode)"),
    ]
    return PRELUDE + "\n".join(parts) + "\n" + MAIN


TMP = None
EXE = None


def setUpModule():
    global TMP, EXE
    TMP = tempfile.mkdtemp(prefix="otd_queue_reset_")
    cpp = Path(TMP) / "t.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    EXE = compile_cpp(cpp, Path(TMP) / ("t.exe" if os.name == "nt" else "t"))
    if EXE is None:
        raise unittest.SkipTest("no host compiler")


def tearDownModule():
    if TMP:
        shutil.rmtree(TMP, ignore_errors=True)


def run(case, consumer="sched"):
    p = subprocess.run([str(EXE), case, consumer], capture_output=True, text=True, check=True)
    print("--- %s (%s) ---\n%s" % (case, consumer, p.stdout), end="")
    return p.stdout


CONSUMERS = ("sched", "alt")


class TestQueueFlushOnModeSwitch(unittest.TestCase):
    MODES = {0: "bypass", 1: "gateway", 2: "monitor", 3: "master", 4: "loopback"}

    def test_switch_from_master_sends_none_of_the_queued_frames(self):
        for cons in CONSUMERS:
            for m, name in self.MODES.items():
                with self.subTest(consumer=cons, mode=name):
                    out = run("switch%d" % m, cons)
                    self.assertIn("before depth=3", out)
                    self.assertNotIn("sent 0x", out,
                                     "a frame queued under the old mode reaches the boiler after the switch to " + name)
                    self.assertIn("sent=0 depth=0", out)

    def test_gw_reset_sends_none_of_the_queued_frames(self):
        for cons in CONSUMERS:
            with self.subTest(consumer=cons):
                out = run("reset", cons)
                self.assertNotIn("sent 0x", out, "GW=R leaves queued frames to reach the boiler")
                self.assertIn("sent=0 depth=0", out)

    def test_frame_queued_after_the_switch_is_sent_exactly_once(self):
        for cons in CONSUMERS:
            with self.subTest(consumer=cons):
                out = run("keep_after", cons)
                self.assertIn("enq=1", out)
                self.assertEqual(out.count("sent 0x10012800"), 1, "the flush must keep a frame queued after the switch")
                self.assertIn("sent=1 depth=0", out)

    def test_reset_between_peek_and_tail_advance_replays_nothing(self):
        # The old frame is already in flight when the reset arrives; nothing else of the old queue may follow,
        # and the ring depth never leaves 0..2 (a tail past a zeroed head shows depth 11).
        for cons in CONSUMERS:
            with self.subTest(consumer=cons):
                out = run("race", cons)
                self.assertEqual(out.count("sent 0x"), 1, "stale slots replayed after the reset")
                self.assertIn("sent 0x10101580", out)
                self.assertIn("sent=1 depth=0 ", out)
                maxdepth = int(re.search(r"maxdepth=(\d+)", out).group(1))
                self.assertLessEqual(maxdepth, 2, "ring depth jumped after the reset (tail past a reset head)")

    def test_frame_queued_right_after_a_racing_reset_survives(self):
        for cons in CONSUMERS:
            with self.subTest(consumer=cons):
                out = run("race_keep", cons)
                self.assertEqual(out.count("sent 0x"), 2, "expected the in-flight W16 and the new frame only")
                self.assertEqual(out.count("sent 0x10012800"), 1)
                self.assertIn("sent=2 depth=0", out)

    def test_full_wrapped_ring_gives_all_slots_back(self):
        for cons in CONSUMERS:
            with self.subTest(consumer=cons):
                out = run("wrapped_full", cons)
                self.assertIn("full stored=11", out)
                self.assertNotIn("sent 0x", out, "a frame of the full ring reaches the boiler")
                self.assertIn("after depth=0", out)
                self.assertIn("refill stored=11", out)

    def test_flush_never_moves_the_tail_back(self):
        # A snapshot taken on an empty ring, with the consumer already one frame past it, must be rejected:
        # moving the tail back to the snapshot would send the consumed frame a second time.
        for cons in CONSUMERS:
            with self.subTest(consumer=cons):
                out = run("drained_past", cons)
                self.assertNotIn("sent 0x", out, "the flush moved the tail back and replayed a consumed frame")
                self.assertIn("sent=0 depth=0", out)

    def test_high_water_mark_is_kept_as_diagnostic_peak(self):
        out = run("highwater")
        self.assertIn("highwater before=3 after=3", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
