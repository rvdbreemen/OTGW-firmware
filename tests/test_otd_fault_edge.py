#!/usr/bin/env python3
"""TASK-1804 Bug 5: a new boiler fault reads the fault code (MsgID 5) at once, like the PIC.

PIC ground truth (other-projects/otgw-6.6/gateway.asm statusreturn :2745-2758): on a valid MsgID 0 answer the slave
status LB & 0x41 (fault bit 0, diagnostic bit 6) is checked; clear forgets BoilerFault; set and not seen before sets
BoilerFault and calls SetPriorityMsg(MSG_FAULTCODE), so MsgID 5 goes out at the first opportunity. OT-Direct only read
MsgID 5 from its 60 s schedule entry.

REAL code: the otdFaultEdge block is cut from src/OTGW-firmware/OTDirect.ino and driven with status bytes; a source
check pins that handleMasterResponse() enqueues READ 5 on its edge.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_fault_edge.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

MAIN = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
@BLOCK@
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) printf("%d", otdFaultEdge((uint8_t)strtoul(argv[i], nullptr, 16)) ? 1 : 0);
  printf("\n");
  return 0;
}
"""


def block():
    m = re.search(r"// --- otdFaultEdge begin\n(.*?)// --- otdFaultEdge end", SRC, re.S)
    if not m:
        raise AssertionError("otdFaultEdge() not found in OTDirect.ino (no fault-code read on a new fault)")
    return m.group(1)


class FaultEdge(unittest.TestCase):
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(MAIN.replace("@BLOCK@", block()), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_seq(self, *status):
        if not self.exe:
            self.skipTest("no host compiler")
        return subprocess.run([str(self.exe)] + list(status), capture_output=True, text=True, check=True).stdout.strip()

    def test_a_new_fault_triggers_once(self):
        # flame only, fault set, fault still set, fault + flame, cleared, fault again
        self.assertEqual(self.run_seq("08", "01", "01", "09", "00", "01"), "010001")

    def test_the_diagnostic_bit_counts_too(self):
        self.assertEqual(self.run_seq("00", "40", "40", "41", "00", "40"), "010001")

    def test_other_bits_do_not_trigger(self):
        self.assertEqual(self.run_seq("02", "04", "08", "0E", "BE"), "00000")

    def test_handle_master_response_reads_msgid_5_on_the_edge(self):
        i = SRC.index("static void handleMasterResponse() {")
        body = SRC[i:SRC.index("\n}\n", i)]
        j = body.index("otdFaultEdge(")
        call = body[j:j + 400]
        self.assertIn("static_cast<OpenThermMessageID>(5)", call)
        self.assertIn("otCmdEnqueue(read5)", call)
        k = body.index("if (status == OpenThermResponseStatus::SUCCESS) {")
        self.assertLess(k, j, "only on a valid answer, as statusreturn ('Not a valid response from the boiler')")


if __name__ == "__main__":
    unittest.main(verbosity=2)
