#!/usr/bin/env python3
"""TASK-1842: OT-Direct gets the PIC 6.8 command RT= (room temperature for the boiler, MsgID 24).

PIC 6.8 ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45): SetRoomTemp (:4300-4309) stores
the value in fakeroomtemp1/2 and RT=0 sets NoFakeRoomTemp; MessageID24 (:3061-3068) puts it into the data bytes of
a thermostat MsgID 24 request; the reply is CommandFloat (two decimals). tests/heatpump-1.stc: RT=25.41 -> R10181969
(0x1969: the fraction is rounded, 0.41 * 256 = 104.96 -> 0x69).

The code under test is REAL, cut from src/OTGW-firmware/OTDirect.ino: the RT= branch of handleOTDirectCommand(),
the otOverrides table with setOverride()/clearOverride(), applyOverrides(), buildOverriddenReply(),
updateWriteCache(), clearWriteOverride(), setOTParityBit() and buildOTResponse(). The schedule is a stand-in with
the MsgID 24 write entry.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_rt_room_temp.py
Run: python tests/test_otd_rt_room_temp.py
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
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        return None
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    return None


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cmath>
#include <cstdlib>
#define PSTR(s) (s)
#define OTDDebugTf(...) do {} while (0)
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
static unsigned long otdRelayedStatusFrame(unsigned long f) { return f; }
static uint16_t otdStatusReplyData(uint16_t t, uint16_t) { return t; }
struct OTScheduleEntry { uint8_t msgId; uint32_t intervalMs; uint32_t lastSentMs; bool disabled; bool isWrite; bool valueSet; uint16_t cachedValue; };
static constexpr uint8_t OT_SCHEDULE_SIZE = 2;
static OTScheduleEntry otSchedule[OT_SCHEDULE_SIZE] = { {24, 10000, 0, false, true, false, 0}, {25, 10000, 0, false, false, false, 0} };
static char* dtostrf(double v, int w, int p, char* out) { sprintf(out, "%*.*f", w, p, v); return out; }
static char g_status[8] = "";
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_status, sizeof g_status, "%s", s); }
static char g_rsp[48] = "";
static void synthesizeResponse(const char* buf, const char* v) { snprintf(g_rsp, sizeof g_rsp, "%.2s: %s", buf, v); }
"""

MAIN = r"""
@SLICES@
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  char rspBuf[24];
  g_rsp[0] = 0; g_status[0] = 0;
  if (false) {}
  @BRANCH@
  else { otDirectBridgeProcessStatus("NG"); return false; }
  return true;
}
static void relay(const char* tag, unsigned long thermostat, unsigned long boilerAckData) {
  bool modified = false;
  unsigned long sent = applyOverrides(thermostat, modified);
  unsigned long boiler = buildOTResponse(5, 24, (uint16_t)(boilerAckData ? boilerAckData : (sent & 0xFFFF)));
  unsigned long reply = modified ? buildOverriddenReply(thermostat, sent, boiler) : boiler;
  printf("%s sent=%08lX modified=%d reply=%08lX\n", tag, sent, (int)modified, reply);
}
int main() {
  const unsigned long tw = 0x90181400UL;     // thermostat WRITE_DATA MsgID 24, 20.00 C
  cmd("RT", "25.41");
  printf("rt rsp=[%s] status=[%s] cache=%d:%04X\n", g_rsp, g_status, otSchedule[0].valueSet, otSchedule[0].cachedValue);
  relay("held", tw, 0);
  cmd("RT", "0");
  printf("rt0 rsp=[%s] cache=%d\n", g_rsp, otSchedule[0].valueSet);
  relay("cleared", tw, 0);
  cmd("RT", "x");
  printf("bad status=[%s]\n", g_status);
  cmd("RT", "-1");     printf("neg status=[%s]\n", g_status);
  cmd("RT", "100.01"); printf("over status=[%s]\n", g_status);
  cmd("RT", "0.02");   printf("up rsp=[%s] cache=%04X\n", g_rsp, otSchedule[0].cachedValue);
  cmd("RT", "25.415"); printf("third rsp=[%s] cache=%04X\n", g_rsp, otSchedule[0].cachedValue);
  return 0;
}
"""


