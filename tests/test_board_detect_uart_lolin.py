#!/usr/bin/env python3
"""TASK-1790: in Auto board mode a missed boot PIC probe on a LOLIN module (a Classic) must leave the PIC UART open.

Before the fix the Auto no-PIC branch of setup() called OTGWSerial.end() before it asked isOtgw32HardwarePossible().
On a LOLIN module (PSRAM, or the S3 Mini Pro IMU) OT-Direct is not started (TASK-1730), the mode stays HW_MODE_DEGRADED,
and nothing opens the UART again (begin() only on the boot paths). The per-minute PR=A retry (doTaskEvery60s) then
writes to a closed UART, no banner can come back, and a PIC that comes up late is never found until the next reboot.
The ADR-125 reason to close the UART (a floating PIC RX pin storming interrupts) applies only to the OTGW32 module,
where no PIC is wired: that branch must still close it before OT-Direct starts.

REAL code, cut from src/OTGW-firmware/OTGW-firmware.ino (the board resolution block, the same cut as the ADR-201 gate
tests/test_board_detect_persist_gate.py) plus the real isOtgw32HardwarePossible() from OTGW-firmware.h. STUBBED: the PIC
serial object (tracks open/closed), detectPIC() (scripted verdict), platformHasPsram() (scripted), initOTDirect(),
writeSettings(), logging, delay().
Old-vs-fix: OTGW_INO=<older OTGW-firmware.ino> python tests/test_board_detect_uart_lolin.py
Run: python tests/test_board_detect_uart_lolin.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp)
import test_board_detect_persist_gate as gate  # noqa: E402  (START_MARK / END_MARK / load / FWH)

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define HAS_RUNTIME_HW_DETECT 1
#define F(x) x
#define PSTR(x) x
#define SetupDebugf(...) ((void)0)
#define SetupDebugln(x) ((void)0)
#define DebugTln(x) ((void)0)
#define PIN_PIC_RX 1
#define PIN_PIC_TX 2
#define SERIAL_8N1 0
struct HwState { bool bClassicPro = false; int eMode = 0; };
struct PicState { bool bAvailable = false; };
struct State { HwState hw; PicState pic; };
struct Settings { int iBoardMode = 0; };
static State state; static Settings settings;
static bool g_pic = false, g_psram = false;
static int g_otdirect = 0;
static void detectPIC() {}
static bool isPICEnabled() { return g_pic; }
static bool isOTDirectEnabled() { return false; }
static void initOTDirect() { g_otdirect++; }
static bool platformHasPsram() { return g_psram; }
static void delay(int) {}
static int activePicRst() { return 0; }
static int activeLed2() { return 0; }
static void writeSettings(bool) {}
struct SerialStub { bool open = false; void begin(int, int, int, int) { open = true; } void setResetPin(int) {}
                    void setProgressLed(int) {} void end() { open = false; } };
static SerialStub OTGWSerial;
"""

MAIN = r"""
int main(int argc, char **argv) {
  settings.iBoardMode = atoi(argv[1]); g_pic = atoi(argv[2]) != 0; g_psram = atoi(argv[3]) != 0;
  state.hw.bClassicPro = atoi(argv[4]) != 0;
  resolveBoard();
  printf("uart=%d otdirect=%d mode=%d\n", OTGWSerial.open ? 1 : 0, g_otdirect, settings.iBoardMode);
  return 0;
}
"""


def build_source():
    src = gate.load(gate.INO)
    a = src.index(gate.START_MARK)
    b = src.index(gate.END_MARK, a)
    m = re.search(r"inline bool isOtgw32HardwarePossible\(\) \{\n.*?\n\}\n", gate.load(gate.FWH), re.S)
    assert m, "isOtgw32HardwarePossible() not found in OTGW-firmware.h"
    return STUBS + m.group(0) + "static void resolveBoard() {\n" + src[a:b] + "}\n" + MAIN


class AutoNoPicUart(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, mode, pic, psram, pro):
        out = subprocess.run([str(self.exe), str(mode), str(pic), str(psram), str(pro)],
                             capture_output=True, text=True, check=True).stdout
        return dict((k, int(v)) for k, v in (kv.split("=") for kv in out.split()))

    def test_lolin_with_psram_keeps_the_pic_uart_open(self):
        r = self.run_case(0, 0, 1, 0)
        self.assertEqual((r["uart"], r["otdirect"], r["mode"]), (1, 0, 0),
                         "Auto, no PIC answer, PSRAM module: the UART must stay open for the PR=A retry")

    def test_s3_mini_pro_keeps_the_pic_uart_open(self):
        r = self.run_case(0, 0, 0, 1)
        self.assertEqual((r["uart"], r["otdirect"]), (1, 0))

    def test_otgw32_still_closes_the_uart_before_otdirect(self):
        r = self.run_case(0, 0, 0, 0)
        self.assertEqual((r["uart"], r["otdirect"], r["mode"]), (0, 1, 0), "ADR-125: OTGW32 tears down UART0 first")

    def test_pic_found_keeps_it_open(self):
        r = self.run_case(0, 1, 1, 0)
        self.assertEqual((r["uart"], r["otdirect"]), (1, 0))

    def test_forced_otdirect_without_pic_still_closes(self):
        r = self.run_case(2, 0, 0, 0)
        self.assertEqual((r["uart"], r["otdirect"]), (0, 1))


if __name__ == "__main__":
    unittest.main(verbosity=2)
