#!/usr/bin/env python3
"""TASK-1839 (PIC 6.8 parity audit N7): GW= for the mode already running changes no state, as on the PIC.

The PIC's SetGatewayMode only switches the comparators and saves the mode (gateway.asm 6.8 :4729-4747,
:4749-4774), so CS=, MM=, HW=, BW=, SR=, UI= and TT=/TC= stay. OT-Direct reset its transient state on every GW=
(setOTDirectMode -> applyOTDirectMode -> resetTransientState), also for the mode it was already in.

The real setOTDirectMode() and applyOTDirectMode() are cut from src/OTGW-firmware/OTDirect.ino and compiled with a
host C++ compiler; resetTransientState() is a counter, the rest is stubbed. OTGW_OTD_INO points at another copy for
old-vs-fix. What a real mode change keeps (AC2) is Robert's decision and not tested here.
Run: python tests/test_otd_gw_same_mode.py
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


def func(header):
    i = SRC.find(header)
    if i < 0:
        return ""
    return SRC[i:SRC.find("\n}\n", i) + 3]


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(s) (s)
#define PSTR(s) (s)
#define HAS_BYPASS_RELAY 0
#define OTDDebugTln(s) ((void)0)
#define snprintf_P snprintf
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
static int g_resets = 0, g_settingWrites = 0;
static char g_lastMode[8] = "";
static void resetTransientState() { g_resets++; }
static void sendWebSocketJSON(const char*) {}
static bool updateSetting(const char*, const char* v) { g_settingWrites++; strncpy(g_lastMode, v, sizeof(g_lastMode) - 1); return true; }
struct MockOT { void begin(void (*)(), void (*)(unsigned long, int)) {} void end() {} };
static MockOT otSlave;
static void slaveISR() {}
static void handleSlaveRequest(unsigned long, int) {}
struct OtdSettings { bool bEnableSlave; };
struct Settings { OtdSettings otd; };
static Settings settings = { { true } };
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: running mode, then each GW= mode in order
  otCurrentMode = (OTDirectMode)atoi(argv[1]);
  for (int i = 2; i < argc; i++) setOTDirectMode((OTDirectMode)atoi(argv[i]));
  printf("%d %d %d %s\n", g_resets, (int)otCurrentMode, g_settingWrites, g_lastMode);
  return 0;
}
"""


class GwSameMode(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        parts = [func("static void applyOTDirectMode(OTDirectMode newMode) {"), func("static void setOTDirectMode(OTDirectMode newMode) {")]
        if not all(parts):
            cls.err = "applyOTDirectMode / setOTDirectMode not found"
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + "\n".join(parts) + MAIN, encoding="utf-8")
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

    def run_gw(self, running, *modes):
        out = subprocess.run([str(self.exe), str(running)] + [str(m) for m in modes],
                             capture_output=True, text=True, check=True).stdout.split()
        return int(out[0]), int(out[1]), int(out[2]), (out[3] if len(out) > 3 else "")

    def test_gw1_in_gateway_mode_keeps_the_state(self):
        resets, mode, writes, saved = self.run_gw(1, 1)
        self.assertEqual((resets, mode), (0, 1), "GW=1 while in gateway mode must not reset the command state")
        self.assertEqual((writes, saved), (1, "1"), "the mode setting is still written")

    def test_gw0_in_monitor_mode_keeps_the_state(self):
        self.assertEqual(self.run_gw(2, 2)[:2], (0, 2))

    def test_a_real_mode_change_still_applies(self):
        self.assertEqual(self.run_gw(1, 2)[:2], (1, 2))       # gateway -> monitor resets, as before

    def test_repeated_gw1_never_resets(self):
        self.assertEqual(self.run_gw(1, 1, 1, 1)[:2], (0, 1))


if __name__ == "__main__":
    unittest.main(verbosity=2)
