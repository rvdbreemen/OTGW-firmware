#!/usr/bin/env python3
"""TASK-1834: an OT-Direct CS=/C2= override must live at least as long as on the PIC.

PIC 6.8 (OTGW-other-projects/otgw-6.8/gateway.asm): the command expiry counts Timer 1 overflows of 524.288 ms
(:157-158). StartTimer loads minutetimer with 120 and clears the heartbeat flags (:4233-4239); CommandExpiry decrements it
on every overflow and acts when bit 7 (Expired, :368) sets, i.e. on the 121st tick (:1623-1646). The first CS= starts the
timer and its own HeartbeatCS is cleared at once (SetCtrlSetpoint :4227-4228), so a single CS= lives 120..121 ticks
depending on the tick phase: 62.9 .. 63.4 s. tests/control-3.stc: CS at 2 s still active at 32 s, gone between 65 and 74 s.
OT-Direct before the fix: OT_CSC2_EXPIRY_MS = 60000, checked once a second, so an override refreshed every 60 s could drop
just before its refresh.

REAL code, cut from src/OTGW-firmware/OTDirect.ino: the OT_CSC2_EXPIRY_MS constant and the CS/C2 expiry statements of the
1-second status timer. STUBBED: millis() (fake clock), satOwnsControlSetpoint(), clearWriteOverride() (records the MsgID),
the debug sink.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_csc2_expiry.py
Run: python tests/test_otd_csc2_expiry.py
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
#include <cstring>
#define F(x) x
#define OTDDebugTln(x) do {} while (0)
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool g_satOwns = false;
static bool satOwnsControlSetpoint() { return g_satOwns; }
static int g_cleared1 = 0, g_cleared8 = 0;
static void clearWriteOverride(uint8_t id) { if (id == 1) g_cleared1++; if (id == 8) g_cleared8++; }
static uint32_t otCSLastCommandMs = 0;
static uint32_t otC2LastCommandMs = 0;
"""

MAIN = r"""
static void tick() {   // the 1-second status timer body: the real expiry statements
  uint32_t nowExp = millis();
EXPIRY
}
// argv: owns(0/1) then the times (s) to sample; the command (CS and C2) is given at t = 0
int main(int argc, char** argv) {
  g_satOwns = atoi(argv[1]) != 0;
  g_now = 0; otCSLastCommandMs = 1; otC2LastCommandMs = 1;   // CS= and C2= at t=0 (0 is the "none" sentinel)
  int next = 2;
  for (uint32_t s = 1; s <= 200 && next < argc; s++) {
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
    const = re.search(r"static constexpr uint32_t OT_CSC2_EXPIRY_MS = [^\n]*\n", src)
    assert const, "OT_CSC2_EXPIRY_MS not found"
    a = src.index("    if (otCSLastCommandMs != 0 && !satOwnsControlSetpoint() &&")
    b = src.index("    // TASK-733, TASK-1840: the SC= time becomes invalid", a)
    return STUBS + const.group(0) + MAIN.replace("EXPIRY", src[a:b])


class CsC2Expiry(unittest.TestCase):
    exe = None

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

    def sample(self, owns, *times):
        out = subprocess.run([str(self.exe), str(int(owns)), *map(str, times)], capture_output=True, text=True,
                             check=True).stdout.split("\n")
        return {int(m.group(1)): (m.group(2) == "1", m.group(3) == "1")
                for m in (re.match(r"t=(\d+) cs=(\d) c2=(\d)", l) for l in out) if m}

    def test_still_active_at_61_and_62_s(self):
        # AC1: the last CS= at t=0 is still active at 61 and 62 s (the PIC keeps it at least 62.9 s)
        r = self.sample(False, 61, 62)
        self.assertEqual(r[61], (True, True), r)
        self.assertEqual(r[62], (True, True), r)

    def test_not_expired_before_63_s(self):
        # AC2: never earlier than 63 s, for CS and C2
        r = self.sample(False, 63)
        self.assertEqual(r[63], (True, True), r)

    def test_still_expires_after_the_window(self):
        r = self.sample(False, 64, 70)
        self.assertEqual(r[70], (False, False), r)
        self.assertEqual(r[64], (False, False), r)   # 63.5 s window, checked once a second

    def test_sat_owned_cs_never_expires(self):
        # TASK-1150: while SAT owns the control setpoint the CS override stays (C2 is not SAT's)
        r = self.sample(True, 70, 150)
        self.assertEqual(r[150], (True, False), r)


if __name__ == "__main__":
    unittest.main(verbosity=2)
