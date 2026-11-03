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
#define SATTraceTf(...) ((void)0)
#define SATTraceTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
// TASK-1521 log helpers
static void satNarrate_P(const char*) {}
static void satNarratef_P(const char*, ...) {}
static bool _sat_followNarrPending = false;
static bool _sat_narrAtMax = false;
static bool _sat_narrCold = false;
static bool _sat_bootBoilerLogged = false;
static bool _sat_bootOutsideLogged = false;
#define SATDebugTln(x) ((void)0)
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
static float satEffectiveMaxSetpoint() { return 1000.0f; }   // TASK-1536: satCapRepeatCS() ceiling, no cap here
static bool g_imm = false;
static bool satImmergasTpActive() { return g_imm; }   // TASK-1537: satSendMM Immergas TP
struct OTSys { uint8_t SlaveStatus=0; }; static OTSys OTcurrentSystemState;
static int16_t satPreSatPersisted(bool) { return 0; }   // TASK-1541: the stored restore values are tested in test_sat_disable_handback.py
static void satPreSatPersist(int16_t, int16_t) {}
static bool satIsEnabled() { return false; }   // TASK-1541: satDisable() restores only when SAT is off or tripped
static char g_sent[256];
enum class CmdQueueResult : uint8_t { Queued, Replaced, Sent, Dropped };   // TASK-1574
static int g_drop = 0;   // TASK-1574: 1 = the PIC queue refuses everything (nothing recorded), 2 = refuses only TP=
static CmdQueueResult addCommandToQueue(const char* c, size_t n, bool, int, bool = false) {
  if (g_drop == 1 || (g_drop == 2 && !strncmp(c, "TP=", 3))) return CmdQueueResult::Dropped;
  strncat(g_sent, c, n); strcat(g_sent, ";"); return CmdQueueResult::Queued; }
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
  } else if (mode == 7) {                                  // TASK-1570 entry 237: MM and CH only on change
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_sent[0] = 0; g_ms = start + 1000; tick(40.0f, 60, 1); printf("[%s]\n", g_sent);   // MM changed
    g_sent[0] = 0; g_ms = start + 2000; tick(40.0f, 60, 0); printf("[%s]\n", g_sent);   // CH changed
    g_sent[0] = 0; g_ms = start + 3000; satNotifyPicRestart(); tick(40.0f, 60, 0); printf("[%s]\n", g_sent);   // PIC restart
    g_sent[0] = 0; g_ms = start + 4000; tick(40.0f, 60, 0); printf("[%s]\n", g_sent);   // nothing changed
  } else if (mode == 8) {                                  // TASK-1570: the Immergas TP follows MM on change
    g_imm = true; g_ms = start; satResetCmdCache(); tick(40.0f, 100, 1); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 1000; tick(40.0f, 100, 1); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 2000; tick(40.0f, 60, 1); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 7000; satCmdResend(); printf("[%s]\n", g_sent);
  } else if (mode == 9) {                                  // TASK-1570 entry 239: repair resend of an unchanged MM/CH after 60 s
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_sent[0] = 0; g_ms = start + 59999; tick(40.0f, 80, 1); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 60000; tick(40.0f, 80, 1); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 90000; tick(40.0f, 80, 1); printf("[%s]\n", g_sent);
  } else if (mode == 10) {                                 // TASK-1574: a dropped tick is not recorded, the next one sends again
    g_ms = start; satResetCmdCache(); g_drop = 1; tick(40.0f, 80, 1);
    printf("[%s]\n", g_sent);                                // nothing was accepted
    g_drop = 0; g_ms = start + 1000; tick(40.0f, 80, 1); printf("[%s]\n", g_sent);   // same values: CS, MM and CH go out
    g_sent[0] = 0; g_ms = start + 2000; tick(40.0f, 80, 1); printf("[%s]\n", g_sent);    // accepted now: MM/CH on change only
  } else if (mode == 11) {                                 // Immergas: a dropped TP keeps the MM flag open
    g_imm = true; g_ms = start; satResetCmdCache(); g_drop = 2; tick(40.0f, 100, 1);   // only the TP is refused, the MM is accepted
    g_sent[0] = 0; g_drop = 0; g_ms = start + 1000; tick(40.0f, 100, 1); printf("[%s]\n", g_sent);
    g_sent[0] = 0; g_ms = start + 2000; tick(40.0f, 100, 1); printf("[%s]\n", g_sent);
  } else if (mode == 12) {                                 // a dropped satSendCS is repeated by the 5 s resend
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_ms = start + 1000; g_drop = 1; satSendCS(42.0f); g_drop = 0;   // iLastSentCSMs stays at start
    g_sent[0] = 0; g_ms = start + 4999; satCmdResend(); printf("[%s]\n", g_sent);
    g_ms = start + 5000; satCmdResend(); printf("[%s]\n", g_sent);   // 5 s after the last ACCEPTED CS
  } else if (mode == 14) {                                 // TASK-1539: MM to the maximum for a draw; keepalive on, no 5 s resend
    g_ms = start; satResetCmdCache(); tick(40.0f, 0, 1);   // PWM tick: MM=0
    printf("[%s]\n", g_sent); g_sent[0] = 0;
    char line[512]; line[0] = 0;
    OTcurrentSystemState.SlaveStatus = 0x04;               // the draw
    for (uint32_t s = 1; s <= 119; s++) {                  // satControlLoopRun() order: keepalive, resend, then the control tick
      g_ms = start + s * 1000UL;
      g_sent[0] = 0;
      satPicCSKeepalive(); satCmdResend();
      if (s % 30 == 0) { _sat_cmdResendArmed = false; satSendMMDuringDhw(); }   // the DHW branch of the tick
      if (g_sent[0]) { char b[96]; snprintf(b, sizeof b, "s%lu:%s ", (unsigned long)s, g_sent); strcat(line, b); }
    }
    printf("[%s]\n", line);
    OTcurrentSystemState.SlaveStatus = 0x00;               // the draw ends; the next tick regulates again
    g_ms = start + 120000UL; g_sent[0] = 0; tick(40.0f, 0, 1); printf("[%s]\n", g_sent);
  } else if (mode == 15) {                                 // TASK-1539: which MM the draw sends (satComputeMM, pwmMode false)
    g_ms = start;
    settings.sat.iMaxRelModulation = 80; satResetCmdCache(); g_sent[0] = 0; satSendMMDuringDhw(); printf("[%s]\n", g_sent);   // gas boiler
    settings.sat.iMaxRelModulation = 0;  satResetCmdCache(); g_sent[0] = 0; g_src = SAT_SRC_HEAT_PUMP; satSendMMDuringDhw(); printf("[%s]\n", g_sent);   // heat pump never 0
    satResetCmdCache(); g_sent[0] = 0; g_src = SAT_SRC_GAS_BOILER; g_quirks = SAT_QUIRK_MIN_MOD_10; satSendMMDuringDhw(); printf("[%s]\n", g_sent);   // Geminox floor 10
    g_quirks = 0; settings.sat.iMaxRelModulation = 100; g_imm = true; satResetCmdCache(); g_sent[0] = 0; satSendMMDuringDhw(); printf("[%s]\n", g_sent);   // Immergas: TP follows
  } else if (mode == 13) {                                 // a dropped PIC keepalive is tried again on the next pass
    g_ms = start; satResetCmdCache(); tick(40.0f, 80, 1);
    g_sent[0] = 0; g_ms = start + 30000; g_drop = 1; satPicCSKeepalive(); g_drop = 0; printf("[%s]\n", g_sent);
    g_ms = start + 30001; satPicCSKeepalive(); printf("[%s]\n", g_sent);
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
        if "satNotifyPicRestart" not in body:
            stub += "\nstatic void satNotifyPicRestart() {}"
        # TASK-1539: satSendMMDuringDhw() (outside the cut block, just above satControlLoopRun) = satSendMM(satComputeMM(false, ...));
        # both are cut from the real file. These stubs live here, not in STUBS, which other harnesses reuse as their prelude.
        full = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        extra = ("#define SAT_QUIRK_MIN_MOD_10 0x01\n"
                 "enum SATHeatingSource : uint8_t { SAT_SRC_AUTO = 0, SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2, SAT_SRC_HYBRID = 3 };\n"
                 "struct { struct { uint8_t iMaxRelModulation = 80; } sat; } settings;\n"
                 "static uint8_t g_src = SAT_SRC_GAS_BOILER, g_quirks = 0;\n"
                 "static uint8_t satGetEffectiveHeatingSource() { return g_src; }\n"
                 "static uint8_t satGetManufacturerQuirks() { return g_quirks; }\n")
        compute = full[full.index("static uint8_t satComputeMM("):]
        extra += compute[:compute.index("\n}\n") + 3] + "\n"   # the real satComputeMM()
        if "static void satSendMMDuringDhw()" in full:
            dhw = full[full.index("static void satSendMMDuringDhw()"):]
            body += "\n" + dhw[:dhw.index("\n}\n") + 3]       # the real helper, after satSendMM
        else:                                       # old-vs-fix: HEAD sends no MM during a draw
            stub += "\nstatic void satSendMMDuringDhw() {}"
        cpp.write_text(STUBS.replace("@BODY@", extra + body).replace("@TICK@", tick).replace("@STUB_RESEND@", stub)
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
        # TASK-1570, Sergeant D entry 237: MM and CH only on change; CS still on every tick.
        self.assertEqual(again, "[CS=40.0;]")

    def test_reset_at_millis_zero(self):
        self.assertEqual(self.run_at(0)[0], "CS=40.0;MM=80;CH=1;")

    def test_reset_late_after_boot_resends_all(self):
        self.assertEqual(self.run_at(900000)[0], "CS=40.0;MM=80;CH=1;")

    def test_reset_across_uint32_wrap(self):
        self.assertEqual(self.run_at(4294967295 - 500)[0], "CS=40.0;MM=80;CH=1;")


    def test_resend_repeats_last_values_after_5s_not_before(self):
        a, b, c, d = self.run_at(10000, 1)
        self.assertEqual(a, "[]")                             # 4999 ms after the tick
        self.assertEqual(b, "[CS=40.0;]")                     # 5000 ms; MM/CH on change only (entry 237)
        self.assertEqual(c, "[]")                             # 4999 ms after the resend
        self.assertEqual(d, "[CS=40.0;]")

    def test_no_resend_before_first_send(self):
        self.assertEqual(self.run_at(10000, 2)[0], "[]")

    def test_resend_repeats_the_cs_the_suppression_refresh_sent(self):
        self.assertEqual(self.run_at(10000, 3)[0], "[CS=37.0;]")


    def test_dhw_ending_tick_disarms_the_resend(self):
        a, b, _, _ = self.run_at(10000, 4)
        self.assertEqual(a, "[]")
        self.assertEqual(b, "[CS=41.0;]")

    def test_pic_keepalive_stays_silent_while_the_resend_runs(self):
        # ADR-203 Verification: every resend CS= restarts the keepalive window (iLastSentCSMs), so over 120 s the 30 s
        # keepalive never fires and the 5 s resend fires 24 times.
        self.assertEqual(self.run_at(10000, 6)[0], "[keepalive=0 resend=24]")

    def test_live_fault_and_dhw_bits_pause_the_resend(self):
        a, b, c, _ = self.run_at(10000, 5)
        self.assertEqual((a, b), ("[]", "[]"))
        self.assertEqual(c, "[CS=40.0;]")

    def test_mm_and_ch_only_on_change_and_again_after_pic_restart(self):
        # TASK-1570, Sergeant D entry 237 (msgs 1566917384483381390, 1566917834351575095)
        a, b, c, d = self.run_at(10000, 7)
        self.assertEqual(a, "[CS=40.0;MM=60;]")
        self.assertEqual(b, "[CS=40.0;CH=0;]")
        self.assertEqual(c, "[CS=40.0;MM=60;CH=0;]")     # a PIC restart forgets MM/CH: sent again
        self.assertEqual(d, "[CS=40.0;]")

    def test_immergas_tp_follows_mm_on_change(self):
        a, b, c, d = self.run_at(10000, 8)
        self.assertEqual(a, "[CS=40.0;TP=11:12=80;MM=100;CH=1;]")
        self.assertEqual(b, "[CS=40.0;]")
        self.assertEqual(c, "[CS=40.0;TP=11:12=60;MM=60;]")
        self.assertEqual(d, "[CS=40.0;]")               # the 5 s resend repeats CS only

    def test_unchanged_mm_and_ch_repaired_after_60_s(self):
        # TASK-1570, entry 239: a dropped enqueue or an external MM=/CH= is corrected by one resend once the value is 60 s old
        a, b, c, _ = self.run_at(10000, 9)
        self.assertEqual(a, "[CS=40.0;]")
        self.assertEqual(b, "[CS=40.0;MM=80;CH=1;]")
        self.assertEqual(c, "[CS=40.0;]")

    def test_dropped_enqueue_is_not_cached_and_sent_again(self):
        # TASK-1574 (commit-on-success): a Dropped CS/MM/CH never reaches the cache, so the next tick repeats all three
        a, b, c, _ = self.run_at(10000, 10)
        self.assertEqual(a, "[]")
        self.assertEqual(b, "[CS=40.0;MM=80;CH=1;]")
        self.assertEqual(c, "[CS=40.0;]")  # accepted now: CS every tick, MM/CH on change only

    def test_immergas_dropped_tp_sends_tp_and_mm_again(self):
        a, b, _, _ = self.run_at(10000, 11)
        self.assertEqual(a, "[CS=40.0;TP=11:12=80;MM=100;]")   # TP is min(mm, 80); CH was accepted on the first tick
        self.assertEqual(b, "[CS=40.0;]")

    def test_dropped_satsendcs_is_retried_by_the_5s_resend(self):
        a, b, _, _ = self.run_at(10000, 12)
        self.assertEqual(a, "[]")
        self.assertEqual(b, "[CS=40.0;]")

    def test_dhw_draw_sends_mm_max_keeps_the_keepalive_and_drops_the_resend(self):
        # TASK-1539, Sergeant D entries 220 (MM to max during the draw), 221 (no 5 s resend in hot water), 232 (the 30 s PIC CS
        # keepalive keeps running). Timeline: PWM tick MM=0 at t0; DHW ticks at 30/60/90 s; the draw ends; the next tick regulates.
        a, b, c, _ = self.run_at(10000, 14)
        self.assertEqual(a, "[CS=40.0;MM=0;CH=1;]")
        # keepalive every 30 s after the last CS (s30, s60, s90, s119 not yet); MM=80 on the first draw tick, not again on the
        # next (30 s old), once more at 60 s old (repair, entry 239); no resend CS in between (it would show at s5, s10 ...)
        self.assertEqual(b, "[s30:CS=40.0;MM=80; s60:CS=40.0; s90:CS=40.0;MM=80; ]")
        # after the draw MM=0 differs from the maximum: sent; CH=1 is 120 s old by now, so its repair resend (entry 239) goes too
        self.assertEqual(c, "[CS=40.0;MM=0;CH=1;]")

    def test_dhw_mm_value_follows_the_non_pwm_rules(self):
        # satComputeMM(pwmMode false): the configured maximum; a heat pump never gets 0; Geminox at least 10; Immergas TP follows
        a, b, c, d = self.run_at(10000, 15)
        self.assertEqual(a, "[MM=80;]")
        self.assertEqual(b, "[MM=100;]")
        self.assertEqual(c, "[MM=10;]")
        self.assertEqual(d, "[TP=11:12=80;MM=100;]")

    def test_dropped_keepalive_is_tried_again_on_the_next_pass(self):
        a, b, _, _ = self.run_at(10000, 13)
        self.assertEqual(a, "[]")
        self.assertEqual(b, "[CS=40.0;]")


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
            # TASK-1570: the resend calls satSendCS only (MM/CH are on change, entry 237)
            # TASK-1539: satSendMM also has the call in satSendMMDuringDhw()
            self.assertEqual(len(re.findall(r"\b%s\(" % name, self.code)), 1 + 4 + (3 if name == "satSendCS" else 0) + (1 if name == "satSendMM" else 0), name)   # + the start-up flood CS and its hand-over to the suppression (TASK-1371)
        for old in ("satEnqueueIfChangedMM", "satEnqueueIfChangedCH"):
            self.assertNotIn(old, self.code)
        # the only change-checked CS callers left: its definition and the suppression refresh
        self.assertEqual(len(re.findall(r"\bsatEnqueueIfChangedCS\(", self.code)), 2)

    def test_pic_banner_calls_the_restart_hook(self):
        # TASK-1570: the runtime PIC banner branch of processOT() tells SAT the PIC forgot MM=/CH=
        core = (SAT_INO.parent / "OTGW-Core.ino").read_text(encoding="utf-8")
        branch = core[core.index("} else if (strstr(buf, OTGW_BANNER)!=NULL){"):]
        self.assertIn("satNotifyPicRestart();", branch[:branch.index("\n  } else")] if "\n  } else" in branch else branch[:1500])
        self.assertIn("void satNotifyPicRestart() { satResetCmdCache(); }", self.src)

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

    def test_dhw_tick_sends_the_dhw_mm_before_it_returns(self):
        # TASK-1539: the DHW branch of the tick sends MM through satSendMMDuringDhw() (OT command interface only) and still returns
        src = self.src
        dhw = src[src.index("    // DHW has priority"):]
        dhw = dhw[:dhw.index("    return;")]
        self.assertIn("if (hasOTCommandInterface()) satSendMMDuringDhw();", dhw)
        self.assertNotIn("satSendCS", dhw)
        self.assertNotIn("satSendCH", dhw)

    def test_resend_runs_after_the_gates_and_before_the_tick(self):
        src = self.src
        i = src.index("\n  satCmdResend();")
        self.assertGreater(i, src.index("if (satRestartStep()) return;"))
        self.assertGreater(i, src.index("if (satOtLinkStep()) return;"))
        self.assertLess(i, src.index("  if (!DUE(timerSATControl)) return;"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
