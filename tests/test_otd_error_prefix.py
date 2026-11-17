#!/usr/bin/env python3
"""TASK-1804 B3a: an OT-Direct command error after a valid header is printed as "XX: <code>", like the PIC.

PIC 6.6 (other-projects/otgw-6.6/gateway.asm): SerialCmdSub answers a command shorter than 4 characters (the CR
counts) or one whose third character is not '=' with a bare SyntaxError (:3686-3694); after those checks it prints
the two command letters, ':' and ' ' (:3695-3703) and only then runs the command, whose error code is printed after
that prefix (:1425-1431). So "CH=x" answers "CH: BV" and "C" answers "SE". This is wire parity with the PIC (pyotgw
handles both forms, commandprocessor.py:54-58 and :66-71). Before the fix OT-Direct printed every code bare ("BV").

The real code is compiled with a host C++ compiler: otdCmdPrefix, OtdCmdPrefixScope, otdIsCmdErrorCode and
otDirectBridgeProcessStatus() from src/OTGW-firmware/OTDirect.ino, and the header part of handleOTDirectCommand() up
to its bare-value check. A test tail then reports the error code a command branch would report. On code without the
fix only otDirectBridgeProcessStatus() exists and the cases go red. OTGW_OTD_INO points at another copy for
old-vs-fix.
Run: python tests/test_otd_error_prefix.py
"""
import os
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


def body_end(src, start):
    j = src.index("{", start) + 1
    depth = 1
    while depth and j < len(src):
        depth += {"{": 1, "}": -1}.get(src[j], 0)
        j += 1
    return j


def status_block():
    fn = SRC.find("static void otDirectBridgeProcessStatus(const char* status)")
    if fn < 0:
        return ""
    start = SRC.find("static char otdCmdPrefix")
    if start < 0 or start > fn:
        start = fn
    return SRC[start:body_end(SRC, fn)]


def handler_head():
    i = SRC.find("bool handleOTDirectCommand(const char* buf, int len, bool satOwn) {")
    k = SRC.find("  if (len == 3 && !otdPicBareValueCmd(cmd0, cmd1)) return false;", i)
    if i < 0 or k < 0:
        return ""
    return SRC[i:k]


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define OTDDebugTf(...) ((void)0)
static bool otdWriteRefused = false;
struct DiagRec { bool active = false; void recOtdCommand(const char*, size_t) {} };
static DiagRec diagRec;
static char g_lines[512] = "";
static void addLine(const char* tag, const char* s, size_t n) {
  size_t l = strlen(g_lines);
  snprintf(g_lines + l, sizeof(g_lines) - l, "%s[%.*s]", tag, (int)n, s);
}
static void otDirectBridgeWriteLine(const char* line, size_t len) { addLine("B", line, len); }
static void processOT(const char* buf, int len) {
  if (strlen(buf) != (size_t)len) addLine("NOTERM", buf, (size_t)len);   // processOT logs buf as a C string
  addLine("P", buf, (size_t)len);
}
static const char* g_code = "BV";
"""

TAIL = r"""
  otDirectBridgeProcessStatus(g_code);   // what a command branch reports for a bad value
  (void)satOwn;
  return false;
}
int main(int argc, char** argv) {
  g_code = argv[2];
  handleOTDirectCommand(argv[1], (int)strlen(argv[1]), false);
  printf("%s|", g_lines);
  g_lines[0] = 0;
  otDirectBridgeProcessStatus("BV");   // after the command: no prefix left behind
  printf("%s\n", g_lines);
  return 0;
}
"""


class OtdErrorPrefix(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        block, head = status_block(), handler_head()
        if not block or not head:
            cls.err = "otDirectBridgeProcessStatus() or the handleOTDirectCommand() header not found"
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + block + "\n" + head + TAIL, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))
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

    def run_cmd(self, cmd, code="BV"):
        out = subprocess.run([str(self.exe), cmd, code], capture_output=True, text=True, check=True).stdout.strip()
        during, after = out.split("|", 1)
        return during, after

    def test_error_after_valid_header_is_prefixed(self):
        for cmd, code, line in (("CH=x", "BV", "CH: BV"), ("XY=1", "NG", "XY: NG"), ("TT=99", "OR", "TT: OR"),
                                ("CH=", "SE", "CH: SE"), ("UI=5", "NF", "UI: NF"), ("AA=5", "NS", "AA: NS")):
            with self.subTest(cmd=cmd):
                during, _ = self.run_cmd(cmd, code)
                self.assertEqual(during, "B[%s]P[%s]" % (line, line))

    def test_header_failures_stay_bare_se(self):
        for cmd in ("C", "CH", "CHX", "CH:1"):
            with self.subTest(cmd=cmd):
                during, _ = self.run_cmd(cmd)
                self.assertEqual(during, "B[SE]P[SE]")

    def test_non_error_code_stays_bare(self):
        during, _ = self.run_cmd("CH=1", "OE")
        self.assertEqual(during, "B[OE]P[OE]")

    def test_prefix_does_not_outlive_the_command(self):
        for cmd in ("CH=x", "C"):
            with self.subTest(cmd=cmd):
                _, after = self.run_cmd(cmd)
                self.assertEqual(after, "B[BV]P[BV]")


if __name__ == "__main__":
    unittest.main(verbosity=2)
