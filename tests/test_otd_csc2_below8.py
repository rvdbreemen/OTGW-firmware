#!/usr/bin/env python3
"""TASK-1917: on OT-Direct a CS=/C2= below 8 C stays, like the PIC; 8 and higher still expire (TASK-1834).

PIC 6.8 (OTGW-other-projects/otgw-6.8/gateway.asm): CommandExpiry masks the integer byte of the user control setpoint with
b'11111000' and releases the override only when that is non-zero (:1626-1633 CS, :1634-1639 C2). tests/control-4.stc: CS=5
is still R10010500 after 74 s. Command reference (OTGW-documentation/OTGW-Commands.md, from firmware.html): "Values below 8
are maintained without the need to be repeated"; PIC 6.5 changelog says the same.

REAL code, cut from src/OTGW-firmware/OTDirect.ino: OT_CSC2_EXPIRY_MS, otdCsc2ExpiryStamp() (absent on the old code: the
test then uses the old rule, millis() with the 0 sentinel, which is what the old CS=/C2= handlers did) and the CS/C2 expiry
statements of the 1-second status timer. STUBBED: millis() (fake clock), satOwnsControlSetpoint() (false),
clearWriteOverride() (records the MsgID), the debug sink.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_csc2_below8.py
Run: python tests/test_otd_csc2_below8.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = TESTS.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define F(x) x
#define OTDDebugTln(x) do {} while (0)
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool satOwnsControlSetpoint() { return false; }
static int g_cleared1 = 0, g_cleared8 = 0;
static void clearWriteOverride(uint8_t id) { if (id == 1) g_cleared1++; if (id == 8) g_cleared8++; }
static uint32_t otCSLastCommandMs = 0;
static uint32_t otC2LastCommandMs = 0;
"""

OLD_RULE = r"""
static uint32_t otdCsc2ExpiryStamp(uint16_t) { uint32_t n = millis(); return n == 0 ? 1 : n; }  // pre-TASK-1917 handlers
"""

MAIN = r"""
static void tick() {
  uint32_t nowExp = millis();
EXPIRY
}
// argv: f88 (hex) given as CS= and C2= at t=0, then the times (s) to sample
int main(int argc, char** argv) {
  if (argv[1][0] == 'c') { for (int i = 2; i < argc; i++) printf("%04X\n", floatToF88((float)atof(argv[i]))); return 0; }
  uint16_t f88 = (uint16_t)strtoul(argv[1], nullptr, 16);
  g_now = 0;
  otCSLastCommandMs = otdCsc2ExpiryStamp(f88);
  otC2LastCommandMs = otdCsc2ExpiryStamp(f88);
  int next = 2;
  for (uint32_t s = 1; s <= 400 && next < argc; s++) {
    g_now = s * 1000UL; tick();
    while (next < argc && (uint32_t)atoi(argv[next]) == s) {
      printf("t=%u cs=%d c2=%d\n", s, g_cleared1 == 0 ? 1 : 0, g_cleared8 == 0 ? 1 : 0);
      next++;
    }
  }
  return 0;
}
"""


def build_source():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    const = re.search(r"static constexpr uint32_t OT_CSC2_EXPIRY_MS = [^\n]*\n", src).group(0)
    m = re.search(r"static uint32_t otdCsc2ExpiryStamp\(uint16_t f88\) \{.*?\n\}\n", src, re.S)
    helper = m.group(0) if m else OLD_RULE
    a = src.index("    if (otCSLastCommandMs != 0 && !satOwnsControlSetpoint() &&")
    b = src.index("    // TASK-733, TASK-1840: the SC= time becomes invalid", a)
    conv = re.search(r"static inline uint16_t floatToF88\(float celsius\) \{.*?\n\}\n", src, re.S).group(0)
    return STUBS + conv + const + helper + MAIN.replace("EXPIRY", src[a:b])


