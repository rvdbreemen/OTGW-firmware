#!/usr/bin/env python3
"""TASK-1849: OT-Direct success replies of SC=, SR= and FT= use the PIC 6.8 text, byte for byte.

PIC ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45):
  - SC= (SetClock :4331-4388): hours through PrintDecimal (:2086-2101, leading zero kept), ':' and the minutes
    through PrintFraction, '/' and the day digit: "SC: 09:05/3";
  - SR= (UpdateResponse :4846-4859): PrintByte of the id, ':', PrintBytes high '/' low: "SR: 18:1/230",
    whatever form the argument had;
  - FT= (SetTStatModel :4705-4727): C, I and S are set by hand, any other letter is auto-detect, and the reply is
    PrintRemeha (:2270-2280) of the result: C, I, S or D (R only after a detection, which OT-Direct has not).

The code under test is REAL: the SC=, SR= and FT= branches of handleOTDirectCommand() are cut from
src/OTGW-firmware/OTDirect.ino (plus the otdPrHelpers block when present) and compiled with doubles for the
state they touch.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_reply_texts_pic.py
Run: python tests/test_otd_reply_texts_pic.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))


def braced(src, start_pat):
    m = re.search(start_pat, src)
    if not m:
        raise AssertionError("branch not found: " + start_pat)
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced branch: " + start_pat)


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define siscanf sscanf
#define F(s) (s)
#define OTDDebugTln(...) do {} while (0)
#define OTDDebugTf(...) do {} while (0)
static uint32_t millis() { return 1000; }
static char g_rsp[64];
static void synthesizeResponse(const char* buf, const char* rsp) { snprintf(g_rsp, sizeof g_rsp, "%s: %s", buf, rsp); }
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_rsp, sizeof g_rsp, "%s", s); }
static uint32_t otSCLastCommandMs = 0;
static uint16_t otSCClockData = 0;
#define OT_RESPONSE_OVERRIDE_MAX 8
struct { bool active; uint8_t msgId; uint16_t value; } otResponseOverrides[OT_RESPONSE_OVERRIDE_MAX] = {};
static char otForceThermostat = 'D';
"""

MAIN = r"""
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  char rspBuf[16];
  g_rsp[0] = 0;
  if (false) {}
  @BRANCHES@
  return true;
}
int main(int argc, char** argv) {
  // argv: pairs "XX" "value"
  for (int a = 1; a + 1 < argc; a += 2) { cmd(argv[a], argv[a + 1]); printf("%s\n", g_rsp); }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    src = OTD_INO.read_text(encoding="utf-8")
    branches = "\n  ".join(braced(src, p) for p in (
        r"else if \(cmd0 == 'S' && cmd1 == 'C'\) \{",
        r"else if \(cmd0 == 'S' && cmd1 == 'R'\) \{",
        r"else if \(cmd0 == 'F' && cmd1 == 'T'\) \{"))
    h = re.search(r"// --- otdPrHelpers begin\r?\n(.*?)// --- otdPrHelpers end", src, re.S)
    helpers = h.group(1) if h else ""
    _tmp = Path(tempfile.mkdtemp(prefix="otd_rt_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(PRE + helpers + MAIN.replace("@BRANCHES@", branches), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def ask(*pairs):
    args = [str(_exe)]
    for c, v in pairs:
        args += [c, v]
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout.splitlines()


class ReplyTexts(unittest.TestCase):
    def test_SC_two_digit_hours(self):
        self.assertEqual(ask(("SC", "9:05/3"), ("SC", "21:50/3"), ("SC", "0:00/1")),
                         ["SC: 09:05/3", "SC: 21:50/3", "SC: 00:00/1"])

    def test_SR_prints_the_stored_bytes_as_high_slash_low(self):
        self.assertEqual(ask(("SR", "18:1,230"), ("SR", "70:14"), ("SR", "70:0,14"), ("SR", "5:0"),
                             ("SR", "127:255,255")),
                         ["SR: 18:1/230", "SR: 70:0/14", "SR: 70:0/14", "SR: 5:0/0", "SR: 127:255/255"])

    def test_SR_refusals_stay(self):
        self.assertEqual(ask(("SR", "18"), ("SR", "200:1,2")), ["BV", "BV"])

    def test_FT_replies_the_selected_model(self):
        self.assertEqual(ask(("FT", "C"), ("FT", "I"), ("FT", "S"), ("FT", "D"), ("FT", "X"), ("FT", "R")),
                         ["FT: C", "FT: I", "FT: S", "FT: D", "FT: D", "FT: D"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
