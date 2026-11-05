#!/usr/bin/env python3
"""TASK-1391: only a boiler (B) frame is evidence of what the boiler supports.

The PIC prints 'A' for every response it transmits to the thermostat itself (otgw-6.6 gateway.asm:2490-2496: 'R' for a
request, 'A' for a response it sends; nothing in monitor mode; OTGW-Protocol.md:92 "modified response being sent to the
thermostat"). The boiler's own answer is always a B frame. An A frame with no B of the same id just before it (an unpaired A,
bAnswerOverride false) used to count as boiler evidence. In gruijter's field capture 20261030-092718 (alpha.549, 350k lines) the
boiler never saw MsgIDs 9, 16, 24, 57 or 115 (no B frame at all), yet the PIC answered them ~350 times each with an unpaired A
after a different B (T57 R26 B26 A57), and the published otgw-firmware/boiler/unsupported_msgids held 24W and 115R.

This test compiles the REAL bitmap block of OTGW-Core.ino (cut from the '{' that opens 'const uint8_t idx = OTdata.id >> 3;'
by brace matching) and feeds it frame sequences. Run against another copy with OTGW_CORE_INO=<path> (red on the code before
TASK-1391). The frame decode and the (B,A) pairing before the block are not run here; bAnswerOverride is set per frame as the
pairing would set it.

Run: python tests/test_boiler_support_bitmap_b_only.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

CORE = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))

PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
enum { OTGW_THERMOSTAT = 1, OTGW_BOILER, OTGW_REQUEST_BOILER, OTGW_ANSWER_THERMOSTAT };
enum { OT_READ_DATA = 0, OT_WRITE_DATA = 1, OT_INVALID_DATA = 2, OT_READ_ACK = 4, OT_WRITE_ACK = 5, OT_DATA_INVALID = 6,
       OT_UNKNOWN_DATA_ID = 7 };
struct OTd { int id = 0, type = 0, masterslave = 0, rsptype = 0; bool bAnswerOverride = false, bLocalAnswer = false, bReplayed = false; };
static OTd OTdata;
static uint8_t boilerUnsupportedRead[32], boilerUnsupportedWrite[32], boilerAckedRead[32], boilerAckedWrite[32];
static uint8_t boilerLastMasterWasWrite[32], thermostatSentRead[32], thermostatSentWrite[32];
static bool boilerUnsupportedDirty, boilerFileDirty, thermostatFileDirty, boilerUnknown28ThisBoot;
static bool bit(const uint8_t* b, int id) { return (b[id >> 3] >> (id & 7)) & 1; }
static void feed(char k, int type, int id, bool override_ = false, bool local = false) {
  OTdata = OTd();
  OTdata.id = id; OTdata.type = type; OTdata.bAnswerOverride = override_; OTdata.bLocalAnswer = local;
  OTdata.rsptype = k == 'T' ? OTGW_THERMOSTAT : k == 'R' ? OTGW_REQUEST_BOILER : k == 'B' ? OTGW_BOILER : OTGW_ANSWER_THERMOSTAT;
  OTdata.masterslave = (k == 'T' || k == 'R') ? 0 : 1;
"""

MAIN = r"""
}
int main(int argc, char** argv) {
  // argv[1]: frames "K:type:id[:o][:l]" separated by spaces in one argument
  char buf[4096]; strncpy(buf, argv[1], sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
  for (char* tok = strtok(buf, " "); tok; tok = strtok(nullptr, " ")) {
    char k = tok[0]; int type = 0, id = 0; char f1 = 0, f2 = 0;
    sscanf(tok + 2, "%d:%d:%c:%c", &type, &id, &f1, &f2);
    feed(k, type, id, f1 == 'o' || f2 == 'o', f1 == 'l' || f2 == 'l');
  }
  for (int id = 0; id < 256; id++) {
    int ar = bit(boilerAckedRead, id), aw = bit(boilerAckedWrite, id), ur = bit(boilerUnsupportedRead, id), uw = bit(boilerUnsupportedWrite, id);
    if (ar || aw || ur || uw) printf("%d %d %d %d %d\n", id, ar, aw, ur, uw);
  }
  printf("u28 %d\n", boilerUnknown28ThisBoot ? 1 : 0);
  return 0;
}
"""


