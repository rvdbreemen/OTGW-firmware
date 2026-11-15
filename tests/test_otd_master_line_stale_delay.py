#!/usr/bin/env python3
"""TASK-1966: the OT-Direct boiler line falls silent for 35.8 min after 35.8 min without a valid boiler answer.

Root cause (library src/libraries/OpenTherm/src/OpenTherm.cpp): isReady() also needs
micros() - delayTimestamp < 0x80000000 (:136), and delayTimestamp is written only on a valid answer (DATA_READY,
:443) or in setDelay(). A timeout (:457-460) leaves it alone. micros() is 32 bit and wraps every 71.6 min, so with
no valid answer (no boiler, boiler off) isReady() turns false 35.8 min after the last valid answer and stays false
for 35.8 min: no frame goes out, otMasterRequestActive stays set, the TASK-1761 command ring is not drained (503).
Seen on .61 (09, beta.298, no boiler): stall from ~08:06, flips 08:42 ... 11:05.

The code under test is REAL:
- from OpenTherm.cpp: the isReady() expression, the DATA_READY delay line and the WAIT_RESPONSE timeout block,
  placed in a host stand-in of the OpenTherm class with the library's protected member names;
- from OTDirect.ino: the "otdMasterLine" block (OTDMasterLine::keepFresh) and the "otdMasterGuard" block.
The host loop mimics loopOTDirect(): process(), the guard, handleMasterResponse when ready, then a new request when
ready (the schedule always has one due). micros() is a fake clock that starts near 2^32 so it wraps on the way.

Cases: (1) no boiler for 80 min after one valid answer: frames keep going every second on the fix; on the old code
none go out from 35.8 to 71.6 min. (2) a boiler answering every request: same frames on old and fix, no repairs.
(3) the TX timer stops mid-frame: the guard restarts the line once and frames resume.
Old-vs-fix: OTGW_OTD_INO=<old OTDirect.ino> python tests/test_otd_master_line_stale_delay.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
LIB_CPP = REPO / "src" / "libraries" / "OpenTherm" / "src" / "OpenTherm.cpp"


def lib_parts():
    c = LIB_CPP.read_text(encoding="utf-8").replace("\r\n", "\n")
    m = re.search(r"bool IRAM_ATTR OpenTherm::isReady\(\)\s*\{\s*return (.*?);\s*\}", c, re.S)
    assert m, "isReady() not found in OpenTherm.cpp"
    ready = m.group(1)
    assert "delayTimestamp = ts + RESPONSE_TIME * 1000UL;" in c, "DATA_READY delay line not found"
    t = re.search(r"(    if \( \(txStatus == OpenThermTxStatus::WAIT_RESPONSE\).*?\n    \})", c, re.S)
    assert t, "WAIT_RESPONSE timeout block not found"
    return ready, t.group(1)


def block(src, name):
    m = re.search(r"// --- %s begin\n(.*?)// --- %s end" % (name, name), src, re.S)
    return m.group(1) if m else None


PRE = r"""
#include <cstdio>
#include <cstdint>
#define PSTR(s) (s)
static uint32_t g_us = 0;
static uint32_t micros() { return g_us; }
enum class OpenThermRxStatus { NOT_INITIALIZED, IDLE, START_BIT, RECEIVING, DATA_READY, DATA_INVALID };
enum class OpenThermTxStatus { NOT_INITIALIZED, IDLE, TX_DATA, WAIT_RESPONSE };
enum class OpenThermResponseStatus { NONE, SUCCESS, INVALID, TIMEOUT };
static const uint16_t RESPONSE_TIME = 50;
static int g_log = 0;
#define DebugTf(...) (g_log++)
static struct { struct { bool bMasterRequestActive = false; uint16_t iLineStaleFixes = 0, iLineTxRestarts = 0,
                         iCmdRingDropped = 0; } otd; } state;
