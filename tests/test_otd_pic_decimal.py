#!/usr/bin/env python3
"""TASK-1804 B8: MH=, MW= and M2= parse their value like the PIC.

PIC ground truth (other-projects/otgw-6.6/gateway.asm): SetOperModeHC1 :4424-4436, SetOperModeDHW :4440,
SetOperModeHC2 :5208 call GetDecimalArg (:5002-5025): a first character that is not a digit is SE, digits are read up to
the first non-digit (MH=5x gives 5; no end check, unlike SetPrioMessage), above 255 is OR; then a value above 15 is OR.
OT-Direct used atoi(): a non-number became 0 and was stored, 256 wrapped to 0, and 16-255 answered BV.

REAL code: the otdPicDecimal block (otdPicDecimalArg) is cut from src/OTGW-firmware/OTDirect.ino and run; the three
command branches are checked in the source to use it and to answer OR above 15.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_pic_decimal.py
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

CASES = [("5", "ok 5"), ("15", "ok 15"), ("16", "ok 16"), ("255", "ok 255"), ("256", "OR"), ("999", "OR"),
         ("abc", "SE"), ("", "SE"), ("-1", "SE"), ("5x", "ok 5"), ("007", "ok 7")]

MAIN = r"""
#include <cstdio>
#include <cstdint>
#include <cctype>
@BLOCK@
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    uint8_t v = 0;
    const char* err = otdPicDecimalArg(argv[i][0] == '#' ? "" : argv[i], v);
    if (err) printf("%s\n", err); else printf("ok %u\n", (unsigned)v);
  }
  return 0;
}
"""


def block():
    m = re.search(r"// --- otdPicDecimal begin\n(.*?)// --- otdPicDecimal end", SRC, re.S)
    if not m:
        raise AssertionError("otdPicDecimalArg() not found in OTDirect.ino (MH/MW/M2 still use atoi)")
    return m.group(1)


class PicDecimal(unittest.TestCase):
    def test_get_decimal_arg_like_the_pic(self):
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(MAIN.replace("@BLOCK@", block()), encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                self.skipTest("no host compiler")
            args = [v if v else "#" for v, _ in CASES]
            out = subprocess.run([str(exe)] + args, capture_output=True, text=True, check=True).stdout.splitlines()
        for (v, want), got in zip(CASES, out):
            self.assertEqual(got, want, f"value {v!r}")

    def test_mh_mw_m2_use_it_and_answer_or_above_15(self):
        for cmd in ("'M' && cmd1 == 'H'", "'M' && cmd1 == 'W'", "'M' && cmd1 == '2'"):
            with self.subTest(cmd=cmd):
                i = SRC.index("cmd0 == " + cmd)
                branch = SRC[i:SRC.index("else if (cmd0 ==", i + 10)]
                self.assertIn("otdPicDecimalArg(value, val)", branch)
                self.assertIn('if (val > 15) { otDirectBridgeProcessStatus("OR")', branch)
                self.assertNotIn("atoi(value)", branch)


if __name__ == "__main__":
    unittest.main(verbosity=2)
