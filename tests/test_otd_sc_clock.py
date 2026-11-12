#!/usr/bin/env python3
"""TASK-1840: OT-Direct SC= stores the time for the thermostat and never writes MsgID 20 to the boiler, like the PIC.

PIC ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45):
  - SetClock (gateway.asm:4331-4388) only stores clock1/clock2 and starts the expiry timer; no frame is sent;
  - MessageID20 (:3048-3056): with a valid time the thermostat's request becomes the matching acknowledgement
    (messageack, :2676-2679) carrying the SC= time; without one, missingclock (:2694-2706) turns a boiler
    Data-Invalid / Unknown-DataID into Data-Invalid for a READ (messageinv changes only the type, :2674-2675)
    and WRITE_ACK with the thermostat's own data for a WRITE; a valid boiler answer passes;
  - tests/thermostat-2.stc with the thermostat frames of tests/clock.stc (WRITE 0x9014892A, READ 0x00140000):
    SC=21:50/3 at 2 s -> A50147532 and AC0147532 at 66 s; after expiry A5014892A and A60140000.

The code under test is REAL, cut from src/OTGW-firmware/OTDirect.ino:
  - the `else if (cmd0 == 'S' && cmd1 == 'C') { ... }` branch of handleOTDirectCommand(), compiled with a
    recording enqueueWriteCommand(); the old code enqueues WRITE 20, the fix must not;
  - otdClockReply() between its markers, with the real setOTParityBit() and buildOTResponse().
Source checks cover the three places the thermostat gets an answer (replyToThermostat, the alternative
slot, the UI= table) and the expiry.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_sc_clock.py
Run: python tests/test_otd_sc_clock.py
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
SRC = OTD_INO.read_text(encoding="utf-8")


def braced(src, start_pat):
    m = re.search(start_pat, src)
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


def marked(src, name):
    m = re.search(r"// --- %s begin\r?\n(.*?)// --- %s end" % (name, name), src, re.S)
    return m.group(1) if m else None


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define siscanf sscanf
#define F(s) (s)
#define OTDDebugTln(...) do {} while (0)
#define OTDDebugTf(...) do {} while (0)
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static int g_enq = 0; static unsigned g_enqId = 0, g_enqData = 0;
static void enqueueWriteCommand(uint8_t id, uint16_t data, const char*) { g_enq++; g_enqId = id; g_enqData = data; }
static char g_rsp[64];
static void synthesizeResponse(const char* buf, const char* rsp) { snprintf(g_rsp, sizeof g_rsp, "%s: %s", buf, rsp); }
static uint32_t otSCLastCommandMs = 0;
static uint16_t otSCClockData = 0;
static void setWriteOverride(uint8_t, uint16_t) {}
static void clearWriteOverride(uint8_t) {}
"""

SC_MAIN = r"""
static void scCmd(const char* buf, const char* value) {
  const char cmd0 = 'S', cmd1 = 'C';
  char rspBuf[24];
  if (false) {}
  @BRANCH@
}
int main() {
  scCmd("SC", "21:50/3");
  printf("enq=%d id=%u data=%04X valid=%d clock=%04X rsp=%s\n", g_enq, g_enqId, g_enqData,
         otSCLastCommandMs != 0, otSCClockData, g_rsp);
  return 0;
}
"""

REPLY_MAIN = r"""
static inline void setOTParityBit(unsigned long &frame) { @PARITY@ }
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data) { @BUILD@ }
@HELPER@
int main() {
  const unsigned long W = 0x9014892AUL, R = 0x00140000UL;          // tests/clock.stc
  const unsigned long UNK = 0xF0140000UL, DINV = 0x60140000UL;      // boiler Unknown-DataID / Data-Invalid
  const unsigned long ACK = 0xC014ABCDUL;                           // a boiler READ_ACK with its own time
  printf("validW %08lX\n", otdClockReply(W, UNK, true, 0x7532));
  printf("validR %08lX\n", otdClockReply(R, UNK, true, 0x7532));
  printf("validRack %08lX\n", otdClockReply(R, ACK, true, 0x7532));
  printf("expW %08lX\n", otdClockReply(W, UNK, false, 0x7532));
  printf("expR %08lX\n", otdClockReply(R, UNK, false, 0x7532));
  printf("expRdinv %08lX\n", otdClockReply(R, DINV, false, 0x7532));
  printf("expRack %08lX\n", otdClockReply(R, ACK, false, 0x7532));
  printf("other %08lX\n", otdClockReply(0x00190000UL, 0xC0193200UL, true, 0x7532));
  return 0;
}
"""


