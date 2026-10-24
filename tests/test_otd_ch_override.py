#!/usr/bin/env python3
"""TASK-1346: host test for the OT-Direct CH= override rule (PIC CHModeOff semantics).

The real helper otdEffectiveMasterFlags() is cut from src/OTGW-firmware/OTDirect.ino between its
begin/end markers and compiled with a host C++ compiler; nothing is re-implemented. Source-pattern
checks cover the call site (buildStatusRequest) and the CH= handler.
OTGW_OTD_INO points at another copy (e.g. `git show HEAD:...`) for old-vs-fix runs.
Without a compiler the compiled cases are skipped.

Run: python tests/test_otd_ch_override.py
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

MAIN = r"""
#include <cstdint>
#include <cstdio>
@BODY@
int main() {
  for (int board = 0; board < 256; board += 0x7E) {   // 0x00, 0x7E, 0xFC: spread of other bits
    for (int off = 0; off < 2; off++)
      for (int cs = 0; cs < 2; cs++)
        printf("%d %d %d %d\n", board | 1, off, cs, otdEffectiveMasterFlags((uint8_t)(board | 1), off, cs));
  }
  printf("%d %d %d %d\n", 0x20, 0, 0, otdEffectiveMasterFlags(0x20, false, false));  // hysteresis cleared bit0
  printf("%d %d %d %d\n", 0x20, 1, 0, otdEffectiveMasterFlags(0x20, true, false));
  printf("%d %d %d %d\n", 0x20, 0, 1, otdEffectiveMasterFlags(0x20, false, true));
  printf("%d %d %d %d\n", 0x21, 1, 1, otdEffectiveMasterFlags(0x21, true, true));
  return 0;
}
"""


def extract(src):
    m = re.search(r"// --- otdEffectiveMasterFlags begin\r?\n(.*?)// --- otdEffectiveMasterFlags end", src, re.S)
    return m.group(1) if m else None


def func_body(src, header):
    i = src.find(header)
    if i < 0:
        return ""
    j = src.find("\n}", i)
    return src[i:j]


class Compiled(unittest.TestCase):
    rows = None

    @classmethod
    def setUpClass(cls):
        body = extract(SRC)
        if body is None:
            cls.err = "otdEffectiveMasterFlags helper not found between markers"
            return
        cls.err = None
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(MAIN.replace("@BODY@", body), encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                cls.err = "SKIP no host compiler"
                return
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
            cls.rows = [tuple(int(x) for x in l.split()) for l in out.splitlines()]

    def setUp(self):
        if self.err and self.err.startswith("SKIP"):
            self.skipTest(self.err)
        if self.err:
            self.fail(self.err)

    def test_cs_active_chmode_on_forces_bit0_set(self):
        for board, off, cs, res in self.rows:
            if cs and not off:
                self.assertEqual(res & 1, 1)

    def test_cs_active_chmode_off_forces_bit0_clear(self):
        for board, off, cs, res in self.rows:
            if cs and off:
                self.assertEqual(res & 1, 0)

    def test_no_cs_override_board_bit0_stands(self):
        for board, off, cs, res in self.rows:
            if not cs:
                self.assertEqual(res, board)
        self.assertIn((0x20, 1, 0, 0x20), self.rows)   # CH=0 without CS: hysteresis-cleared bit0 stays 0
        self.assertIn((0x20, 0, 0, 0x20), self.rows)

    def test_other_bits_untouched(self):
        for board, off, cs, res in self.rows:
            self.assertEqual(res & ~1, board & ~1)


class SourcePatterns(unittest.TestCase):
    def test_status_request_uses_effective_flags(self):
        body = func_body(SRC, "static unsigned long buildStatusRequest()")
        self.assertIn("otdEffectiveMasterFlags(", body, "buildStatusRequest must use the effective flags")
        self.assertNotRegex(body, r"\(unsigned int\)otMasterStatusFlags\s*<<")

    def test_ch_handler_writes_only_the_mode_flag(self):
        i = SRC.find("cmd0 == 'C' && cmd1 == 'H'")
        self.assertGreaterEqual(i, 0)
        seg = SRC[i:SRC.find("synthesizeResponse", i)]
        self.assertIn("otChModeOff", seg, "CH= handler must write otChModeOff")
        self.assertNotIn("otMasterStatusFlags", seg, "CH= handler must not write otMasterStatusFlags")

    def test_sat_disable_hands_back_ch(self):
        # The CH= flag outlives CS=0 on both boards; a later CS= (the OTDirect heating
        # curve, the ESP curve on a PIC board) would apply SAT's last CH=0 again.
        sat = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino")).read_text(encoding="utf-8", errors="replace")
        body = func_body(sat, "void satDisable()")
        self.assertIn('"CS=0"', body)
        self.assertIn('"CH=1"', body, "satDisable must restore the CH override flag (CH=1)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
