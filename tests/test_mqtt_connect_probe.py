"""TASK-1757 AC3 diagnostics: the connect-window probes in MQTTstuff.ino.

The beta.208/211 boot traces show 40-55 KB of heap vanish in one 83-288 ms gap right after
onMqttConnect, in which the loop does not reach the trace tick. connProbe(tag) is called between
the big loop steps; it keeps a record only when the window minimum fell by 1 KB or more since the
previous probe, or when 20 ms or more passed, so the drop is bracketed by two named call sites.

This test cuts the REAL probe block (from the 'connect-window probes' comment to the end of
connProbePrint) out of MQTTstuff.ino, compiles it on the host with stubbed platform gauges and
checks: a drop is kept with the right at/from tags; a heap that stays low is not recorded again on
every probe; a slow step is kept; nothing is recorded while the trace is off; at most 32 records.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

INO = Path(os.environ.get("OTGW_MQTT_INO", HERE.parent / "src" / "OTGW-firmware" / "MQTTstuff.ino"))
HDR = INO.with_name("MQTTstuff.h")

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#define PSTR(s) s
typedef const char* PGM_P;
static uint32_t nowMs = 1000, freeHeap = 60000, winMin = 60000, winGen = 1;
static uint32_t millis() { return nowMs; }
static uint32_t platformFreeHeap() { return freeHeap; }
static uint32_t platformMaxFreeBlock() { return 30000; }
static bool winActive = true;   // TASK-2019: the takeover trace opens a window when none is open
static uint32_t platformHeapWindowMin() { return winActive ? winMin : 0; }
// ADR-265 cut 1: heapPaceLine() prints the pacer counters and the runtime switch
static uint32_t satPaceNoCount() { return 0; }
static uint32_t satPaceLiveCount() { return 0; }
static uint32_t dripPaceNoCount() { return 0; }
static uint32_t dripPaceLiveCount() { return 0; }
static uint32_t connPaceNoCount() { return 0; }
static uint32_t connPaceLiveCount() { return 0; }
static uint32_t ovrPaceNoCount() { return 0; }
static uint32_t ovrPaceLiveCount() { return 0; }
static uint32_t statsPaceNoCount() { return 0; }
static uint32_t statsPaceLiveCount() { return 0; }
static struct { struct { bool bPaceSat = true; bool bPaceDrip = true; bool bPaceConn = true; bool bPaceTail = true; } debug; } state;
static bool platformHeapWindowActive() { return winActive; }
static bool platformHeapWindowStart() { winActive = true; winGen++; winMin = freeHeap; return true; }
static void platformHeapWindowStop() { winActive = false; }
static uint32_t platformMinFreeHeap() { return winMin; }
static uint32_t platformHeapWinGen() { return winGen; }
static uint32_t platformTcpTxBytesToRemotePort(uint16_t) { return 0; }
static uint16_t mqttQueueNow() { return 3; }
static uint32_t webSocketClientCount() { return 1; }
struct { struct { uint16_t iBrokerPort = 1883; } mqtt; } settings;
static void DebugTf(const char* f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); }
static bool sConnTraceOn = false;
static uint32_t sConnTraceT0 = 1000;
static bool     sSatTraceOn = false, sSatTraceOwnWindow = false;   // TASK-2019 (declared next to sConnTraceOn)
static uint32_t sSatTraceT0 = 0;
static constexpr uint32_t SAT_TRACE_MS = 5000;
static bool     sHeapBurstOpen = false;
enum : uint8_t { BOOTHEAP_IDLE = 0, BOOTHEAP_RUN = 1, BOOTHEAP_DONE = 2 };
static uint8_t  sBootHeapState = BOOTHEAP_DONE;
static inline uint16_t connTraceCap16(uint32_t v) { return v > 0xFFFFu ? 0xFFFFu : (uint16_t)v; }
static inline uint8_t  connTraceCap8(uint32_t v)  { return v > 0xFFu ? 0xFFu : (uint8_t)v; }
"""

