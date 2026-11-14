#!/usr/bin/env python3
"""TASK-1811: in OT-Direct master mode a thermostat-written TSet (MsgID 1) gets the CS expiry and a TSet of 0 releases.

Robert 2026-11-11: "Expiry + TSet=0 respected". Coordinator 2026-11-14 (reading of that decision after TASK-1917): the
thermostat's TSet always expires after OT_CSC2_EXPIRY_MS, whatever its value; the below-8 permanence of a user CS=
(otdCsc2ExpiryStamp) is not used for a thermostat write. TASK-1150: while SAT owns the setpoint the write changes nothing.

REAL code, cut from src/OTGW-firmware/OTDirect.ino: OT_CSC2_EXPIRY_MS, buildOTResponse(), handleMasterModeSlaveFrame() and
the CS/C2 expiry statements of the 1-second status timer. STUBBED: millis() (fake clock), satOwnsControlSetpoint(),
setOverride/clearWriteOverride/updateWriteCache (record the MsgID 1 override state), setOTParityBit, the frame log and
reply sinks, the boiler cache arrays, otdCsc2ExpiryStamp() (counts calls: must not be used by this path).
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_master_tset_expiry.py
Run: python tests/test_otd_master_tset_expiry.py
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
static bool g_ovr1 = false; static uint16_t g_ovr1Val = 0; static int g_stampCalls = 0;
static void setOverride(uint8_t id, uint16_t v) { if (id == 1) { g_ovr1 = true; g_ovr1Val = v; } }
static void clearWriteOverride(uint8_t id) { if (id == 1) g_ovr1 = false; }
static void updateWriteCache(uint8_t, uint16_t) {}
static uint32_t otdCsc2ExpiryStamp(uint16_t) { g_stampCalls++; return 0; }
static void setOTParityBit(unsigned long&) {}
static void bridgeFrameToParser(char, unsigned long) {}
static void otdSendReply(unsigned long) {}
static bool otBoilerCacheValid[128];
static uint16_t otBoilerCache[128];
static uint32_t otCSLastCommandMs = 0;
static uint32_t otC2LastCommandMs = 0;
static uint8_t getUnknownCount(uint8_t) { return 0; }   // TASK-1808: READ path only
"""

MAIN = r"""
static void tick() {
  uint32_t nowExp = millis();
EXPIRY
}
static unsigned long writeFrame(uint8_t id, uint16_t v) { return (1UL << 28) | ((unsigned long)id << 16) | v; }
// argv: owns(0/1) f88(hex, written by the thermostat at t=0) [zero_at_s] then sample times (s)
int main(int argc, char** argv) {
  g_satOwns = atoi(argv[1]) != 0;
  uint16_t f88 = (uint16_t)strtoul(argv[2], nullptr, 16);
  int zeroAt = atoi(argv[3]);
  g_now = 1000; handleMasterModeSlaveFrame(writeFrame(1, f88));
  int next = 4;
  for (uint32_t s = 2; s <= 200 && next < argc; s++) {
    g_now = s * 1000UL;
    if ((int)s == zeroAt) handleMasterModeSlaveFrame(writeFrame(1, 0));
    tick();
    while (next < argc && (uint32_t)atoi(argv[next]) == s) {
      printf("t=%u ovr=%d val=%04X stampcalls=%d\n", s, g_ovr1 ? 1 : 0, g_ovr1Val, g_stampCalls);
      next++;
    }
  }
  return 0;
}
"""


def cut(src, start, end_marker):
    a = src.index(start)
    b = src.index(end_marker, a)
    return src[a:b]


def build_source():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    const = re.search(r"static constexpr uint32_t OT_CSC2_EXPIRY_MS = [^\n]*\n", src).group(0)
    resp = cut(src, "static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data) {",
               "\nstatic void handleMasterModeSlaveFrame(") + "\n"
    handler = cut(src, "static void handleMasterModeSlaveFrame(unsigned long frame) {", "\n// ----") + "\n"
    expiry = cut(src, "    if (otCSLastCommandMs != 0 && !satOwnsControlSetpoint() &&",
                 "    // TASK-733, TASK-1840: the SC= time becomes invalid")
    return STUBS + const + resp + handler + MAIN.replace("EXPIRY", expiry)


class MasterTSetExpiry(unittest.TestCase):
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

    def run_case(self, owns, f88, zero_at, *times):
        out = subprocess.run([str(self.exe), str(int(owns)), f88, str(zero_at), *map(str, times)],
                             capture_output=True, text=True, check=True).stdout.split("\n")
        return {int(m.group(1)): (m.group(2) == "1", m.group(3), int(m.group(4)))
                for m in (re.match(r"t=(\d+) ovr=(\d) val=([0-9A-F]{4}) stampcalls=(\d+)", l) for l in out) if m}

    def test_tset_expires_after_the_window(self):
        # AC1: TSet 45.5 written at t=1 s: active at 64 s (63 s after), gone at 66 s
        r = self.run_case(False, "2D80", 0, 64, 66)
        self.assertEqual(r[64][:2], (True, "2D80"), r)
        self.assertFalse(r[66][0], r)

    def test_tset_below_8_also_expires(self):
        # coordinator reading of Robert's decision: a thermostat TSet of 5.0 is not permanent (no TASK-1917 rule)
        r = self.run_case(False, "0500", 0, 64, 66)
        self.assertTrue(r[64][0], r)
        self.assertFalse(r[66][0], r)
        self.assertEqual(r[66][2], 0, "the thermostat path must not use otdCsc2ExpiryStamp")

    def test_tset_zero_releases_at_once(self):
        # AC2: TSet 45.5 at 1 s, TSet 0 at 10 s: gone at 10 s
        r = self.run_case(False, "2D80", 10, 9, 10)
        self.assertTrue(r[9][0], r)
        self.assertFalse(r[10][0], r)

    def test_sat_owned_write_changes_nothing(self):
        # AC3 (TASK-1150): while SAT owns the setpoint the thermostat write sets no override
        r = self.run_case(True, "2D80", 0, 5)
        self.assertFalse(r[5][0], r)


if __name__ == "__main__":
    unittest.main(verbosity=2)
