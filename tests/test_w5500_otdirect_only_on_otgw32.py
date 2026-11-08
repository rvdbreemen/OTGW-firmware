#!/usr/bin/env python3
"""TASK-1730: on the combo, the W5500 probe and OT-Direct only run on a board that can be an OTGW32.

Maintainer rule (Robert, 2026-11-08): the W5500 may only be touched on confirmed OTGW32
hardware, never in PIC mode. The W5500 SPI probe clocks GPIO12, which on an OTGW Classic is
the PIC reset line (boards.h combo pin table); crashevans's beta.77 Classic with a PIC that was
not detected went into a restart loop with a task watchdog in w5500_tsk. Robert's condition:
initEthernet() only when board mode is not 1/3, no PIC is enabled, eMode is HW_MODE_OT_DIRECT and
no S3 Mini Pro IMU was found; initOTDirect() in the auto path only under the same condition
without the eMode term. Robert added a PSRAM term after crashevans's own transcript showed a plain
S3 Mini Classic in auto mode (psram_found 1, ESP32-S3FH4R2, PIC not detected) running as OT-Direct:
the LOLIN modules have PSRAM, the OTGW32 module has none (TASK-1221 bench).

The code under test is NOT re-implemented here. The test cuts from the real sources and compiles
them with a host C++ compiler (g++, clang++ or MSVC cl):
  1. src/OTGW-firmware/OTGW-firmware.ino: the combo board resolution in setup() (same markers as
     tests/test_board_detect_persist_gate.py) and the statement that calls initEthernet().
  2. src/OTGW-firmware/OTGW-firmware.h: isPICEnabled() and, when present, isOtgw32HardwarePossible()
     and isConfirmedOtgw32().
Stubs only replace hardware: platformHasPsram() reports a scripted value; detectPIC() reports a scripted PIC verdict and sets eMode to PIC or
DEGRADED as the real one does; initOTDirect() sets HW_MODE_OT_DIRECT when no PIC is enabled, as
OTDirect.ino does; initEthernet() and writeSettings() only count. Without a compiler the tests are
skipped, they do not pass silently.

OTGW_INO and OTGW_FIRMWARE_H point at other copies of the sources (old-vs-fix runs).

Run: python tests/test_w5500_otdirect_only_on_otgw32.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

REPO = Path(__file__).resolve().parent.parent
INO = Path(os.environ.get("OTGW_INO", REPO / "src" / "OTGW-firmware" / "OTGW-firmware.ino"))
FWH = Path(os.environ.get("OTGW_FIRMWARE_H", REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h"))

START_MARK = "  if (settings.iBoardMode == 2) {"
END_MARK = "  // The resolved mode may select the other pin map"

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define PSTR(x) x
#define SetupDebugf(...) ((void)0)
#define SetupDebugln(x) ((void)0)
#define DebugTln(x) ((void)0)
#define HAS_PIC 1
#define HAS_DIRECT_OT 1
#define HAS_RUNTIME_HW_DETECT 1
#define PIN_PIC_RX 1
#define PIN_PIC_TX 2
#define SERIAL_8N1 0
enum { HW_MODE_UNKNOWN = 0, HW_MODE_PIC = 1, HW_MODE_OT_DIRECT = 2, HW_MODE_DEGRADED = 3 };
struct HwState { bool bClassicPro = false; int eMode = HW_MODE_UNKNOWN; };
struct PicState { bool bAvailable = false; };
struct State { HwState hw; PicState pic; };
struct Settings { int iBoardMode = 0; };
static State state; static Settings settings;
static bool g_pic = false;                  // scripted verdict of the PIC probe
static bool g_psram = false;                // scripted PSRAM presence
static bool platformHasPsram() { return g_psram; }
static int g_otdirect = 0, g_eth = 0;
static void detectPIC() { state.pic.bAvailable = g_pic; state.hw.eMode = g_pic ? HW_MODE_PIC : HW_MODE_DEGRADED; }
static void initOTDirect() { g_otdirect++; if (!state.pic.bAvailable) state.hw.eMode = HW_MODE_OT_DIRECT; }
static void initEthernet() { g_eth++; }
static void writeSettings(bool) {}
static void delay(int) {}
static int activePicRst() { return 0; }
static int activeLed2() { return 0; }
struct SerialStub { void begin(int, int, int, int) {} void setResetPin(int) {} void setProgressLed(int) {} void end() {} };
static SerialStub OTGWSerial;
"""

