#!/usr/bin/env python3
"""TASK-1957: a bare "XX=" (no value) on OT-Direct gets the PIC 6.8 answer instead of nothing.

Found on the TASK-1859 bench (09, beta.294 on .61): "CS=" got no reply, because handleOTDirectCommand() returned
at its `len < 4` entry check before any branch. The TASK-1859 host test fed value='' straight into the cut-out
branch and never went through that entry check; this test does.

PIC ground truth (OTGW-other-projects/otgw-6.8, repo 770aa45): GetPercent/GetPosFloat/GetFloat refuse an empty
value with SE (GetDecimal :5045-5049 needs a digit first; tests/command-4.stc: "CS=" -> SE); OT=, MM= and CL= clear
on a parse error (ClrOutsideT :4263, ClrMaxModLevel :4806, ClrCoolLevel :4463, the ValueCleared reply "XX: -").

The code under test is REAL: handleOTDirectCommand() from its first line through the OT= branch (the CS=, TC=, TT=,
BS=, RT=, C2=, CC=, SW=, SH=, MM=, OT= branches), plus the CL= and SB= branches, the otdPicFloat block and
otdPicInBounds(), all cut from src/OTGW-firmware/OTDirect.ino; the doubles come from tests/test_otd_pic_float.py.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_bare_commands.py
Run: python tests/test_otd_bare_commands.py
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
import test_otd_pic_float as pf  # noqa: E402  (shares OTGW_OTD_INO and its doubles)

SRC = pf.SRC

EXTRA = r"""
static bool otdWriteRefused = false;
static struct DiagRec { bool active = false; void recOtdCommand(const char*, size_t) {} } diagRec;
static bool satOwnsControlSetpoint() { return false; }
static bool satBlocksExternalCommand(const char*, int) { return false; }
static void satNoteExternalRefused(const char*, int) {}
"""

MAIN = r"""
int main(int argc, char** argv) {
  for (int a = 1; a < argc; a++) {
    g_rsp[0] = 0; g_status[0] = 0; g_enq = g_clear = g_set = -1; g_rmode = 0; g_rclear = 0;
    const bool ok = handleOTDirectCommand(argv[a], (int)strlen(argv[a]), false);
    printf("%s|ok=%d|rsp=%s|st=%s|enq=%d|clr=%d|set=%d|rmode=%d\n", argv[a], ok ? 1 : 0, g_rsp, g_status, g_enq, g_clear,
           g_set, g_rmode);
  }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    start = SRC.index("bool handleOTDirectCommand(const char* buf, int len, bool satOwn) {")
    vs = SRC.index("  // VS=xx", start)
    head = SRC[start:vs]
    extra_branches = "\n  ".join(pf.braced(SRC, p) for p in (r"else if \(cmd0 == 'C' && cmd1 == 'L'\) \{",
                                                              r"else if \(cmd0 == 'S' && cmd1 == 'B'\) \{"))
    handler = head + "  " + extra_branches + "\n  return !otdWriteRefused;\n}\n"
    blk = re.search(r"// --- otdPicFloat begin\n(.*?)// --- otdPicFloat end", SRC, re.S)
    old = re.search(r"// --- otdPicPercentF88 begin\n(.*?)// --- otdPicPercentF88 end", SRC, re.S)
    helpers = blk.group(1) if blk else (old.group(1) if old else "")
    if "static bool otdPicInBounds(" in SRC:
        helpers += "\n" + pf.braced(SRC, r"^static bool otdPicInBounds\(")
    f88 = pf.braced(SRC, r"^static inline uint16_t floatToF88\(float celsius\) \{")
    src = pf.PRE + EXTRA + f88 + "\n" + helpers + "\n" + handler + MAIN
    _tmp = Path(tempfile.mkdtemp(prefix="otd_bare_"))
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


class BareCommands(unittest.TestCase):
    def test_bare_value_is_se_where_the_pic_parser_refuses_it(self):
        cmds = ("CS=", "C2=", "BS=", "CC=", "RT=", "TT=", "TC=", "SB=", "SW=", "SH=")
        r = run(*cmds)
        for c in cmds:
            self.assertEqual((r[c]["st"], r[c]["enq"], r[c]["clr"], r[c]["set"], r[c]["rmode"]),
                             ("SE", "-1", "-1", "-1", "0"), c)

    def test_bare_value_clears_where_the_pic_clears(self):
        r = run("OT=", "MM=", "CL=")
        for c, mid in (("OT=", "27"), ("MM=", "14"), ("CL=", "7")):
            self.assertEqual((r[c]["clr"], r[c]["rsp"], r[c]["enq"]), (mid, "-", "-1"), c)

    def test_a_value_still_works_through_the_entry(self):
        r = run("CS=49.72", "OT=-1.4", "MM=50")
        self.assertEqual((r["CS=49.72"]["enq"], r["CS=49.72"]["rsp"]), ("1", "49.72"))
        self.assertEqual(r["OT=-1.4"]["rsp"], "-1.40")
        self.assertEqual(r["MM=50"]["rsp"], "50")

    def test_other_bare_commands_stay_unanswered(self):
        r = run("GW=", "XX=", "C")
        for c in ("GW=", "XX=", "C"):
            self.assertEqual((r[c]["ok"], r[c]["rsp"], r[c]["st"]), ("0", "", ""), c)


if __name__ == "__main__":
    unittest.main(verbosity=2)