// Host stand-in for the library class: the protected members keep their names, the logic below is cut from
// OpenTherm.cpp. The TX timer is modelled: a frame takes 34 ms and then waits for the answer.
class OpenTherm {
public:
  OpenTherm(int, int, bool = false) {}
  bool isReady() { return @READY@; }
  bool sendRequestAsync(unsigned long) {
    if (!isReady()) return false;
    txStatus = OpenThermTxStatus::TX_DATA; requestTimestamp = micros(); txEndUs = micros() + 34000; frames++;
    return true;
  }
  void process() {
    unsigned long newTs = micros(); unsigned long reqts = requestTimestamp;
    if (txStatus == OpenThermTxStatus::TX_DATA && !txStalled && (int32_t)(newTs - txEndUs) >= 0) {
      txStatus = OpenThermTxStatus::WAIT_RESPONSE; requestTimestamp = micros(); reqts = requestTimestamp;
      if (boilerAnswers) answerAtUs = newTs + 40000;
    }
    if (txStatus == OpenThermTxStatus::TX_DATA && !txStalled) requestTimestamp = micros();   // the timer ISR ticks
    if (txStatus == OpenThermTxStatus::WAIT_RESPONSE && boilerAnswers && (int32_t)(newTs - answerAtUs) >= 0) {
      unsigned long ts = newTs;   // DATA_READY: the real line
      rxStatus = OpenThermRxStatus::IDLE; txStatus = OpenThermTxStatus::IDLE;
      delayTimestamp = ts + RESPONSE_TIME * 1000UL;
      responseStatus = OpenThermResponseStatus::SUCCESS;
      return;
    }
@TIMEOUT@
  }
  void end() { rxStatus = OpenThermRxStatus::NOT_INITIALIZED; }        // as the library: txStatus is not touched
  bool begin(void (*)(void)) { txStalled = false; rxStatus = OpenThermRxStatus::IDLE; return true; }
  int frames = 0; bool boilerAnswers = false; bool txStalled = false;
protected:
  volatile OpenThermRxStatus rxStatus = OpenThermRxStatus::IDLE;
  volatile OpenThermTxStatus txStatus = OpenThermTxStatus::IDLE;
  volatile OpenThermResponseStatus responseStatus = OpenThermResponseStatus::NONE;
  volatile unsigned long requestTimestamp = 0;
  volatile unsigned long delayTimestamp = 0;
  uint32_t txEndUs = 0, answerAtUs = 0;
  void processResponse() {}
public:
  void stallTx() { txStalled = true; }
  void forceTx(uint32_t ts) { txStatus = OpenThermTxStatus::TX_DATA; requestTimestamp = ts; }   // case r
  void forceAnswerNow() { delayTimestamp = micros() + RESPONSE_TIME * 1000UL; }   // one valid answer at t=0
};
#define PIN_OT_MASTER_IN 1
#define PIN_OT_MASTER_OUT 2
"""

MAIN = r"""
#ifndef HAVE_GUARD
static void otdMasterGuard() {}
#endif
int main(int argc, char** argv) {
  const char c = argv[1][0];
#ifdef HAVE_GUARD
  if (c == 'r') {   // the ISR stamped requestTimestamp 5 us after `now` was taken: no stall
    g_us = 0x10000000u; otMaster.forceTx(g_us + 5);
    printf("buckets 0 0 0 0 0 0 0 0\nstale 0 txrestart %u log 0\n", (unsigned)otMaster.keepFresh(g_us));
    return 0;
  }
#endif
  g_us = 0xF0000000u;                       // wraps after ~4.5 min
  if (c == 'b') otMaster.boilerAnswers = true;
  otMaster.forceAnswerNow();
  int bucket[9] = {0};
  const uint32_t stepUs = 100000;          // one loop pass per 100 ms
  for (uint32_t i = 0; i < 80u * 600u; i++) {
    if (c == 's' && i == 3000) { otMasterRequestActive = otMaster.sendRequestAsync(1) || otMasterRequestActive; otMaster.stallTx(); }
    const int before = otMaster.frames;
    otMaster.process();
    otdMasterGuard();
    if (otMasterRequestActive && otMaster.isReady()) otMasterRequestActive = false;   // handleMasterResponse
    if (!otMasterRequestActive && (i % 10) == 0) otMasterRequestActive = otMaster.sendRequestAsync(1);
    bucket[i / 6000] += otMaster.frames - before;
    g_us += stepUs;
  }
  printf("buckets");
  for (int b = 0; b < 8; b++) printf(" %d", bucket[b]);
  printf("\nstale %u txrestart %u log %d\n", (unsigned)state.otd.iLineStaleFixes, (unsigned)state.otd.iLineTxRestarts, g_log);
  return 0;
}
"""

CRLF, LF = chr(13) + chr(10), chr(10)
_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    ready, timeout = lib_parts()
    line, guard = block(src, "otdMasterLine"), block(src, "otdMasterGuard")
    body = PRE.replace("@READY@", ready).replace("@TIMEOUT@", timeout)
    body += "static void masterISR() {}\nstatic bool otMasterRequestActive = false;\n"
    if line and guard:
        body += line + "static OTDMasterLine otMaster(PIN_OT_MASTER_IN, PIN_OT_MASTER_OUT);\n#define HAVE_GUARD 1\n" + guard
    else:
        body += "static OpenTherm otMaster(PIN_OT_MASTER_IN, PIN_OT_MASTER_OUT);\n"
    body += MAIN
    _tmp = Path(tempfile.mkdtemp(prefix="otd_line_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(body, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(case):
    out = subprocess.run([str(_exe), case], capture_output=True, text=True, check=True).stdout.split("\n")
    b = [int(x) for x in out[0].split()[1:]]
    t = out[1].split()
    return b, {"stale": int(t[1]), "txrestart": int(t[3]), "log": int(t[5])}


class MasterLineStaleDelay(unittest.TestCase):
    def test_1_no_boiler_frames_never_stop(self):
        b, r = run("n")
        # 10-min buckets: the old code sends nothing in the buckets from 40 to 70 min (35.8-71.6 min stalled)
        self.assertTrue(all(x >= 250 for x in b), f"frames per 10 min without a boiler answer: {b}")
        self.assertEqual(r["txrestart"], 0)

    def test_2_answering_boiler_unchanged_and_no_repair(self):
        b, r = run("b")
        self.assertTrue(all(x >= 250 for x in b), b)
        self.assertEqual((r["stale"], r["txrestart"], r["log"]), (0, 0, 0), r)

    def test_3_tx_stall_restarts_the_line_once(self):
        b, r = run("s")
        self.assertEqual(r["txrestart"], 1, r)
        self.assertTrue(all(x >= 250 for x in b[1:]), f"frames after the stall: {b}")

    def test_4_the_real_loop_runs_the_guard_before_it_asks_isready(self):
        src = OTD_INO.read_text(encoding="utf-8").replace(CRLF, LF)
        i = src.index("void loopOTDirect() {")
        body = src[i:src.index(LF + "}" + LF, i)]
        p, g = body.find("  otMaster.process();"), body.find("otdMasterGuard();")
        q = body.find("if (otMasterRequestActive && otMaster.isReady())")
        self.assertTrue(p >= 0 and q > p, "loopOTDirect() layout changed")
        self.assertTrue(p < g < q, "loopOTDirect() must call otdMasterGuard() after otMaster.process() and before isReady()")
        self.assertIn("static OTDMasterLine otMaster(", src, "otMaster must be the OTDMasterLine subclass")

    def test_5_isr_timestamp_just_after_now_is_no_stall(self):
        b, r = run("r")
        self.assertEqual(r["txrestart"], 0, "keepFresh() must not report a stall for a timestamp 5 us after now")


if __name__ == "__main__":
    unittest.main(verbosity=2)
