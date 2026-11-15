#!/usr/bin/env python3
"""TASK-1804 Bug 4 (moved to TASK-1954's lane): MsgID 0 goes first whenever it is due.

The scheduler sends one frame per call, round-robin from the entry after the last one it sent. MsgID 0 (status, due
every OT_STATUS_INTERVAL_MS = 800 ms) only came round when the scan wrapped, so a chain of due entries held it back:
the 13 entries of the 10 s tier, or the 81 entries of the 60 s slow-poll burst, each frame taking request + answer +
the library's 50 ms hold + the 100 ms MWT of TASK-1954. Now scheduleMasterRequest() sends MsgID 0 first when due.

REAL code: scheduleMasterRequest() and its constants with the harness of tests/test_otd_offline_retry.py (fake time,
fake bus). The bus model: the boiler answers 50 ms after the request (34 ms frame + slave delay folded in), the end
of the conversation is stamped as handleMasterResponse() does, and the library keeps isReady() false for 50 ms after
the answer (OpenTherm.cpp:443), so the next call can come no earlier. Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino>.
Run: python tests/test_otd_status_priority.py
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
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_otd_offline_retry as ofr  # noqa: E402

MAIN = r"""
// argv[1]: "tier10" = MsgID 0 + 13 entries at 10 s; "burst" = + 81 entries at 60 s (the real table's shape)
int main(int argc, char** argv) {
  const bool burst = argv[1][0] == 'b';
  otCurrentMode = OTD_MODE_MASTER;
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i] = { 0, 60000, 0, true, false, false, 0 };
  int n = 0;
  otSchedule[n++] = { 0, OT_STATUS_INTERVAL_MS, 0, false, false, false, 0 };
  for (int k = 0; k < 13 && n < OT_SCHEDULE_SIZE; k++) otSchedule[n++] = { (uint8_t)(20 + k), 10000, 0, false, false, false, 0 };
  if (burst) for (int k = 0; k < 81 && n < OT_SCHEDULE_SIZE; k++) otSchedule[n++] = { (uint8_t)(100 + k), 60000, 0, false, false, false, 0 };
  state.otBus.bOnline = true;
  uint32_t reqAt = 0, readyAt = 0, lastStatus = 0, maxStatusGap = 0, statusSends = 0;
  for (g_now = 1000; g_now < 181000; g_now += 1) {
    if (otMasterRequestActive) {
      if (g_now - reqAt >= 50) {                       // the boiler answers; handleMasterResponse() ends the conversation
        otMasterRequestActive = false;
        @STAMP@
        readyAt = g_now + 50;                          // the library's RESPONSE_TIME hold before isReady()
      }
      continue;
    }
    if (g_now < readyAt) continue;
    const int before = g_sent;
    scheduleMasterRequest();
    if (g_sent != before) {
      reqAt = g_now; otLastAnySendMs = g_now;
      const uint32_t f = g_sentFrames[before < 64 ? before : 63];
      if (((f >> 16) & 0xFF) == 0) {
        if (lastStatus && g_now > 10000 && g_now - lastStatus > maxStatusGap) maxStatusGap = g_now - lastStatus;
        lastStatus = g_now; statusSends++;
      }
    }
  }
  printf("sent=%d status=%u maxstatusgap=%u\n", g_sent, (unsigned)statusSends, (unsigned)maxStatusGap);
  return 0;
}
"""


def build_source():
    full = ofr.build_source()
    # the shared prelude has a 4-entry table; this test needs the real table's shape (105 entries)
    small = "static constexpr uint8_t OT_SCHEDULE_SIZE = 4;"
    assert small in full, "prelude changed: OT_SCHEDULE_SIZE stub"
    full = full.replace(small, "static constexpr uint8_t OT_SCHEDULE_SIZE = 128;")
    src = ofr.OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    has_mwt = "otdLastConvEndMs" in src
    m = re.search(r"static constexpr uint32_t OTD_MWT_MS = \d+;", src)
    stub = re.search(r"static constexpr uint32_t OTD_MWT_MS = \d+;", full)
    if m and stub:
        full = full.replace(stub.group(0), m.group(0), 1)
    main = MAIN.replace("@STAMP@", "otdLastConvEndMs = g_now;" if has_mwt else "")
    return full.replace(ofr.MAIN, main)


class StatusPriority(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="otd_status_prio_")
        cpp = Path(cls.tmp) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def run_case(self, case):
        if not self.exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(self.exe), case], capture_output=True, text=True, check=True).stdout
        print(case, out.strip())
        return {k: int(v) for k, v in (kv.split("=") for kv in out.split())}

    def test_the_10_s_tier_does_not_hold_back_msgid_0(self):
        self.assertLessEqual(self.run_case("tier10")["maxstatusgap"], 1150, "spec v4.2 :1101 master interval")

    def test_the_60_s_burst_does_not_hold_back_msgid_0(self):
        r = self.run_case("burst")
        self.assertLessEqual(r["maxstatusgap"], 1150)
        self.assertGreaterEqual(r["sent"] - r["status"], 300, "the other entries still go out (old code: about 360)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
