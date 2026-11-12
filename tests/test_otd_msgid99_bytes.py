#!/usr/bin/env python3
"""TASK-1832: OT-Direct writes MsgID 99 (Remote Override Operating Mode) with the bytes in the right place, like the PIC.

Ground truth (three sources agree):
  - OpenTherm spec v4.2 (docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:2249-2289): MsgID 99
    LB = operating mode HC1 (bits 0..3) and HC2 (bits 4..7); HB = operating mode DHW (bits 0..3), bit 4 = manual DHW push.
  - PIC 6.8 gateway.asm (OTGW-other-projects/otgw-6.8): OperModeData puts operatingmode1 (DHW, push) in byte 3 = HB and
    operatingmode2 (HC1, HC2) in byte 4 = LB (gateway.asm:3247-3250); SetOperModeDHW keeps the DHW mode in the low
    nibble of operatingmode1 (gateway.asm:4482-4494).
  - PIC test scripts: tests/dhw-5.stc HW=P -> R10631000 (data 0x1000: push bit = HB bit 4).
OT-Direct before the fix built `DHW | (HC1 | HC2 << 4) << 8`: DHW and the push in LB, heating in HB.

The code under test is REAL: the `MH`, `MW`, `M2` and `HW` branches of handleOTDirectCommand() are cut from
src/OTGW-firmware/OTDirect.ino (plus the helper they call, when present) and compiled with doubles for the queue and the
frame builder; the frames they enqueue are checked.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_msgid99_bytes.py
Run: python tests/test_otd_msgid99_bytes.py
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

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
#define OTDDebugTf(...) do {} while (0)
#define OTDDebugTln(...) do {} while (0)
enum class OpenThermMessageType { READ_DATA = 0, WRITE_DATA = 1 };
typedef uint8_t OpenThermMessageID;
struct OpenTherm {
  static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, uint16_t data) {
    return ((unsigned long)(int)t << 28) | ((unsigned long)id << 16) | data;
  }
};
static unsigned long g_frames[16]; static int g_nframes = 0;
static bool otCmdEnqueue(unsigned long f) { g_frames[g_nframes++] = f; return true; }
static unsigned long millis() { return 1000; }
static void otDirectBridgeProcessStatus(const char* s) { printf("STATUS %s\n", s); }
static void synthesizeResponse(const char* c, const char* v) { printf("REPLY %.2s: %s\n", c, v); }
enum { PUSH_IDLE = 0, PUSH_PENDING = 1 };
static int otDHWPushState = PUSH_IDLE;
static unsigned long otDHWPushStartMs = 0;
static uint8_t otMasterStatusFlags = 0;
static uint8_t otDHWOverride = 0xFF;
static uint8_t otOperModeDHW = 0, otOperModeCH1 = 0, otOperModeCH2 = 0;
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: pairs of command and value, applied in order; every enqueued MsgID 99 frame is printed
  for (int i = 1; i + 1 < argc; i += 2) {
    char buf[8]; snprintf(buf, sizeof buf, "%s=%s", argv[i], argv[i + 1]);
    g_nframes = 0;
    cmd(buf, argv[i + 1]);
    for (int k = 0; k < g_nframes; k++)
      if (((g_frames[k] >> 16) & 0xFF) == 99) printf("FRAME %s=%s %08lX\n", argv[i], argv[i + 1], g_frames[k]);
  }
  return 0;
}
"""


def branch(src: str, a: str, b: str) -> str:
    m = re.search(r"else if \(cmd0 == '%s' && cmd1 == '%s'\) \{" % (a, b), src)
    if not m:
        raise AssertionError(f"{a}{b}= branch not found in {OTD_INO}")
    i, depth = m.end() - 1, 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced branch")


def helper(src: str) -> str:
    """The MsgID 99 data helper the fixed branches call (absent in the code before TASK-1832)."""
    m = re.search(r"^static uint16_t otdOperModeData\(bool push\) \{", src, re.M)
    if not m:
        return ""
    end = src.index("\n}\n", m.start())
    return src[m.start():end + 3]


def build_source() -> str:
    src = OTD_INO.read_text(encoding="utf-8")
    body = "".join(branch(src, a, b) + "\n  " for a, b in (("H", "W"), ("M", "H"), ("M", "W"), ("M", "2")))
    fn = ("static bool cmd(const char* buf, const char* value) {\n  const char cmd0 = buf[0], cmd1 = buf[1];\n"
          "  char rspBuf[16];\n  if (false) {}\n  " + body + "\n  return true;\n}\n")
    return STUBS + helper(src) + fn + MAIN


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="otd_99_"))
    cpp = _tmp / "otd_99.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("otd_99.exe" if os.name == "nt" else "otd_99"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def frames(*args) -> list:
    out = subprocess.run([str(_exe), *args], capture_output=True, text=True, check=True).stdout
    return [int(line.split()[2], 16) & 0xFFFF for line in out.splitlines() if line.startswith("FRAME")]


class MsgId99Bytes(unittest.TestCase):
    def test_mw_puts_dhw_mode_in_the_high_byte(self):
        self.assertEqual(frames("MW", "4"), [0x0400])

    def test_mh_puts_hc1_in_the_low_nibble_of_the_low_byte(self):
        self.assertEqual(frames("MH", "2"), [0x0002])

    def test_m2_puts_hc2_in_the_high_nibble_of_the_low_byte(self):
        self.assertEqual(frames("M2", "3"), [0x0030])

    def test_hw_p_sets_the_push_bit_hb_bit4_as_pic_dhw5(self):
        self.assertEqual(frames("HW", "P"), [0x1000])   # tests/dhw-5.stc: R10631000

    def test_modes_combine_and_keep_their_bytes(self):
        self.assertEqual(frames("MH", "2", "M2", "3", "MW", "4"), [0x0002, 0x0032, 0x0432])

    def test_push_keeps_the_dhw_and_heating_modes(self):
        self.assertEqual(frames("MW", "1", "MH", "4", "HW", "P")[-1], 0x1104)


if __name__ == "__main__":
    unittest.main(verbosity=2)
