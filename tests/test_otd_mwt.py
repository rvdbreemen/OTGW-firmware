#!/usr/bin/env python3
"""TASK-1954: the gateway's own frames keep the 100 ms master wait time (MWT) after the END of a conversation.

OT spec v4.2 :1100: "The master unit must wait 100ms (MWT) from the end of a previous conversation before initiating a
new conversation." scheduleMasterRequest() gapped its frames only by MI= (otMinIntervalMs, default 100 ms) counted from
the START of the previous send; with a late boiler answer (here 900 ms) the next frame started right at the end of the
answer, 0 ms into the MWT. The OpenTherm library itself only waits RESPONSE_TIME (50 ms) after a received frame
(OpenTherm.cpp:443). Now handleMasterResponse() stamps otdLastConvEndMs and the scheduler waits OTD_MWT_MS from it.

REAL code: scheduleMasterRequest() and the scheduler constants, cut from OTDirect.ino with the harness of
tests/test_otd_offline_retry.py (fake time, fake bus); the end stamp is taken where handleMasterResponse() takes it
(a source check pins that line; a timeout does not stamp, so the offline MsgID 0 keeps its ~1 s cadence, see
tests/test_otd_offline_retry.py). Runs on old AND new code: on the old code the gap after a late answer is 1 ms.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino>.
Run: python tests/test_otd_mwt.py
"""
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_otd_offline_retry as ofr  # noqa: E402

MAIN = r"""
// argv[1]: boiler answer time in ms
int main(int argc, char** argv) {
  const uint32_t answerMs = (uint32_t)atoi(argv[1]);
  const bool timeoutCase = argc > 2 && argv[2][0] == 't';   // no answer: the library's 1 s TIMEOUT, no end stamp
  uint32_t maxStartGap = 0, lastStart = 0;
  otCurrentMode = OTD_MODE_MASTER;
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i] = { 0, 60000, 0, true, false, false, 0 };
  otSchedule[0] = { 0, OT_STATUS_INTERVAL_MS, 0, false, false, false, 0 };
  otSchedule[1] = { 25, 100, 0, false, false, false, 0 };      // always due: the scheduler sends as soon as it may
  state.otBus.bOnline = true;
  uint32_t reqAt = 0, endAt = 0, minGap = 0xFFFFFFFFu;
  bool haveEnd = false;
  for (g_now = 1000; g_now < 20000; g_now += 1) {
    if (otMasterRequestActive) {
      if (g_now - reqAt >= answerMs) {                 // the boiler answers; handleMasterResponse() ends the conversation
        otMasterRequestActive = false;
        if (!timeoutCase) { otdLastConvEndMs = g_now; endAt = g_now; haveEnd = true; }   // as handleMasterResponse()
      }
      continue;
    }
    const int before = g_sent;
    scheduleMasterRequest();
    if (g_sent != before) {
      if (haveEnd && g_now - endAt < minGap) minGap = g_now - endAt;
      if (lastStart && g_now - lastStart > maxStartGap) maxStartGap = g_now - lastStart;
      lastStart = g_now; reqAt = g_now; otLastAnySendMs = g_now;
    }
  }
  printf("sent=%d mingap=%u maxstartgap=%u\n", g_sent, (unsigned)minGap, (unsigned)maxStartGap);
  return 0;
}
"""


def build_source():
    src = ofr.OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    full = ofr.build_source()
    assert ofr.MAIN in full
    # the shared prelude (test_otd_mode_switch_queue_reset) stubs OTD_MWT_MS and otdLastConvEndMs; use the source's value
    stub = re.search(r"static constexpr uint32_t OTD_MWT_MS = \d+;", full).group(0)
    m = re.search(r"static constexpr uint32_t OTD_MWT_MS = \d+;", src)
    if m:
        full = full.replace(stub, m.group(0), 1)
    return full.replace(ofr.MAIN, MAIN)


class Mwt(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="otd_mwt_")
        cpp = Path(cls.tmp) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp) / ("t.exe" if sys.platform == "win32" else "t"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def run_case(self, answer_ms, mode="a"):
        if not self.exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(self.exe), str(answer_ms), mode], capture_output=True, text=True, check=True).stdout
        print(answer_ms, out.strip())
        return {k: int(v) for k, v in (kv.split("=") for kv in out.split())}

    def test_a_late_boiler_answer_is_followed_by_the_full_mwt(self):
        self.assertGreaterEqual(self.run_case(900)["mingap"], 100, "spec v4.2 :1100: 100 ms from the end of the conversation")

    def test_a_fast_boiler_answer_keeps_the_mwt_too(self):
        self.assertGreaterEqual(self.run_case(30)["mingap"], 100)

    def test_a_timeout_keeps_the_next_frame_within_the_master_interval(self):
        # review (92): a timeout does not stamp the end, so a silent boiler still gets a frame within 1.15 s of the previous
        # start (spec v4.2 :1101); the library reports the timeout after 1 s (OpenTherm.cpp:457)
        r = self.run_case(1000, "t")
        self.assertLessEqual(r["maxstartgap"], 1150)

    def test_the_scheduler_still_sends(self):
        self.assertGreater(self.run_case(30)["sent"], 50, "about one frame per 130 ms for 19 s")


class StampSource(unittest.TestCase):
    def test_handle_master_response_stamps_the_end_of_every_received_frame(self):
        # a received frame (answer or invalid) stamps the end; a timeout does not (the bus was quiet ~1 s already, and the
        # offline MsgID 0 must stay within the 1.15 s master interval: tests/test_otd_offline_retry.py, maxgap <= 1150)
        src = ofr.OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.index("static void handleMasterResponse() {")
        body = src[a:src.index("\n}\n", a)]
        self.assertRegex(body, r"\n  if \(status != OpenThermResponseStatus::TIMEOUT\) otdLastConvEndMs = millis\(\);\n"
                               r"  otMasterRequestActive = false;", "right before the request is marked done")


if __name__ == "__main__":
    unittest.main(verbosity=2)
