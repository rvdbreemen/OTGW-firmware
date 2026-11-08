#!/usr/bin/env python3
"""TASK-1719: GET /api/v2/settings carries no OT-Direct (otd*) setting in PIC mode.

Maintainer rule (Robert, 2026-11-08): on the combo board in PIC mode (board mode 1 = Classic
S3 Mini, 3 = Classic S3 Mini Pro, or whenever a PIC is live) the web UI shows no OT-Direct
setting. The web UI builds its Settings rail from the keys the firmware sends, so the gate is
the emission in sendDeviceSettings().

The code under test is NOT re-implemented here. The test cuts the real OT-Direct block out of
src/OTGW-firmware/restAPI.ino (from `#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT` up to its
`#endif`) and the real isPICEnabled() / isOTDirectUiExposed() helpers out of OTGW-firmware.h,
and compiles them with a host C++ compiler (g++, clang++ or MSVC cl). Stubs only record the
emitted keys and provide the settings/state structs. On code without isOTDirectUiExposed()
the block is compiled as it is, so the cases fail on behaviour, not on the build. Without a
compiler the tests are skipped, they do not pass silently.

OTGW_RESTAPI_INO and OTGW_FIRMWARE_H point at other copies of the sources (old-vs-fix runs).

Run: python tests/test_otd_settings_hidden_in_pic_mode.py
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
RESTAPI = Path(os.environ.get("OTGW_RESTAPI_INO", REPO / "src" / "OTGW-firmware" / "restAPI.ino"))
FWH = Path(os.environ.get("OTGW_FIRMWARE_H", REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h"))

BLOCK_START = "#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT\n  // --- OT-Direct settings"
OTD_KEYS = 14   # otdmode .. otdventsetpoint, as emitted today on an OT-Direct board

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define HAS_PIC 1
#define HAS_DIRECT_OT 1
#define HAS_RUNTIME_HW_DETECT 1
enum { HW_MODE_UNKNOWN = 0, HW_MODE_PIC = 1, HW_MODE_OT_DIRECT = 2, HW_MODE_DEGRADED = 3 };
struct HwState { int eMode = 0; };
struct PicState { bool bAvailable = false; };
struct State { HwState hw; PicState pic; };
struct OtdSettings {
  int iMode = 0; bool bAutoDetect = false; float fSetbackTemp = 16.0f; int iSetbackTimeout = 60;
  bool bEnableSlave = false, bSummerMode = false, bFailSafe = false; int iMsgInterval = 1000;
  bool bHasBypassRelay = false, bVentEnable = false, bOpenBypass = false, bAutoBypass = false;
  bool bFreeVentEnable = false; int iVentSetpoint = 0;
};
struct Settings { int iBoardMode = 0; OtdSettings otd; };
static State state; static Settings settings;
static int g_keys = 0; static int g_otd = 0;
static void note(const char* k) { g_keys++; if (strncmp(k, "otd", 3) == 0) g_otd++; }
static void addInt(const char* k, long, const char*, long, long) { note(k); }
static void addBool(const char* k, bool, const char*) { note(k); }
static void addNum(const char* k, const char*, const char*, double, double) { note(k); }
static char* dtostrf(double v, int, int p, char* b) { sprintf(b, "%.*f", p, v); return b; }
"""

MAIN = r"""
int main(int argc, char** argv) {
  settings.iBoardMode = atoi(argv[1]);
  state.pic.bAvailable = atoi(argv[2]) != 0;
  state.hw.eMode = atoi(argv[3]);
  emitOtd();
  printf("%d %d\n", g_keys, g_otd);
  return 0;
}
"""


def cut_block(src: str) -> str:
    a = src.find(BLOCK_START)
    if a < 0:
        raise AssertionError("OT-Direct settings block not found in restAPI.ino; update the test")
    e = src.find("\n#endif", a)
    return src[a:e] + "\n#endif\n"


def cut_inline(src: str, name: str) -> str:
    m = re.search(r"inline bool " + name + r"\(\) \{\n.*?\n\}\n", src, re.S)
    return m.group(0) if m else ""


def build(workdir: Path, rest_src: str, fwh_src: str, tag: str):
    helpers = cut_inline(fwh_src, "isPICEnabled") + cut_inline(fwh_src, "isOTDirectUiExposed")
    body = "static void emitOtd() {\n" + cut_block(rest_src) + "}\n"
    cpp = workdir / (tag + ".cpp")
    cpp.write_text(STUBS + helpers + body + MAIN, encoding="utf-8")
    return base.compile_cpp(cpp, workdir / (tag + (".exe" if os.name == "nt" else "")))


def run(exe, board_mode: int, pic: int, emode: int):
    out = subprocess.run([str(exe), str(board_mode), str(pic), str(emode)],
                         capture_output=True, text=True, check=True).stdout.split()
    return int(out[0]), int(out[1])


# (board mode, live PIC, eMode) -> OT-Direct keys expected
PIC_CASES = (
    (1, 1, 1),   # Classic S3 Mini, PIC live
    (3, 1, 1),   # Classic S3 Mini Pro, PIC live
    (1, 0, 3),   # forced PIC but the PIC is dead (DEGRADED): still PIC mode
    (3, 0, 3),
    (0, 1, 1),   # auto with a live PIC, before the verdict is persisted
)
OTD_CASES = (
    (0, 0, 2),   # auto, no PIC: OT-Direct this boot
    (2, 0, 2),   # forced OT-Direct
)


class OtdSettingsGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name), RESTAPI.read_text(encoding="utf-8"),
                        FWH.read_text(encoding="utf-8"), "otd_gate")
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler (g++, clang++ or MSVC)")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_pic_mode_emits_no_otd_setting(self):
        for case in PIC_CASES:
            self.assertEqual(run(self.exe, *case), (0, 0), case)

    def test_otdirect_mode_emits_every_otd_setting(self):
        for case in OTD_CASES:
            self.assertEqual(run(self.exe, *case), (OTD_KEYS, OTD_KEYS), case)


if __name__ == "__main__":
    unittest.main(verbosity=2)
