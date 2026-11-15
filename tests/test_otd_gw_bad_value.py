#!/usr/bin/env python3
"""TASK-1946: GW= with an unsupported value answers "GW: BV" (and a value longer than one character "GW: SE") and
changes nothing, as the PIC.

PIC 6.6 (other-projects/otgw-6.6/gateway.asm): SerialCommand prints "GW: " before the command runs (:3695-3703),
SetGatewayMode needs a 4-character command (:4687-4690, SyntaxError), CheckBoolean accepts 0 and 1 (:4041-4051) and
ResetGateway answers BadValue to anything but R (:4734-4738). OT-Direct accepts the extensions M, 2/S, P and L too.
Before the fix every other value was echoed ("GW: 3") and only the first character counted (GW=10 meant gateway).

The real GW= branch of handleOTDirectCommand() is cut from src/OTGW-firmware/OTDirect.ino and compiled with a host
C++ compiler; setOTDirectMode, resetTransientState and the replies are recorded. OTGW_OTD_INO points at another copy
for old-vs-fix.
Run: python tests/test_otd_gw_bad_value.py
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
HEAD = "else if (cmd0 == 'G' && cmd1 == 'W') {"


def gw_branch():
    i = SRC.find(HEAD)
    if i < 0:
        return ""
    j = i + len(HEAD)
    depth = 1
    while depth and j < len(SRC):
        depth += {"{": 1, "}": -1}.get(SRC[j], 0)
        j += 1
    return SRC[i + len("else if (cmd0 == 'G' && cmd1 == 'W') "):j]


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(s) (s)
#define PSTR(s) (s)
#define HAS_BYPASS_RELAY 0
#define OTDDebugTln(s) ((void)0)
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
static int g_mode = -1, g_resets = 0;
static char g_reply[32] = "";
static void setOTDirectMode(OTDirectMode m) { g_mode = (int)m; }
static void resetTransientState() { g_resets++; }
static void synthesizeResponse(const char* cmd, const char* v) { snprintf(g_reply, sizeof(g_reply), "%c%c: %s", cmd[0], cmd[1], v); }
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_reply, sizeof(g_reply), "%s", s); }
static void delay(int) {}
struct MockOT { void begin(void (*)()) {} void begin(void (*)(), void (*)(unsigned long, int)) {} void end() {} };
static MockOT otMaster, otSlave;
static void masterISR() {}
static void slaveISR() {}
static void handleSlaveRequest(unsigned long, int) {}
static uint8_t otdPsValues[4];
static bool otHideReports = false, otSummaryPending = false;
static bool gw(const char* buf) {
  const char* value = buf + 3;
  char rspBuf[16];
"""

MAIN = r"""
  return true;
}
int main(int argc, char** argv) {
  bool ok = gw(argv[1]);
  printf("%d|%d|%d|%s\n", ok ? 1 : 0, g_mode, g_resets, g_reply);
  return 0;
}
"""


class GwBadValue(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        body = gw_branch()
        if not body:
            cls.err = "GW= branch not found"
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + body + MAIN, encoding="utf-8")
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

    def gw(self, cmd):
        ok, mode, resets, reply = subprocess.run([str(self.exe), cmd], capture_output=True, text=True,
                                                 check=True).stdout.strip().split("|", 3)
        return int(ok), int(mode), int(resets), reply

    def test_unsupported_values_answer_bv_and_change_nothing(self):
        for cmd in ("GW=3", "GW=9", "GW=X", "GW=r"):
            with self.subTest(cmd=cmd):
                self.assertEqual(self.gw(cmd), (0, -1, 0, "GW: BV"))

    def test_more_than_one_character_answers_se(self):
        for cmd in ("GW=10", "GW=11", "GW=RR", "GW=0 "):
            with self.subTest(cmd=cmd):
                self.assertEqual(self.gw(cmd), (0, -1, 0, "GW: SE"))

    def test_supported_values_still_work(self):
        self.assertEqual(self.gw("GW=0"), (1, 2, 0, "GW: 0"))
        self.assertEqual(self.gw("GW=M"), (1, 2, 0, "GW: M"))
        self.assertEqual(self.gw("GW=1"), (1, 1, 0, "GW: 1"))
        self.assertEqual(self.gw("GW=2"), (1, 3, 0, "GW: 2"))
        self.assertEqual(self.gw("GW=S"), (1, 3, 0, "GW: S"))
        self.assertEqual(self.gw("GW=L"), (1, 4, 0, "GW: L"))
        self.assertEqual(self.gw("GW=R"), (1, -1, 1, "GW: R"))

    def test_bypass_without_relay_still_answers_ng(self):
        self.assertEqual(self.gw("GW=P"), (0, -1, 0, "NG"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