def run(cpp_text):
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "t.cpp"
        cpp.write_text(cpp_text, encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
        if exe is None:
            return None
        return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout


class ScCommand(unittest.TestCase):
    def test_sc_writes_nothing_to_the_boiler(self):
        branch = braced(SRC, r"else if \(cmd0 == 'S' && cmd1 == 'C'\) \{")
        self.assertIsNotNone(branch, "SC= branch not found")
        out = run(PRE + SC_MAIN.replace("@BRANCH@", branch))
        if out is None:
            self.skipTest("no host compiler")
        self.assertIn("enq=0 ", out, "SC= must not enqueue a frame for the boiler (PIC SetClock): " + out)
        self.assertIn("valid=1", out, out)
        self.assertIn("clock=7532", out, "SC=21:50/3 stores HB 0x75 (dow 3 << 5 | 21), LB 0x32: " + out)
        self.assertIn("rsp=SC: 21:50/3", out, out)


class ClockReply(unittest.TestCase):
    rows = None

    @classmethod
    def setUpClass(cls):
        helper = marked(SRC, "otdClockReply")
        if helper is None:
            cls.err = "otdClockReply not found between markers (the code before TASK-1840 has no clock answer)"
            return
        par = braced(SRC, r"static inline void setOTParityBit\(unsigned long &frame\) \{")
        bld = braced(SRC, r"static unsigned long buildOTResponse\(uint8_t type, uint8_t msgId, uint16_t data\) \{")
        body = lambda s: s[s.index("{") + 1:s.rindex("}")]
        text = "#include <cstdio>\n#include <cstdint>\n" + REPLY_MAIN.replace("@PARITY@", body(par)) \
            .replace("@BUILD@", body(bld)).replace("@HELPER@", helper)
        out = run(text)
        if out is None:
            cls.err = "SKIP no host compiler"
            return
        cls.err = None
        cls.rows = dict(l.split() for l in out.splitlines())

    def setUp(self):
        if self.err and self.err.startswith("SKIP"):
            self.skipTest(self.err)
        self.assertIsNone(self.err, self.err)

    def test_valid_time_answers_both_requests_like_thermostat_2_stc(self):
        self.assertEqual(self.rows["validW"], "50147532")   # A50147532
        self.assertEqual(self.rows["validR"], "C0147532")   # AC0147532

    def test_valid_time_wins_over_a_boiler_answer(self):
        self.assertEqual(self.rows["validRack"], "C0147532")

    def test_expired_time_like_thermostat_2_stc(self):
        self.assertEqual(self.rows["expW"], "5014892A")     # A5014892A: WRITE_ACK, thermostat's own data
        self.assertEqual(self.rows["expR"], "60140000")     # A60140000: Data-Invalid
        self.assertEqual(self.rows["expRdinv"], "60140000")

    def test_expired_time_passes_a_valid_boiler_answer(self):
        self.assertEqual(self.rows["expRack"], "C014ABCD")

    def test_other_ids_untouched(self):
        self.assertEqual(self.rows["other"], "C0193200")


class CallSites(unittest.TestCase):
    def test_every_thermostat_answer_path_applies_the_clock_rule(self):
        reply = braced(SRC, r"static void replyToThermostat\(")
        alt = braced(SRC, r"static bool otdAltSlotTaken\(unsigned long thermostatFrame\) \{")
        self.assertIn("otdClockReply(sentReq, reply", reply or "")
        self.assertIn("otdClockReply(thermostatFrame, unknownResp", alt or "")
        self.assertRegex(SRC, r"otdClockReply\(otSlaveFrame, unknownResp")

    def test_expiry_invalidates_the_time_and_is_not_shorter_than_the_pic(self):
        m = re.search(r"OT_SC_EXPIRY_MS\s*=\s*(\d+)", SRC)
        self.assertIsNotNone(m)
        self.assertGreaterEqual(int(m.group(1)), 63000, "PIC: >= 121 x 524.288 ms (gateway.asm:157-158)")
        self.assertNotIn("clearWriteOverride(20)", SRC)


if __name__ == "__main__":
    unittest.main(verbosity=2)
