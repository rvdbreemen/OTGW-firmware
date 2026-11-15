#!/usr/bin/env python3
"""TASK-1804 B9: VS= and MI= answer like the PIC.

PIC ground truth (other-projects/otgw-6.6/gateway.asm):
- SetVentSetpoint: GetDecimalArg; any parse error clears (ClrVentSetpoint -> ValueCleared "VS: -"), characters after the
  number are SE, above 100 is OR. OT-Direct clamped silently (VS=150 stored 100).
- SetInterval: the last character must be a digit (SE), one digit is OR, the digits before it are GetDecimal (SE/OR, SE
  on trailing characters); the value is rounded to 5 ms in an 8-bit register, below 100 ms is OR; MI=99 -> "MI: 100"
  (tests/interval-1.stc). OT-Direct used atoi: MI=99 gave OR, MI=abc gave OR, MI=101 echoed 101.

REAL code: handleOTDirectCommand() head, the VS= and MI= branches and the otdPicDecimal block, cut from
src/OTGW-firmware/OTDirect.ino with the harness parts of tests/test_otd_bare_commands.py / test_otd_pic_float.py.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_vs_mi_pic.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_otd_pic_float as pf  # noqa: E402
import test_otd_bare_commands as bc  # noqa: E402

SRC = pf.SRC

STUBS = r"""
static uint16_t otMinIntervalMs = 100;
static int g_persist = -1;
template <typename T, typename U> static void otdPersistU16(T &slot, U v) { slot = (T)v; g_persist = (int)v; }
static void writeSettings(bool) {}
static uint8_t g_ventSetting = 0;
static uint16_t g_miSetting = 100;
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    start = SRC.index("bool handleOTDirectCommand(const char* buf, int len, bool satOwn) {")
    first = SRC.index("  // CS=", start) if "  // CS=" in SRC[start:] else SRC.index("if (cmd0", start)
    head = SRC[start:first]
    branches = "\n  ".join(pf.braced(SRC, p) for p in (r"else if \(cmd0 == 'V' && cmd1 == 'S'\) \{",
                                                       r"else if \(cmd0 == 'M' && cmd1 == 'I'\) \{"))
    # the cut starts with "else if": give it an opening if
    handler = head + "  if (false) {}\n  " + branches + "\n  return true;\n}\n"
    # the settings stub of the shared prelude has neither field: the two persisted settings become test variables
    handler = handler.replace("settings.otd.iVentSetpoint", "g_ventSetting").replace("settings.otd.iMsgInterval",
                                                                                    "g_miSetting")
    blk = re.search(r"// --- otdPicDecimal begin\n(.*?)// --- otdPicDecimal end", SRC, re.S)
    helpers = blk.group(1) if blk else ""
    fl = re.search(r"// --- otdPicFloat begin\n(.*?)// --- otdPicFloat end", SRC, re.S)
    helpers += "\n" + (fl.group(1) if fl else "")
    if "static bool otdPicInBounds(" in SRC:
        helpers += "\n" + pf.braced(SRC, r"^static bool otdPicInBounds\(")
    helpers = pf.braced(SRC, r"^static inline uint16_t floatToF88\(float celsius\) \{") + "\n" + helpers
    src = pf.PRE + bc.EXTRA + pf.RELAY_STUBS + STUBS + helpers + "\n" + handler + bc.MAIN
    _tmp = Path(tempfile.mkdtemp(prefix="otd_vsmi_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(src, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*cmds):
    out = subprocess.run([str(_exe)] + list(cmds), capture_output=True, text=True, check=True).stdout.splitlines()
    rows = {}
    for line in out:
        head, *fields = line.split("|")
        rows[head] = dict(f.split("=", 1) for f in fields)
    return rows


def answer(r):
    return r["rsp"] or r["st"]


class VentSetpoint(unittest.TestCase):
    def test_like_the_pic(self):
        r = run("VS=50", "VS=100", "VS=101", "VS=150", "VS=50x", "VS=abc", "VS=999")
        self.assertEqual(answer(r["VS=50"]), "50")
        self.assertEqual(answer(r["VS=100"]), "100")
        self.assertEqual(answer(r["VS=101"]), "OR", "above 100 is OR, not a clamp")
        self.assertEqual(answer(r["VS=150"]), "OR")
        self.assertEqual(answer(r["VS=50x"]), "SE", "characters after the number")
        self.assertEqual(answer(r["VS=abc"]), "-", "a parse error clears")
        self.assertEqual(answer(r["VS=999"]), "-", "GetDecimalArg OR (above 255) also clears")


class MessageInterval(unittest.TestCase):
    def test_like_the_pic(self):
        r = run("MI=99", "MI=100", "MI=101", "MI=103", "MI=108", "MI=1275", "MI=1276", "MI=97", "MI=5", "MI=abc",
                "MI=1x0", "MI=10x")
        self.assertEqual(answer(r["MI=99"]), "100", "tests/interval-1.stc: MI=99 -> MI: 100")
        self.assertEqual(answer(r["MI=100"]), "100")
        self.assertEqual(answer(r["MI=101"]), "100", "last digit 0-2 rounds down")
        self.assertEqual(answer(r["MI=103"]), "105", "3-7 rounds to 5")
        self.assertEqual(answer(r["MI=108"]), "110", "8-9 rounds up")
        self.assertEqual(answer(r["MI=1275"]), "1275")
        self.assertEqual(answer(r["MI=1276"]), "1275")
        self.assertEqual(answer(r["MI=97"]), "OR", "below 100 ms after rounding (95)")
        self.assertEqual(answer(r["MI=5"]), "OR", "one digit")
        self.assertEqual(answer(r["MI=abc"]), "SE", "the last character is not a digit")
        self.assertEqual(answer(r["MI=1x0"]), "SE")
        self.assertEqual(answer(r["MI=10x"]), "SE")


if __name__ == "__main__":
    unittest.main(verbosity=2)
