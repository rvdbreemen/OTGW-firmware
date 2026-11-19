#!/usr/bin/env python3
"""TASK-1908: OT-Direct SC= parses and refuses like the PIC 6.8 SetClock.

PIC ground truth (OTGW-other-projects/otgw-6.8 gateway.asm, repo 770aa45), SetClock :4331-4370: the hours by
GetDecimalArg (:5044-5069: no digit first SE, above 255 OR), above 23 OR; then ':' or SE; the minutes by GetDecimal
(any error SE), above 59 OR; then '/' or SE; the day of the week is one character '1'-'7' (else BV, also when nothing
follows the '/'); a character after it SE. Only then are the clock bytes stored.

OT-Direct read "%d:%d/%d" with siscanf() >= 2: SC=12:30 without a day, SC=24:00/1 and SC=12:30/0 were stored (the
bits masked) and SC=12:30/3x was accepted.

The code under test is REAL: the SC= branch of handleOTDirectCommand(), cut from src/OTGW-firmware/OTDirect.ino; the
doubles (reply and status capture) come from tests/test_otd_pic_float.py.
Old-vs-fix: OTGW_OTD_INO=<old OTDirect.ino> python tests/test_otd_sc_clock_pic.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_otd_pic_float as pf  # noqa: E402  (shares OTGW_OTD_INO and the reply doubles)

EXTRA = r"""
#ifndef siscanf
#define siscanf sscanf
#endif
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static uint16_t otSCClockData = 0;
static uint32_t otSCLastCommandMs = 0;
"""

MAIN = r"""
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1]; char rspBuf[16];
  if (false) {}
  @BRANCH@
  return true;
}
int main(int argc, char** argv) {
  for (int a = 1; a < argc; a++) {
    g_rsp[0] = 0; g_status[0] = 0; otSCClockData = 0; otSCLastCommandMs = 0;
    char buf[32]; snprintf(buf, sizeof buf, "SC=%s", argv[a]);
    cmd(buf, buf + 3);
    printf("%s|rsp=%s|st=%s|data=%04X|stamp=%u\n", argv[a], g_rsp, g_status, otSCClockData, (unsigned)otSCLastCommandMs);
  }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    branch = pf.braced(pf.SRC, r"else if \(cmd0 == 'S' && cmd1 == 'C'\) \{")
    src = pf.PRE + EXTRA + MAIN.replace("@BRANCH@", branch)
    _tmp = Path(tempfile.mkdtemp(prefix="otd_sc_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(src, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*vals):
    out = subprocess.run([str(_exe)] + list(vals), capture_output=True, text=True, check=True).stdout.splitlines()
    rows = {}
    for line in out:
        head, *fields = line.split("|")
        rows[head] = dict(f.split("=", 1) for f in fields)
    return rows


class SetClockLikeThePic(unittest.TestCase):
    def test_ac1_refused_like_the_pic_and_nothing_stored(self):
        cases = {"24:00/1": "OR", "12:60/1": "OR", "12:30": "SE", "12:30/0": "BV", "12:30/3x": "SE",
                 "12:30/": "BV", "12:30/8": "BV", "x2:30/3": "SE", "12-30/3": "SE", "12:/3": "SE", "256:00/1": "OR"}
        r = run(*cases)
        for v, want in cases.items():
            self.assertEqual((r[v]["st"], r[v]["rsp"], r[v]["data"], r[v]["stamp"]), (want, "", "0000", "0"), v)

    def test_ac2_valid_value_stored_and_answered_as_before(self):
        r = run("12:30/3", "9:05/7", "0:00/1")
        self.assertEqual((r["12:30/3"]["rsp"], r["12:30/3"]["data"], r["12:30/3"]["st"]), ("12:30/3", "6C1E", ""))
        self.assertEqual((r["9:05/7"]["rsp"], r["9:05/7"]["data"]), ("09:05/7", "E905"))
        self.assertEqual((r["0:00/1"]["rsp"], r["0:00/1"]["data"]), ("00:00/1", "2000"))
        self.assertEqual(r["12:30/3"]["stamp"], "1000")


if __name__ == "__main__":
    unittest.main(verbosity=2)