MAIN = r"""
int main(int argc, char** argv) {
  settings.iBoardMode = atoi(argv[1]);
  g_pic = atoi(argv[2]) != 0;
  state.hw.bClassicPro = atoi(argv[3]) != 0;
  g_psram = argc > 4 && atoi(argv[4]) != 0;
  resolve();
  ethGate();
  printf("%d %d %d\n", g_otdirect, g_eth, state.hw.eMode);
  return 0;
}
"""


def cut_resolve(src: str) -> str:
    a = src.find(START_MARK)
    b = src.find(END_MARK, a)
    if a < 0 or b < 0:
        raise AssertionError("board-resolution markers not found in OTGW-firmware.ino; update the test")
    return "static void resolve() {\n" + src[a:b] + "}\n"


def cut_eth_gate(src: str) -> str:
    lines = [ln for ln in src.splitlines() if "initEthernet();" in ln and not ln.lstrip().startswith("//")]
    if len(lines) != 1:
        raise AssertionError("expected exactly one initEthernet() call in OTGW-firmware.ino, found %d" % len(lines))
    return "static void ethGate() {\n" + lines[0] + "\n}\n"


def cut_inline(src: str, name: str) -> str:
    m = re.search(r"inline bool " + name + r"\(\) \{\n.*?\n\}\n", src, re.S)
    return m.group(0) if m else ""


def build(workdir: Path, ino: str, fwh: str, tag: str):
    helpers = "".join(cut_inline(fwh, n) for n in ("isPICEnabled", "isOtgw32HardwarePossible", "isConfirmedOtgw32"))
    cpp = workdir / (tag + ".cpp")
    cpp.write_text(STUBS + helpers + cut_resolve(ino) + cut_eth_gate(ino) + MAIN, encoding="utf-8")
    return base.compile_cpp(cpp, workdir / (tag + (".exe" if os.name == "nt" else "")))


def run(exe, board_mode: int, pic: int, pro: int, psram: int = 0):
    out = subprocess.run([str(exe), str(board_mode), str(pic), str(pro), str(psram)],
                         capture_output=True, text=True, check=True).stdout.split()
    return {"otdirect": int(out[0]), "eth": int(out[1]), "mode": int(out[2])}


class W5500OtDirectGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name), INO.read_text(encoding="utf-8"),
                        FWH.read_text(encoding="utf-8"), "w5500_gate")
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler (g++, clang++ or MSVC)")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_classic_mode_with_dead_pic_never_probes_w5500(self):
        # crashevans: a Classic (mode 1 S3 Mini / 3 S3 Mini Pro) whose PIC is not detected
        for mode, pro in ((1, 0), (3, 1), (1, 1), (3, 0)):
            r = run(self.exe, mode, 0, pro, 1)
            self.assertEqual((r["eth"], r["otdirect"]), (0, 0), (mode, pro, r))

    def test_auto_s3_mini_pro_with_dead_pic_runs_neither(self):
        r = run(self.exe, 0, 0, 1, 1)
        self.assertEqual((r["otdirect"], r["eth"]), (0, 0), r)

    def test_live_pic_never_probes_w5500(self):
        for mode in (0, 1, 2, 3):
            for pro in (0, 1):
                r = run(self.exe, mode, 1, pro, 1)
                self.assertEqual(r["eth"], 0, (mode, pro, r))

    def test_otgw32_keeps_otdirect_and_ethernet(self):
        for mode in (0, 2):   # auto without PIC, forced OT-Direct
            r = run(self.exe, mode, 0, 0)
            self.assertEqual((r["otdirect"], r["eth"], r["mode"]), (1, 1, 2), (mode, r))

    def test_crashevans_plain_s3_mini_auto_dead_pic(self):
        # board mode 0, PIC not detected, no Pro IMU, PSRAM present: a Classic, not an OTGW32
        r = run(self.exe, 0, 0, 0, 1)
        self.assertEqual((r["otdirect"], r["eth"]), (0, 0), r)

    def test_forced_otdirect_on_lolin_skips_w5500(self):
        # board mode 2 forced by hand on a Classic: OT-Direct as asked, but never the W5500 probe
        r = run(self.exe, 2, 0, 0, 1)
        self.assertEqual(r["eth"], 0, r)


if __name__ == "__main__":
    unittest.main(verbosity=2)