class CsC2Below8(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def sample(self, f88, *times):
        out = subprocess.run([str(self.exe), f88, *map(str, times)], capture_output=True, text=True,
                             check=True).stdout.split("\n")
        return {int(m.group(1)): (m.group(2) == "1", m.group(3) == "1")
                for m in (re.match(r"t=(\d+) cs=(\d) c2=(\d)", l) for l in out) if m}

    def test_cs5_c2_5_never_expire(self):
        # AC1: CS=5 / C2=5 (f8.8 0x0500) still active at 200 s (control-4.stc: CS=5 after 74 s)
        r = self.sample("0500", 74, 200)
        self.assertEqual(r[74], (True, True), r)
        self.assertEqual(r[200], (True, True), r)

    def test_just_below_8_never_expires(self):
        r = self.sample("07FF", 200)   # 7.996 C: integer byte 7
        self.assertEqual(r[200], (True, True), r)

    def test_8_and_up_still_expire_as_task_1834(self):
        # AC2: 8.0 (0x0800) and 45.5 (0x2D80) active at 63 s, gone at 64 s
        for f88 in ("0800", "2D80"):
            r = self.sample(f88, 63, 64)
            self.assertEqual(r[63], (True, True), (f88, r))
            self.assertEqual(r[64], (False, False), (f88, r))

    def test_real_cs_and_c2_handlers_use_the_helper(self):
        # the handlers are not compiled here: pin that both real handlers take their stamp from the helper under test
        src = OTD_INO.read_text(encoding="utf-8")
        self.assertEqual(src.count("otCSLastCommandMs = otdCsc2ExpiryStamp(f88);"), 1)
        self.assertEqual(src.count("otC2LastCommandMs = otdCsc2ExpiryStamp(f88);"), 1)
        self.assertNotIn("otCSLastCommandMs = millis();", src)
        self.assertNotIn("otC2LastCommandMs = millis();", src)

    def test_zero_releases_before_the_helper(self):
        # CS=0 / C2=0 is the release (PIC SetCtrlSetpoint -> ClrCtrlSetPoint), never a permanent 0 C override: 0x0000 would
        # pass the below-8 test, so the zero branch must clear the override and the stamp before the helper is reached.
        # Source pin on the real handlers (they are not compiled here).
        src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        for cmd, msgid, stamp in (("'S'", 1, "otCSLastCommandMs"), ("'2'", 8, "otC2LastCommandMs")):
            m = re.search(r"cmd0 == 'C' && cmd1 == " + cmd + r"\) \{\n    float temp = atof\(value\);\n.*?"
                          r"(?:    //[^\n]*\n)*    if \(floatToF88\(temp\) == 0\) \{[^\n]*\n((?:(?!\} else \{).)*?)\} else \{(.*?)\n    \}\n", src, re.S)
            self.assertIsNotNone(m, cmd)
            zero, rest = m.group(1), m.group(2)
            self.assertIn("clearWriteOverride(%d);" % msgid, zero, cmd)
            self.assertIn(stamp + " = 0;", zero, cmd)
            self.assertNotIn("otdCsc2ExpiryStamp", zero, cmd)
            self.assertIn(stamp + " = otdCsc2ExpiryStamp(f88);", rest, cmd)

    def test_tiny_value_releases_like_zero(self):
        # TASK-1917: the release tests the f8.8 value (as the PIC tests the converted bytes, gateway.asm:4218-4226), so a
        # CS=0.001 releases instead of becoming a permanent 0 C override. Real floatToF88 compiled; release condition pinned.
        out = subprocess.run([str(self.exe), "conv", "0.001", "-0.001", "0", "0.004", "0.0039"], capture_output=True,
                             text=True, check=True).stdout.split()
        self.assertEqual(out, ["0000", "0000", "0000", "0001", "0000"], out)
        src = OTD_INO.read_text(encoding="utf-8")
        self.assertEqual(src.count("if (floatToF88(temp) == 0) {"), 2)   # the CS= and C2= release branches

    def test_negative_still_expires(self):
        # integer byte 0xFF masks to 0xF8 (non-zero) on the PIC too: -1.0 (0xFF00) and -0.5 (0xFF80) expire
        for f88 in ("FF00", "FF80"):
            r = self.sample(f88, 63, 64)
            self.assertEqual(r[63], (True, True), (f88, r))
            self.assertEqual(r[64], (False, False), (f88, r))


if __name__ == "__main__":
    unittest.main(verbosity=2)