MAIN = r"""
static void step(uint32_t dtMs, uint32_t fr, uint32_t wm, uint8_t tag) {
  nowMs += dtMs; freeHeap = fr; if (wm < winMin) winMin = wm; connProbe(tag);
}
static void dump(const char* label) {
  printf("%s n=%u", label, (unsigned)sConnProbeN);
  for (uint8_t i = 0; i < sConnProbeN; i++) printf(" [%u<-%u t=%u dt=%u min=%u]", sConnProbe[i].tag, sConnProbe[i].pTag,
      sConnProbe[i].tMs, sConnProbe[i].dtMs, (unsigned)sConnProbe[i].min4 * 4);
  printf("\n");
}
int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "";
  if (!strcmp(mode, "off")) {               // trace off: nothing is recorded
    connProbeReset(); step(1, 20000, 20000, CPROBE_SAT); step(300, 20000, 20000, CPROBE_SAT); dump("off"); return 0;
  }
  if (!strcmp(mode, "sat")) {                // TASK-2019: armed at the takeover, no window open, slow passes then a drop
    winActive = false; freeHeap = 50000; winMin = 50000;
    satTakeoverHeapTraceBegin();
    step(30, 50000, 50000, CPROBE_LOOP_TOP); step(30, 50000, 50000, CPROBE_SAT_RUN);   // slow, no drop: not kept
    step(5, 52000, 3000, CPROBE_SAT_CORE);   // the drop: bracketed by SAT_RUN -> SAT_CORE
    nowMs += 1000; satTakeoverHeapTraceTick();                  // before 5 s: nothing printed
    printf("|early\n");
    nowMs += 4000; satTakeoverHeapTraceTick();
    printf("|win=%d on=%d\n", (int)winActive, (int)sSatTraceOn);
    step(5, 52000, 1000, CPROBE_SAT_CORE);   // after the end: not recorded
    dump("after"); return 0;
  }
  if (!strcmp(mode, "satconn")) {            // TASK-2019 F1 (review): the boot connect trace starts inside the 5 s
    winActive = true; freeHeap = 50000; winMin = 50000;
    satTakeoverHeapTraceBegin();
    step(5, 52000, 3000, CPROBE_SAT_CORE);   // the takeover drop
    satTakeoverHeapTraceYield();             // what connTraceBegin() does first
    printf("|yield on=%d\n", (int)sSatTraceOn);
    sConnTraceOn = true; connProbeReset();   // the rest of connTraceBegin()
    step(30, 52000, 3000, CPROBE_LOOP_TOP);
    nowMs += 5000; satTakeoverHeapTraceTick();   // nothing more under the takeover label
    printf("|end\n"); return 0;
  }
  if (!strcmp(mode, "satbusy")) {            // the boot connect trace owns the probes: no takeover trace
    sConnTraceOn = true; satTakeoverHeapTraceBegin();
    printf("|on=%d\n", (int)sSatTraceOn); return 0;
  }
  if (!strcmp(mode, "satwin")) {             // a window is already open (burst): the trace leaves it open
    winActive = true; sHeapBurstOpen = true; satTakeoverHeapTraceBegin();
    nowMs += 5000; satTakeoverHeapTraceTick();
    printf("|win=%d\n", (int)winActive); return 0;
  }
  sConnTraceOn = true; connProbeReset();
  if (!strcmp(mode, "drop")) {
    // fast loop passes, then the SAT step takes 45 KB and the heap stays low for later probes
    step(1, 60000, 60000, CPROBE_LOOP_TOP); step(1, 60000, 60000, CPROBE_TIMERS); step(1, 60000, 60000, CPROBE_DISCOVERY);
    step(5, 15000, 15000, CPROBE_SAT);       // the drop: bracketed by DISCOVERY -> SAT
    step(1, 15500, 15000, CPROBE_APP_END); step(1, 15500, 15000, CPROBE_PRE_MQTT); step(1, 15500, 15000, CPROBE_POST_MQTT);
    dump("drop"); return 0;
  }
  if (!strcmp(mode, "slow")) {               // no heap change, one step takes 250 ms
    step(1, 60000, 60000, CPROBE_PRE_MQTT); step(250, 60000, 60000, CPROBE_POST_MQTT); step(1, 60000, 60000, CPROBE_POST_PIC);
    dump("slow"); return 0;
  }
  if (!strcmp(mode, "restart")) {            // window restarted between probes: compare with the previous free heap
    step(1, 60000, 60000, CPROBE_LOOP_TOP); winGen++; winMin = 60000; step(1, 30000, 30000, CPROBE_TIMERS);
    dump("restart"); return 0;
  }
  if (!strcmp(mode, "cap")) {                // many slow steps: at most 32 kept
    for (int i = 0; i < 50; i++) step(25, 60000, 60000, CPROBE_LOOP_TOP);
    dump("cap"); return 0;
  }
  return 1;
}
"""


