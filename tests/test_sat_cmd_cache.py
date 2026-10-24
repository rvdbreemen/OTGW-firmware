#!/usr/bin/env python3
"""TASK-1324: host test for the SAT last-sent command cache reset.

The real code is cut out of src/OTGW-firmware/SATcontrol.ino (from SAT_CMD_REFRESH_MS up to
the Thermal Drop section: satResetCmdCache(), satEnqueueOwnCS(), satEnqueueIfChangedCS/MM/CH/TC(),
satPicCSKeepalive()) and compiled with a host C++ compiler; nothing is re-implemented.
OTGW_SAT_INO points at another copy (e.g. `git show HEAD:...`) for old-vs-fix runs.
Without a compiler the tests are skipped.

Run: python tests/test_sat_cmd_cache.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#define PSTR(x) x
#define snprintf_P snprintf
struct SatState { bool bLastSentValid=false; float fLastSentCS=0; uint8_t iLastSentMM=0, iLastSentCH=0; float fLastSentTC=0; bool bTCAccountedValid=false;
                  uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0;
                  uint32_t iTCPushMs=0; bool bPushRefused=false; };   // TASK-1343
struct State { SatState sat; };
static State state;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static bool isPICEnabled() { return true; }
static char g_sent[256];
static void addCommandToQueue(const char* c, size_t n, bool, int) { strncat(g_sent, c, n); strcat(g_sent, ";"); }
@BODY@
// scenario: tick = CS, MM, CH like satControlLoopRun
static void tick(float cs, int mm, int ch) { satEnqueueIfChangedCS(cs); satEnqueueIfChangedMM((uint8_t)mm); satEnqueueIfChangedCH((uint8_t)ch); }
int main(int argc, char** argv) {
  uint32_t start = (uint32_t)strtoul(argv[1], 0, 10);   // millis() at the first tick
  g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);   // first emit
  g_sent[0] = 0;
  g_ms = start + 1000; satResetCmdCache();               // offline->online edge, same values
  tick(40.0f, 80, 1);
  printf("%s\n", g_sent);
  g_sent[0] = 0; g_ms = start + 2000; tick(40.0f, 80, 1);  // no reset: still write-on-change
  printf("[%s]\n", g_sent);
  return 0;
}
"""


def extract(src: str) -> str:
    a = src.index("static const uint32_t SAT_CMD_REFRESH_MS")
    b = src.index("// --- Thermal Drop Learning State")
    return src[a:b]


class CmdCacheReset(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(STUBS.replace("@BODY@", extract(SAT_INO.read_text(encoding="utf-8"))))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_at(self, start):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(start)], capture_output=True, text=True, check=True).stdout.split("\n")
        return out[0], out[1]

    def test_reset_early_after_boot_resends_all(self):
        sent, again = self.run_at(10000)          # uptime 10 s < SAT_CMD_REFRESH_MS
        self.assertEqual(sent, "CS=40.0;MM=80;CH=1;")
        self.assertEqual(again, "[]")

    def test_reset_at_millis_zero(self):
        self.assertEqual(self.run_at(0)[0], "CS=40.0;MM=80;CH=1;")

    def test_reset_late_after_boot_resends_all(self):
        self.assertEqual(self.run_at(900000)[0], "CS=40.0;MM=80;CH=1;")

    def test_reset_across_uint32_wrap(self):
        self.assertEqual(self.run_at(4294967295 - 500)[0], "CS=40.0;MM=80;CH=1;")


if __name__ == "__main__":
    unittest.main(verbosity=2)
