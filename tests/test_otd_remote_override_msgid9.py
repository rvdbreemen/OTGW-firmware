#!/usr/bin/env python3
"""TASK-1718: host test for the OT-Direct gateway answer to the thermostat's MsgID 9 / MsgID 100 reads.

The real helper otdRemoteOverrideReply() and setOTParityBit() are cut from
src/OTGW-firmware/OTDirect.ino and compiled with a host C++ compiler; nothing is re-implemented.
OTGW_OTD_INO points at another copy (e.g. `git show HEAD:...`) for old-vs-fix runs.
Without a compiler the compiled cases are skipped.

Run: python tests/test_otd_remote_override_msgid9.py
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

# Frame types: 4 READ_ACK, 5 WRITE_ACK, 6 DATA_INVALID, 7 UNKNOWN_DATA_ID
MAIN = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
@PARITY@
@BODY@
int main(int argc, char** argv) {
  // args: boilerResp(hex) active f88(hex) flags(hex) pending
  unsigned long r = strtoul(argv[1], 0, 16);
  bool active = atoi(argv[2]);
  uint16_t f88 = (uint16_t)strtoul(argv[3], 0, 16);
  uint16_t flags = (uint16_t)strtoul(argv[4], 0, 16);
  bool pending = atoi(argv[5]);
  unsigned long out = otdRemoteOverrideReply(r, active, f88, flags, pending);
  printf("%08lX %d\n", out, pending ? 1 : 0);
  return 0;
}
"""


def cut(src, name):
    m = re.search(r"// --- %s begin\r?\n(.*?)// --- %s end" % (name, name), src, re.S)
    return m.group(1) if m else None


def parity_fn(src):
    m = re.search(r"static inline void setOTParityBit\(unsigned long &frame\) \{.*?\n\}", src, re.S)
    return m.group(0) if m else None


def frame(typ, mid, data):
    f = (typ << 28) | (mid << 16) | data
    p = bin(f & 0x7FFFFFFF).count("1") & 1
    return f | (p << 31)


class Compiled(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        body, par = cut(SRC, "otdRemoteOverrideReply"), parity_fn(SRC)
        if body is None or par is None:
            cls.err = "otdRemoteOverrideReply helper not found between markers"
            return
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "t.cpp"
        cpp.write_text(MAIN.replace("@BODY@", body).replace("@PARITY@", par), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            cls.err = "SKIP no host compiler"

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def setUp(self):
        if self.err and self.err.startswith("SKIP"):
            self.skipTest(self.err)
        if self.err:
            self.fail(self.err)

    def run_h(self, resp, active=0, f88=0, flags=0, pending=0):
        out = subprocess.run([str(self.exe), "%08X" % resp, str(int(active)), "%X" % f88, "%X" % flags,
                              str(int(pending))], capture_output=True, text=True, check=True).stdout.split()
        return int(out[0], 16), int(out[1])

    def test_active_msgid9_read_answers_override_setpoint(self):
        for typ in (4, 6, 7):
            out, _ = self.run_h(frame(typ, 9, 0), active=True, f88=0x1400, flags=2)   # 20.0 C
            self.assertEqual(out, frame(4, 9, 0x1400), "type %d" % typ)

    def test_active_msgid100_read_answers_flags_low_byte(self):
        out, _ = self.run_h(frame(7, 100, 0), active=True, f88=0x1400, flags=0x0002)   # TT
        self.assertEqual(out, frame(4, 100, 0x0002))
        out, _ = self.run_h(frame(4, 100, 0x0000), active=True, f88=0x1400, flags=0x0001)  # TC
        self.assertEqual(out, frame(4, 100, 0x0001))
        self.assertEqual((out >> 8) & 0xFF, 0)

    def test_reply_parity_is_even(self):
        out, _ = self.run_h(frame(7, 9, 0), active=True, f88=0x1480, flags=2)
        self.assertEqual(bin(out).count("1") % 2, 0)

    def test_clear_pending_answers_zero_once(self):
        out, pend = self.run_h(frame(4, 9, 0x1400), pending=True)   # boiler even answers a value
        self.assertEqual(out, frame(4, 9, 0))
        self.assertEqual(pend, 0, "one-shot flag must be cleared when sent")
        out, pend = self.run_h(frame(4, 9, 0x1400), pending=False)  # next read: normal pass-through
        self.assertEqual(out, frame(4, 9, 0x1400))

    def test_clear_pending_does_not_fire_on_other_ids(self):
        r = frame(4, 25, 0x1234)
        out, pend = self.run_h(r, pending=True)
        self.assertEqual((out, pend), (r, 1))
        r = frame(4, 100, 0)
        out, pend = self.run_h(r, pending=True)
        self.assertEqual((out, pend), (r, 1))

    def test_no_override_invalid_or_unknown_becomes_read_ack_zero(self):
        for mid in (9, 100):
            for typ in (6, 7):
                out, _ = self.run_h(frame(typ, mid, 0))
                self.assertEqual(out, frame(4, mid, 0), "mid %d type %d" % (mid, typ))

    def test_no_override_boiler_read_ack_passes_through(self):
        for mid in (9, 100):
            r = frame(4, mid, 0x0A00)
            self.assertEqual(self.run_h(r)[0], r)

    def test_other_ids_and_write_ack_untouched(self):
        for r in (frame(4, 25, 0x1234), frame(7, 25, 0), frame(5, 9, 0x1400), frame(5, 100, 2)):
            self.assertEqual(self.run_h(r, active=True, f88=0x1400, flags=2)[0], r)


class SourcePatterns(unittest.TestCase):
    def test_reply_path_uses_helper_in_gateway_mode(self):
        i = SRC.find("static void replyToThermostat(")
        self.assertGreaterEqual(i, 0)
        body = SRC[i:SRC.find("\n}", i)]
        self.assertIn("otdRemoteOverrideReply(", body, "replyToThermostat must answer MsgID 9/100 reads")

    def test_clear_sets_one_shot(self):
        i = SRC.find("static void clearRemoteOverride() {")
        self.assertGreaterEqual(i, 0)
        body = SRC[i:SRC.find("\n}", i)]
        self.assertRegex(body, r"otRemoteOverrideClearPending\s*=\s*true")


if __name__ == "__main__":
    unittest.main(verbosity=2)
