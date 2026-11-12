#!/usr/bin/env python3
"""TASK-1844: the thermostat gets the PIC's own answer for MsgID 6, 125 and an unsupported MsgID 56/57.

PIC 6.8 ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45; MsgUnknown = byte1 bit 5, :298):
  - MessageID6 (:2900-2918): READ_ACK, the HB transfer-enable bits 0 and 1 set ("simulate read access");
  - MessageID125 (:3327-3344): 3.0 when the boiler gave none or one below 3.0, 4.51 from 5.0 up, else the boiler's;
    tests/otversion-1.stc (no answer) and otversion-2.stc (boiler 0x507D0234) -> AC07D0300, otversion-4.stc
    (boiler 0x507D0500) -> A407D0433;
  - hotwaterreturn / maxchsetptret (:3149-3161, :3185-3197): an Unknown-DataID for 56/57 becomes an acknowledgement with
    the upper bound (MsgID 48/49 HB, default MAX_DHWSETPOINT 80 / MAX_CHSETPOINT 90, :262-265); Data-Invalid passes.
These answers also come for a blacklisted ID (TreatMessage builds a fake Unknown-DataID first, :2467-2477), so the
alternative-slot and UI= paths apply the same helper (source checks).

The code under test is REAL: otdPicAnswer() between its markers, with setOTParityBit() and buildOTResponse().
MsgID 2 is deliberately not rewritten (TASK-1844 notes: Smart Power needs a hardware check first).

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_pic_fixed_answers.py
Run: python tests/test_otd_pic_fixed_answers.py
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


def frame(t, i, d):
    f = (t << 28) | (i << 16) | d
    if bin(f).count("1") % 2:
        f |= 0x80000000
    return f


CASES = [  # name, thermostat request, boiler/slot reply, expected frame for the thermostat
    ("v_none", frame(0, 125, 0), frame(7, 125, 0), 0xC07D0300),           # otversion-1: AC07D0300
    ("v_low", frame(0, 125, 0), 0x507D0234, 0xC07D0300),                   # otversion-2: AC07D0300
    ("v_high", frame(0, 125, 0), 0x507D0500, 0x407D0433),                  # otversion-4: A407D0433
    ("v_ok", frame(0, 125, 0), frame(4, 125, 0x0400), frame(4, 125, 0x0400)),
    ("p6_unknown", frame(0, 6, 0), frame(7, 6, 0), frame(4, 6, 0x0300)),
    ("p6_ack", frame(0, 6, 0), frame(4, 6, 0x0101), frame(4, 6, 0x0301)),
    ("s56_unknown", frame(0, 56, 0), frame(7, 56, 0), frame(4, 56, 0x5000)),          # default 80
    ("s56_bound", frame(0, 56, 0), frame(7, 56, 0), frame(4, 56, 0x3C00)),            # MsgID 48 upper bound 60
    ("s56_write", frame(1, 56, 0x2D00), frame(7, 56, 0x2D00), frame(5, 56, 0x5000)),
    ("s56_invalid", frame(0, 56, 0), frame(6, 56, 0), frame(6, 56, 0)),               # Data-Invalid passes
    ("s56_ack", frame(0, 56, 0), frame(4, 56, 0x3700), frame(4, 56, 0x3700)),
    ("s57_unknown", frame(0, 57, 0), frame(7, 57, 0), frame(4, 57, 0x5A00)),          # default 90
    ("other", frame(0, 25, 0), frame(4, 25, 0x3200), frame(4, 25, 0x3200)),
]

MAIN = r"""
#include <cstdio>
#include <cstdint>
static inline void setOTParityBit(unsigned long &frame) { @PARITY@ }
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data) { @BUILD@ }
@HELPER@
int main() {
@CALLS@
  return 0;
}
"""


def build_source():
    m = re.search(r"// --- otdPicAnswer begin\n(.*?)// --- otdPicAnswer end", SRC, re.S)
    helper = m.group(1) if m else ("static unsigned long otdPicAnswer(unsigned long, unsigned long r, uint16_t, uint16_t)"
                                   " { return r; }   // the code before TASK-1844: the boiler's answer goes through\n")
    par = braced(SRC, r"^static inline void setOTParityBit\(unsigned long &frame\) \{")
    bld = braced(SRC, r"^static unsigned long buildOTResponse\(uint8_t type, uint8_t msgId, uint16_t data\) \{")
    body = lambda s: s[s.index("{") + 1:s.rindex("}")]
    calls = []
    for name, req, rep, _ in CASES:
        dhw = "0x3C00" if name == "s56_bound" else "0x5000"
        calls.append(f'  printf("{name} %08lX\\n", otdPicAnswer(0x{req:08X}UL, 0x{rep:08X}UL, {dhw}, 0x5A00));')
    return MAIN.replace("@PARITY@", body(par)).replace("@BUILD@", body(bld)).replace("@HELPER@", helper) \
        .replace("@CALLS@", "\n".join(calls))


class PicAnswers(unittest.TestCase):
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
        cls.rows = dict(l.split() for l in out.splitlines())

    def check(self, *names):
        for name, req, rep, want in CASES:
            if name in names:
                self.assertEqual(self.rows[name], f"{want:08X}", f"{name}: request {req:08X}, boiler {rep:08X}")

    def test_msgid125_version_like_otversion_1_2_4(self):
        self.check("v_none", "v_low", "v_high", "v_ok")

    def test_msgid6_simulates_read_access(self):
        self.check("p6_unknown", "p6_ack")

    def test_msgid56_57_unknown_is_acknowledged_with_the_upper_bound(self):
        self.check("s56_unknown", "s56_bound", "s56_write", "s57_unknown")

    def test_msgid56_data_invalid_and_ack_pass(self):
        self.check("s56_invalid", "s56_ack", "other")


class CallSites(unittest.TestCase):
    def test_every_thermostat_answer_path_applies_it_also_once_blacklisted(self):
        reply = braced(SRC, r"^static void replyToThermostat\(")
        alt = braced(SRC, r"^static bool otdAltSlotTaken\(unsigned long thermostatFrame\) \{")
        self.assertIn("otdPicAnswer(sentReq, reply", reply or "")
        self.assertIn("otdPicAnswer(thermostatFrame, unknownResp", alt or "")      # 3 Unknown strikes: the slot path
        self.assertRegex(SRC, r"otdPicAnswer\(otSlaveFrame, unknownResp")           # UI= table


if __name__ == "__main__":
    unittest.main(verbosity=2)
