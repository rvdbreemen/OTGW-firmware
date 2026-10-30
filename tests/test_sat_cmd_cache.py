#!/usr/bin/env python3
"""TASK-1324 / TASK-1370: host test for the SAT last-sent command cache and the 5 s command cadence.

The real code is cut out of src/OTGW-firmware/SATcontrol.ino (from SAT_CMD_REFRESH_MS up to
the Thermal Drop section: satResetCmdCache(), satEnqueueOwnCS(), satEnqueueIfChangedCS/TC(),
satSendCS/MM/CH(), satCmdResend(), satPicCSKeepalive()) and compiled with a host C++ compiler; nothing is re-implemented.
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
struct SatState { bool bActive=true; bool bLastSentValid=false; float fLastSentCS=0; uint8_t iLastSentMM=0, iLastSentCH=0; float fLastSentTC=0; bool bTCAccountedValid=false;
                  uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0;
                  uint32_t iTCPushMs=0; bool bPushRefused=false; };   // TASK-1343
struct State { SatState sat; };
static State state;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static bool isPICEnabled() { return true; }
static bool hasOTCommandInterface() { return true; }
struct OTSys { uint8_t SlaveStatus=0; }; static OTSys OTcurrentSystemState;
static char g_sent[256];
static void addCommandToQueue(const char* c, size_t n, bool, int) { strncat(g_sent, c, n); strcat(g_sent, ";"); }
@BODY@
@STUB_RESEND@
// scenario: tick = CS, MM, CH like satControlLoopRun
static void tick(float cs, int mm, int ch) { @TICK@ }
int main(int argc, char** argv) {
  uint32_t start = (uint32_t)strtoul(argv[1], 0, 10);   // millis() at the first tick
  int mode = argc > 2 ? atoi(argv[2]) : 0;
  if (mode == 0) {
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);   // first emit
    g_sent[0] = 0;
    g_ms = start + 1000; satResetCmdCache();               // offline->online edge, same values
    tick(40.0f, 80, 1);
    printf("%s\n", g_sent);
    g_sent[0] = 0; g_ms = start + 2000; tick(40.0f, 80, 1);  // TASK-1370: no reset, the tick still sends
    printf("[%s]\n", g_sent);
  } else if (mode == 1) {                                  // 5 s resend repeats the last values, not before
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_sent[0] = 0; g_ms = start + 4999; satCmdResend(); printf("[%s]\n", g_sent);
    g_ms = start + 5000; satCmdResend(); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 9999; satCmdResend(); printf("[%s]\n", g_sent);   // 4999 ms after the resend
    g_ms = start + 10000; satCmdResend(); printf("[%s]\n", g_sent);
  } else if (mode == 2) {                                  // nothing sent yet: no resend
    g_ms = start; satResetCmdCache(); g_ms = start + 60000; satCmdResend(); printf("[%s]\n[]\n[]\n[]\n", g_sent);
  } else if (mode == 3) {                                  // the suppression refresh moved CS: the resend repeats that CS
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_ms = start + 3000; satEnqueueIfChangedCS(37.0f);
    g_sent[0] = 0; g_ms = start + 8000; satCmdResend(); printf("[%s]\n[]\n[]\n[]\n", g_sent);
  } else if (mode == 4) {                                  // a DHW-ending tick disarms; a later tick that sends re-arms
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    @DISARM@                                               // what the DHW branch of the tick does
    g_sent[0] = 0; g_ms = start + 8000; satCmdResend(); printf("[%s]\n", g_sent);
    g_ms = start + 30000; tick(41.0f, 80, 1); g_sent[0] = 0;
    g_ms = start + 35000; satCmdResend(); printf("[%s]\n[]\n[]\n", g_sent);
  } else if (mode == 5) {                                  // live fault / DHW bits pause the resend
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_sent[0] = 0; OTcurrentSystemState.SlaveStatus = 0x04; g_ms = start + 8000; satCmdResend(); printf("[%s]\n", g_sent);
    OTcurrentSystemState.SlaveStatus = 0x01; satCmdResend(); printf("[%s]\n", g_sent);
    OTcurrentSystemState.SlaveStatus = 0x00; satCmdResend(); printf("[%s]\n[]\n", g_sent);
  } else if (mode == 6) {                                  // ADR-203: the 30 s PIC keepalive stays silent while the 5 s resend runs
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    int ka = 0, rs = 0;
    for (uint32_t s = 1; s <= 120; s++) {                  // same order as satControlLoopRun(): keepalive first, then the resend
      g_ms = start + s * 1000UL;
      g_sent[0] = 0; satPicCSKeepalive(); if (g_sent[0]) ka++;
      g_sent[0] = 0; satCmdResend();      if (g_sent[0]) rs++;
    }
    printf("[keepalive=%d resend=%d]\n[]\n[]\n[]\n", ka, rs);
  }
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
        body = extract(SAT_INO.read_text(encoding="utf-8"))
        new = "satSendCS" in body                      # old-vs-fix: HEAD has the change-checked senders only
        tick = ("satSendCS(cs); satSendMM((uint8_t)mm); satSendCH((uint8_t)ch);" if new else
                "satEnqueueIfChangedCS(cs); satEnqueueIfChangedMM((uint8_t)mm); satEnqueueIfChangedCH((uint8_t)ch);")
        stub = "" if "satCmdResend" in body else "static void satCmdResend() {}"
        cpp.write_text(STUBS.replace("@BODY@", body).replace("@TICK@", tick).replace("@STUB_RESEND@", stub)
                       .replace("@DISARM@", "_sat_cmdResendArmed = false;" if "_sat_cmdResendArmed" in body else ""))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_at(self, start, mode=0):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(start), str(mode)], capture_output=True, text=True, check=True).stdout.split("\n")
        out += [""] * 4
        return out[0], out[1], out[2], out[3]

    def test_reset_early_after_boot_resends_all(self):
        sent, again, _, _ = self.run_at(10000)    # uptime 10 s < SAT_CMD_REFRESH_MS
        self.assertEqual(sent, "CS=40.0;MM=80;CH=1;")
        # TASK-1370, Sergeant D entry 44: rewritten on purpose. The old assertion was write-on-change ("[]");
        # a control tick now sends CS, MM and CH every time, unchanged values included (thermo-nova heating_control.py:236-238).
        self.assertEqual(again, "[CS=40.0;MM=80;CH=1;]")

    def test_reset_at_millis_zero(self):
        self.assertEqual(self.run_at(0)[0], "CS=40.0;MM=80;CH=1;")

    def test_reset_late_after_boot_resends_all(self):
        self.assertEqual(self.run_at(900000)[0], "CS=40.0;MM=80;CH=1;")

    def test_reset_across_uint32_wrap(self):
        self.assertEqual(self.run_at(4294967295 - 500)[0], "CS=40.0;MM=80;CH=1;")


    def test_resend_repeats_last_values_after_5s_not_before(self):
        a, b, c, d = self.run_at(10000, 1)
        self.assertEqual(a, "[]")                             # 4999 ms after the tick
        self.assertEqual(b, "[CS=40.0;MM=80;CH=1;]")          # 5000 ms
        self.assertEqual(c, "[]")                             # 4999 ms after the resend
        self.assertEqual(d, "[CS=40.0;MM=80;CH=1;]")

    def test_no_resend_before_first_send(self):
        self.assertEqual(self.run_at(10000, 2)[0], "[]")

    def test_resend_repeats_the_cs_the_suppression_refresh_sent(self):
        self.assertEqual(self.run_at(10000, 3)[0], "[CS=37.0;MM=80;CH=1;]")


    def test_dhw_ending_tick_disarms_the_resend(self):
        a, b, _, _ = self.run_at(10000, 4)
        self.assertEqual(a, "[]")
        self.assertEqual(b, "[CS=41.0;MM=80;CH=1;]")

    def test_pic_keepalive_stays_silent_while_the_resend_runs(self):
        # ADR-203 Verification: every resend CS= restarts the keepalive window (iLastSentCSMs), so over 120 s the 30 s
        # keepalive never fires and the 5 s resend fires 24 times.
        self.assertEqual(self.run_at(10000, 6)[0], "[keepalive=0 resend=24]")

    def test_live_fault_and_dhw_bits_pause_the_resend(self):
        a, b, c, _ = self.run_at(10000, 5)
        self.assertEqual((a, b), ("[]", "[]"))
        self.assertEqual(c, "[CS=40.0;MM=80;CH=1;]")


def _code_lines(src):
    return [l for l in src.splitlines() if not l.lstrip().startswith("//")]


class TickPaths(unittest.TestCase):
    """Structural: the four control-tick paths use the always-send helpers; the 3 s suppression refresh stays change-checked."""

    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        cls.code = "\n".join(_code_lines(cls.src))

    def test_tick_paths_use_always_send_helpers(self):
        for name in ("satSendCS", "satSendMM", "satSendCH"):
            # one definition (+ the resend helper's own call) + four control-tick paths
            self.assertEqual(len(re.findall(r"\b%s\(" % name, self.code)), 1 + 4 + 1 + (2 if name == "satSendCS" else 0), name)   # + the start-up flood CS and its hand-over to the suppression (TASK-1371)
        for old in ("satEnqueueIfChangedMM", "satEnqueueIfChangedCH"):
            self.assertNotIn(old, self.code)
        # the only change-checked CS callers left: its definition and the suppression refresh
        self.assertEqual(len(re.findall(r"\bsatEnqueueIfChangedCS\(", self.code)), 2)

    def test_suppression_refresh_stays_change_checked(self):
        refresh = self.src[self.src.index("static void satPwmSuppressionRefresh()"):]
        refresh = refresh[:refresh.index("\n}\n")]
        self.assertIn("satEnqueueIfChangedCS(cs);", refresh)
        self.assertNotIn("satSendCS", refresh)

    def test_dhw_and_fault_tick_paths_disarm_the_resend(self):
        src = self.src
        dhw = src[src.index("    // DHW has priority"):]
        self.assertIn("_sat_cmdResendArmed = false;", dhw[:dhw.index("return;")])
        fault = src[src.index("if (OTcurrentSystemState.SlaveStatus & 0x01) {"):]
        self.assertIn("_sat_cmdResendArmed = false;", fault[:fault.index("return;")])

    def test_resend_runs_after_the_gates_and_before_the_tick(self):
        src = self.src
        i = src.index("\n  satCmdResend();")
        self.assertGreater(i, src.index("if (satRestartStep()) return;"))
        self.assertGreater(i, src.index("if (satOtLinkStep()) return;"))
        self.assertLess(i, src.index("  if (!DUE(timerSATControl)) return;"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