def cut_bitmap_block(src):
    anchor = src.find("const uint8_t idx  = OTdata.id >> 3;")
    if anchor < 0:
        raise AssertionError("bitmap block anchor not found in " + str(CORE))
    start = src.rfind("{", 0, anchor)
    depth = 0
    for i in range(start, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError("unbalanced braces in the bitmap block")


RD, WR, RACK, WACK, UNK = 0, 1, 4, 5, 7


class BoilerSupportBitmapBOnly(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "bitmap.cpp"
        cpp.write_text(PRELUDE + cut_bitmap_block(CORE.read_text(encoding="utf-8")) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("bitmap.exe" if os.name == "nt" else "bitmap"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_frames(self, frames):
        out = subprocess.run([str(self.exe), " ".join(frames)], capture_output=True, text=True, check=True).stdout
        bits, u28 = {}, None
        for line in out.splitlines():
            p = line.split()
            if p[0] == "u28":
                u28 = p[1] == "1"
            else:
                bits[int(p[0])] = dict(zip(("ar", "aw", "ur", "uw"), (x == "1" for x in p[1:])))
        return bits, u28

    def test_field_pattern_pic_answer_after_another_b_is_not_boiler_evidence(self):
        # gruijter 20261030-092718: T57 R26 B26 A57(Read-Ack), T16 R26 B26 A16(Write-Ack), T9 ... A9(Read-Ack)
        bits, _ = self.run_frames([f"T:{RD}:57", f"R:{RD}:26", f"B:{RACK}:26", f"A:{RACK}:57",
                                   f"T:{WR}:16", f"R:{RD}:26", f"B:{RACK}:26", f"A:{WACK}:16",
                                   f"T:{RD}:9", f"R:{RD}:19", f"B:{RACK}:19", f"A:{RACK}:9"])
        for i in (57, 16, 9):
            self.assertNotIn(i, bits, "MsgID %d marked from a PIC-built A frame: %r" % (i, bits.get(i)))
        self.assertTrue(bits[26]["ar"], "the genuine B26 Read-Ack still counts")

    def test_pic_unknown_data_id_answer_does_not_brand_the_boiler(self):
        # the published unsupported_msgids held 24W and 115R with no B24 / B115 in 350k lines
        bits, _ = self.run_frames([f"T:{WR}:24", f"R:{RD}:26", f"B:{RACK}:26", f"A:{UNK}:24",
                                   f"T:{RD}:115", f"R:{RD}:19", f"B:{RACK}:19", f"A:{UNK}:115"])
        self.assertNotIn(24, bits, "24W branded unsupported by a PIC answer")
        self.assertNotIn(115, bits, "115R branded unsupported by a PIC answer")

    def test_pic_unknown_answer_for_28_does_not_confirm_a_missing_return(self):
        # SATcontrol.ino flood rule needs "a boiler frame of this boot confirmed Unknown-DataID for MsgID 28"
        bits, u28 = self.run_frames([f"T:{RD}:28", f"R:{RD}:26", f"B:{RACK}:26", f"A:{UNK}:28"])
        self.assertFalse(u28, "a PIC A(28 Unknown) set boilerUnknown28ThisBoot")
        self.assertNotIn(28, bits)

    def test_boiler_frames_still_set_and_retract(self):
        bits, u28 = self.run_frames([f"T:{RD}:28", f"B:{UNK}:28",
                                     f"T:{WR}:14", f"B:{UNK}:14",
                                     f"T:{RD}:25", f"B:{RACK}:25",
                                     f"T:{WR}:1", f"B:{WACK}:1"])
        self.assertTrue(bits[28]["ur"] and u28, "B28 Unknown-DataID is boiler evidence")
        self.assertTrue(bits[14]["uw"], "B14 Unknown-DataID on a write")
        self.assertTrue(bits[25]["ar"] and bits[1]["aw"])
        bits, _ = self.run_frames([f"T:{RD}:28", f"B:{UNK}:28", f"T:{RD}:28", f"B:{RACK}:28"])
        self.assertFalse(bits[28]["ur"], "a later B28 Read-Ack retracts the verdict")
        self.assertTrue(bits[28]["ar"])

    def test_override_a_after_its_own_b_and_local_frames_count_for_nothing(self):
        bits, _ = self.run_frames([f"T:{RD}:5", f"B:{UNK}:5", f"A:{RACK}:5:o",          # answer-override A: the B decides
                                   f"T:{RD}:6", f"B:{RACK}:6:l"])                          # gateway-made B (OTDirect loopback)
        self.assertTrue(bits[5]["ur"] and not bits[5]["ar"])
        self.assertNotIn(6, bits)


if __name__ == "__main__":
    unittest.main(verbosity=2)