def extract() -> str:
    src = INO.read_text(encoding="utf-8")
    start = src.find("// TASK-1757 AC3 diagnostics: connect-window probes.")
    assert start >= 0, "connect-window probe block not found in MQTTstuff.ino"
    end = src.find("// --- satHeapTrace end", start)   # TASK-2019: the takeover trace follows connProbePrint()
    assert end > start, "satHeapTrace block not found in MQTTstuff.ino (code before TASK-2019)"
    return src[start:end]


def enum_block() -> str:
    h = HDR.read_text(encoding="utf-8")
    m = re.search(r"enum : uint8_t \{\s*CPROBE_LOOP_TOP.*?\};", h, re.S)
    assert m, "CPROBE_* enum not found in MQTTstuff.h"
    return m.group(0)


class ConnectProbe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "probe.cpp"
        watch = INO.with_name("LoopStepWatch.h")   # TASK-1989: connProbe() also feeds the loop step watch
        inc = '#include "%s"\n' % watch.as_posix() if watch.exists() else ""
        cpp.write_text(STUBS + inc + enum_block() + "\n" + extract() + "\n" + MAIN, encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / ("probe" + (".exe" if os.name == "nt" else "")))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_mode(self, mode):
        return subprocess.run([str(self.exe), mode], capture_output=True, text=True, check=True).stdout.strip()

    def test_drop_is_bracketed_by_the_step_that_took_it(self):
        out = self.run_mode("drop")
        # CPROBE_DISCOVERY = 3, CPROBE_SAT = 4: exactly one record, the drop, at SAT from DISCOVERY
        self.assertEqual(out, "drop n=1 [4<-3 t=8 dt=5 min=15000]")

    def test_slow_step_is_kept(self):
        out = self.run_mode("slow")
        # CPROBE_PRE_MQTT = 8, CPROBE_POST_MQTT = 9
        self.assertEqual(out, "slow n=1 [9<-8 t=251 dt=250 min=60000]")

    def test_drop_across_a_window_restart_is_kept(self):
        out = self.run_mode("restart")
        self.assertEqual(out, "restart n=1 [2<-1 t=2 dt=1 min=30000]")

    def test_nothing_while_the_trace_is_off(self):
        self.assertEqual(self.run_mode("off"), "off n=0")

    def test_takeover_trace_keeps_the_drop_at_the_step_that_took_it(self):
        # TASK-2019: CPROBE_SAT_RUN = 16, CPROBE_SAT_CORE = 17
        out = self.run_mode("sat")
        self.assertIn("[HEAP] sat-takeover begin min=50000 free=50000 maxblk=30000 win=1 own=1", out)
        early, rest = out.split("|early", 1)
        self.assertNotIn("sat-takeover-probe", early, "printed before 5 s")
        self.assertRegex(rest, r"\[HEAP\] sat-takeover-probe t=65 dt=5 at=17 from=16 min=3000 free=52000 ")
        self.assertIn("[HEAP] sat-takeover-probe end kept=1 calls=3", rest)
        self.assertIn("|win=0 on=0", rest)                     # its own window closed again
        self.assertIn("after n=1 ", rest)                     # nothing recorded after the end

    def test_a_connect_trace_that_starts_first_prints_the_takeover_records(self):
        # TASK-2019 F1 (second reader): the connect trace's reset wiped the takeover drop and its slow pass was
        # printed under the takeover label. Now the takeover trace ends first, with its own record.
        out = self.run_mode("satconn")
        before, after = out.split("|yield on=0", 1)
        self.assertRegex(before, r"\[HEAP\] sat-takeover-probe t=5 dt=5 at=17 from=0 min=3000 ")
        self.assertIn("[HEAP] sat-takeover-probe end kept=1", before)
        self.assertNotIn("sat-takeover", after, "takeover lines after the connect trace took over")
        src = INO.read_text(encoding="utf-8")
        body = src[src.index("static void connTraceBegin() {"):]
        body = body[:body.index("connProbeReset();")]
        self.assertIn("satTakeoverHeapTraceYield();", body, "connTraceBegin() resets before it ends the takeover trace")

    def test_takeover_trace_not_armed_during_the_boot_connect_trace(self):
        out = self.run_mode("satbusy")
        self.assertNotIn("sat-takeover begin", out)
        self.assertIn("|on=0", out)

    def test_takeover_trace_leaves_a_window_it_did_not_open(self):
        out = self.run_mode("satwin")
        self.assertIn("own=0", out)
        self.assertIn("|win=1", out)

    def test_at_most_32_records(self):
        self.assertTrue(self.run_mode("cap").startswith("cap n=32 "))


if __name__ == "__main__":
    unittest.main()
