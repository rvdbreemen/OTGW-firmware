#!/usr/bin/env python3
"""TASK-1802 Bug 1: in master mode a failed MsgID 0 is followed by the next frame within the spec's 1.15 s.

Any non-SUCCESS answer to MsgID 0 sets state.otBus.bOnline = false (handleMasterResponse). The scheduler then sent only
MsgID 0, and only every OT_OFFLINE_RETRY_MS = 5000 ms: with the 1 s library timeout (OpenTherm.cpp:457) the boiler got
no frame for 5 to 6 s. OT spec v4.2: the master communicates at least every 1 s +15 % (MCI, :1101); a slave may treat
5 s without a valid frame as a short circuit, a heat demand (:1006-1016). The PIC has no offline state and retries at
its next 1 s tick (gateway.asm 6.6 :1490-1498).

REAL code: scheduleMasterRequest() and the scheduler constants, cut from OTDirect.ino and compiled on the host with the
prelude of tests/test_otd_mode_switch_queue_reset.py. FAKE: time (10 ms steps), the bus (a request is busy for the
1 s library timeout when the boiler is silent, 50 ms when it answers; a failed MsgID 0 sets bOnline = false and a
good one sets it true, as handleMasterResponse does). Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino>.
Run: python tests/test_otd_offline_retry.py
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
import test_otd_mode_switch_queue_reset as msq  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", msq.REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))

STUB_OFFLINE = "static constexpr uint32_t OT_OFFLINE_RETRY_MS = 60000;"
OLD_SEND = msq.PRELUDE[msq.PRELUDE.index("static bool sendMasterRequestAsync("):]
OLD_SEND = OLD_SEND[:OLD_SEND.index("\n}\n") + 3]
NEW_SEND = r"""
static uint32_t g_sentAt[4096];
static uint32_t g_busyUntil = 0;
static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin) {
  if (g_sent < 4096) { g_sentFrames[g_sent < 64 ? g_sent : 63] = (uint32_t)request; g_sentAt[g_sent] = g_now; }
  g_sent++;
  otMasterRequestActive = true;
  return true;
}
"""

MAIN = r"""
// argv[1]: "dead" = the boiler never answers; "blip" = it answers, except for one MsgID 0 at about 10 s
int main(int argc, char** argv) {
  const bool dead = argv[1][0] == 'd';
  otCurrentMode = OTD_MODE_MASTER;
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i] = { 0, 60000, 0, true, false, false, 0 };
  otSchedule[0] = { 0, OT_STATUS_INTERVAL_MS, 0, false, false, false, 0 };
  otSchedule[1] = { 25, OT_TEMP_INTERVAL_MS, 0, false, false, false, 0 };
  state.otBus.bOnline = !dead;
  bool failedOnce = false;
  uint32_t reqAt = 0;
  int reqIdx = -1;
  for (g_now = 1000; g_now < 31000; g_now += 10) {
    if (otMasterRequestActive) {
      const bool fail = dead || (!failedOnce && g_now >= 10000 && ((g_sentFrames[reqIdx < 64 ? reqIdx : 63] >> 16) & 0xFF) == 0);
      if (g_now - reqAt >= (fail ? 1000u : 50u)) {          // the library reports TIMEOUT after 1 s, an answer after ~50 ms
        if (((g_sentFrames[reqIdx < 64 ? reqIdx : 63] >> 16) & 0xFF) == 0) state.otBus.bOnline = !fail;
        if (fail && !dead) failedOnce = true;
        otMasterRequestActive = false;
      }
      continue;
    }
    const int before = g_sent;
    scheduleMasterRequest();
    if (g_sent != before) { reqAt = g_now; reqIdx = before; otLastAnySendMs = g_now; }
  }
  uint32_t maxGap = 0;
  for (int i = 1; i < g_sent && i < 4096; i++) if (g_sentAt[i] - g_sentAt[i - 1] > maxGap) maxGap = g_sentAt[i] - g_sentAt[i - 1];
  printf("sent=%d maxgap=%u\n", g_sent, (unsigned)maxGap);
  return 0;
}
"""


def build_source():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    consts = "\n".join(m.group(0) for m in re.finditer(r"static constexpr uint32_t OT_\w+_(?:INTERVAL|RETRY)_MS\s*=\s*\d+;", src)
                       if "VENT_FAST" not in m.group(0))
    pre = msq.PRELUDE
    assert STUB_OFFLINE in pre and OLD_SEND in pre
    pre = pre.replace(STUB_OFFLINE, consts).replace(OLD_SEND, NEW_SEND)
    # buildStatusRequest()/buildRequest return the MsgID in bits 16-23 so the bus model knows a MsgID 0 frame
    pre = pre.replace("static unsigned long buildStatusRequest() { return 0; }",
                      "static unsigned long buildStatusRequest() { return 0x00000300UL; }")
    pre = pre.replace("struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType, OpenThermMessageID, uint16_t) { return 0; } };",
                      "struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType, OpenThermMessageID id, uint16_t) "
                      "{ return (unsigned long)id << 16; } };")
    ring_at = src.index("static constexpr uint8_t OT_CMD_QUEUE_SIZE")
    ring = src[ring_at:src.index("// TASK-494: enqueue with coalesce-by-MsgID.", ring_at)]
    note = (msq.cut_braced(src, "static void otdNoteCmdSent(uint32_t frame, uint32_t now)")
            if "static void otdNoteCmdSent(" in src else "")
    return pre + ring + "\n" + note + "\n" + msq.cut_braced(src, "static void scheduleMasterRequest()") + "\n" + MAIN


class OfflineRetry(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="otd_offline_retry_")
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

    def test_one_failed_status_frame_is_followed_within_1150_ms(self):
        r = self.run_case("blip")
        self.assertLessEqual(r["maxgap"], 1150, "after one failed MsgID 0 the next frame goes out within the spec's 1.15 s")

    def test_a_silent_boiler_still_gets_a_frame_every_1150_ms(self):
        r = self.run_case("dead")
        self.assertLessEqual(r["maxgap"], 1150, "an absent boiler is probed at the spec rate, not every 5 s")
        self.assertGreaterEqual(r["sent"], 25, "about one MsgID 0 per second for 30 s")


if __name__ == "__main__":
    unittest.main(verbosity=2)