def build_source():
    table = re.search(r"^struct OTFrameOverride \{.*?^static constexpr uint8_t OT_OVERRIDE_COUNT[^\n]*\n", SRC, re.S | re.M)
    names = [r"^static const char\* otdPicPercentF88\(const char\* s, uint16_t &out\) \{",
             r"^static inline void setOTParityBit\(unsigned long &frame\) \{",
             r"^static unsigned long buildOTResponse\(uint8_t type, uint8_t msgId, uint16_t data\) \{",
             r"^static void setOverride\(uint8_t msgId, uint16_t value\) \{",
             r"^static void clearOverride\(uint8_t msgId\) \{",
             r"^static void updateWriteCache\(uint8_t msgId, uint16_t value\) \{",
             r"^static void clearWriteOverride\(uint8_t msgId\) \{",
             r"^static unsigned long applyOverrides\(unsigned long frame, bool &modified\) \{",
             r"^static unsigned long buildOverriddenReply\("]
    parts = [table.group(0) if table else None] + [braced(SRC, n) for n in names]
    if parts[1] is None:   # the code before TASK-1842 has no parser and no RT= branch
        parts[1] = 'static const char* otdPicPercentF88(const char*, uint16_t&) { return "NG"; }'
    assert all(parts), "a slice is missing"
    branch = braced(SRC, r"else if \(cmd0 == 'R' && cmd1 == 'T'\) \{") or ""
    return PRE + MAIN.replace("@SLICES@", "\n".join(parts)).replace("@BRANCH@", branch)


class RoomTemp(unittest.TestCase):
    rows = None

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(build_source(), encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                raise unittest.SkipTest("no host compiler")
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.rows = {l.split()[0]: l for l in out.splitlines()}

    def test_ac1_rt_reply_and_stored_value(self):
        r = self.rows["rt"]
        self.assertIn("rsp=[RT: 25.41] status=[]", r, "RT=25.41 must reply 'RT: 25.41' (PIC CommandFloat): " + r)
        self.assertIn("cache=1:1969", r, "RT=25.41 is 0x1969, rounded like the PIC (heatpump-1.stc R10181969): " + r)

    def test_ac1_thermostat_write_reaches_the_boiler_with_the_rt_value(self):
        self.assertIn("sent=10181969 modified=1", self.rows["held"], "the PIC frame of heatpump-1.stc, R10181969: " + self.rows["held"])

    def test_ac2_thermostat_gets_write_ack_with_its_own_data(self):
        self.assertIn("reply=50181400", self.rows["held"], "WRITE_ACK MsgID 24 with the thermostat's own 0x1400: " + self.rows["held"])

    def test_ac1_rt0_stops_it(self):
        self.assertIn("rsp=[RT: 0.00] cache=0", self.rows["rt0"], self.rows["rt0"])
        self.assertIn("sent=90181400 modified=0", self.rows["cleared"], self.rows["cleared"])

    def test_bad_values_give_the_pic_error_codes(self):
        self.assertIn("status=[SE]", self.rows["bad"], self.rows["bad"])     # GetDecimal: not a digit
        self.assertIn("status=[SE]", self.rows["neg"], self.rows["neg"])     # GetPosFloat: no minus
        self.assertIn("status=[OR]", self.rows["over"], self.rows["over"])   # GetPercent: above 100.00

    def test_fraction_rounds_up_like_the_pic(self):
        # 0.02 -> 2/100 * 256 = 5.12 -> 6 (round up, gateway.asm:5173-5175); nearest would give 5
        self.assertIn("cache=0006", self.rows["up"], self.rows["up"])
        # 25.415: the third digit rounds the hundredths to 42 -> 107.52 -> 108 = 0x6C
        self.assertIn("rsp=[RT: 25.42] cache=196C", self.rows["third"], self.rows["third"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
