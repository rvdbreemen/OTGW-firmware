#!/usr/bin/env python3
"""TASK-1412.4 / TASK-3151: host test that the slave low-load flag (OpenTherm MsgID 3, HB bit 4) plays no part in SAT PWM.

History: entries 213/214/215 made bit 4 = 1 refuse, end or delay every PWM entry path; TASK-2000 (entries 392/393) reversed
them; TASK-3151.4 (ADR-251, ADR-305) removed the permission code itself (satPwmPermitted, satPwmEntryAllowed,
satPwmNoteRefused, satCycleCheckLowLoadVeto), the deferred restore (satRestoreDeferredStep) and the start gate. What is left, and
tested here against the REAL functions:
  - the decoder hook satNoteSlaveConfig() records the bit and the time of its change (REST slave_cfg_valid, slave_low_off_denied),
  - the 60 s overshoot switch fires on the same tick for every MsgID 3 state (no reply, bit 4 = 0, bit 4 = 1),
  - a bit 4 that rises while PWM runs does not end PWM and does not delay the switch afterwards,
  - the plain restore after a restart enters PWM for every MsgID 3 state; the control_mode command is ignored (TASK-3151.4),
  - source checks: the decoder feeds only the boiler's valid Read-Ack, and no permission code remains in the entry paths.
OT spec v4.2, slave configuration flags (ID 3), HB bit 4: "Master low-off & pump control function [allowed, not allowed]".

The code under test is NOT re-implemented: satNoteSlaveConfig / satCycleCheckOvershootSwitch / satCycleCheckPwmExits come out of
SATcycles.ino (tests/test_sat_overshoot_sustain.extract) and satSetControlMode / satHandleControlMode / satRestoreControlMode out of
SATcontrol.ino, compiled with a host C++ compiler. Without a compiler the compiled cases are skipped.

Run: python tests/test_sat_slave_lowload.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_sat_overshoot_sustain as sustain  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = sustain.FW
CYCLES_INO = sustain.CYCLES_INO
SAT_INO = sustain.SAT_INO
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino"))

# The shared stubs of tests/test_sat_overshoot_sustain.py are re-cut here by name, not by exact text, so a change of their layout
# does not break this harness; what this test needs is stated in the two structs below.
SAT_STATE = ("struct SatState { SATControlMode eControlMode = SAT_MODE_OFF; bool bDhwActive = false, bPwmFlameRequested = false;\n"
             "  uint8_t iRequestedControlMode = 0; bool bSafetyTripped = false; bool bSlaveCfgValid = false, bSlaveLowOffDenied = false;\n"
             "  uint32_t iSlaveCfgChangeMs = 0; bool bValvesOpen = true, bSummerActive = false; };")
SAT_SETTINGS = "struct SatSettings { float fPwmSwitchMargin = 3.0f; bool bSummerSimmer = false; };"
# stub lines of the old permission / gate code; the new code has none of these functions
DROP = ("satStartGateActive", "satPwmPermitted", "satPwmNoteRefused", "satPwmEntryAllowed", "satCycleCheckLowLoadVeto", "g_allow")

EXTRA_STUBS = r"""
#include <cctype>
#define snprintf_P snprintf
static int strcasecmp_P(const char* a, const char* b) {
  while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
static float satColdThreshold() { return 20.0f; }   // the request (50) is above the cold setpoint: the overshoot rule is allowed to run
static bool g_enabled = true;
static bool satIsEnabled() { return g_enabled; }
static void satHandleEnabled(const char*) {}
static void sendWebSocketJSON(const char*) {}
static bool g_savedPwm = false;
static bool satLoadPwmEnabled() { return g_savedPwm; }
static void satSavePwmEnabled(bool enabled) { g_savedPwm = enabled; }
static int g_pwmResets = 0;
static void satPwmReset() { g_pwmResets++; }
static void satPwmKeptFreeze() {}   // the real satSetControlMode() freezes the kept ON time before the reset; not under test here
static bool _pwm_idle = true, _pwm_entryPending = false;
void satCycleSkipAutoSwitch();
"""

MAIN = r"""
static void setperm(int valid, int denied) { state.sat.bSlaveCfgValid = valid != 0; state.sat.bSlaveLowOffDenied = denied != 0; }
static void report(const char* tag) {
  printf("tag=%s mode=%d req=%d valid=%d denied=%d\n", tag, (int)state.sat.eControlMode, (int)state.sat.iRequestedControlMode,
         (int)state.sat.bSlaveCfgValid, (int)state.sat.bSlaveLowOffDenied);
}
static float g_osFlow = 53.5f;   // 53.5 against 50: above request + 3.0 (the margin default), so the 60 s rule fires
static int overshootTicks(uint32_t maxTicks) {   // 30 s ticks, flow above request + margin: the tick count at the switch, or -1
  state.sat.eControlMode = SAT_MODE_CONTINUOUS; g_flow = g_osFlow;
  for (uint32_t i = 1; i <= maxTicks; i++) {
    g_ms += 30000UL;
    satCycleCheckOvershootSwitch(50.0f, g_ms);
    if (state.sat.eControlMode == SAT_MODE_PWM) return (int)i;
  }
  return -1;
}
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  const char* sc = argv[1];
  if (!strcmp(sc, "decode")) {             // decode hb1 hb2 ... : valid replies in sequence, state after each
    for (int i = 2; i < argc; i++) {
      g_ms += 1000UL; satNoteSlaveConfig((uint8_t)strtoul(argv[i], 0, 16));
      printf("hb=%s valid=%d denied=%d change=%lu\n", argv[i], (int)state.sat.bSlaveCfgValid, (int)state.sat.bSlaveLowOffDenied,
             (unsigned long)state.sat.iSlaveCfgChangeMs);
    }
    return 0;
  }
  const int valid = argc > 2 ? atoi(argv[2]) : 1, denied = argc > 3 ? atoi(argv[3]) : 0;
  setperm(valid, denied);
  if (!strcmp(sc, "overshoot")) {          // the automatic 60 s switch
    const int n = overshootTicks(20);
    printf("ticks=%d ", n); report("overshoot");
  } else if (!strcmp(sc, "restore")) {     // PWM saved before the restart: the plain restore (TASK-3151.2)
    g_savedPwm = argc > 4 ? atoi(argv[4]) != 0 : true;
    satRestoreControlMode();
    report("restore");
  } else if (!strcmp(sc, "command")) {     // control_mode command: argv[4] = value
    g_enabled = argc > 5 ? atoi(argv[5]) != 0 : true;
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
    satHandleControlMode(argc > 4 ? argv[4] : "pwm");
    report("command");
  } else if (!strcmp(sc, "setmode")) {     // any other caller of satSetControlMode(PWM)
    satSetControlMode(SAT_MODE_PWM);
    report("setmode");
  } else if (!strcmp(sc, "rise")) {        // bit 4 turns 1 while PWM runs, later back to 0
    satNoteSlaveConfig(0x00);
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 52.0f; g_flame = true;
    satCycleCheckPwmExits(50.0f, g_ms += 30000UL); report("allowed");
    satNoteSlaveConfig(0x10);
    satCycleCheckPwmExits(50.0f, g_ms += 30000UL); report("denied");       // a PWM check never ends PWM because of bit 4
    const int still = overshootTicks(18); printf("while_denied_ticks=%d ", still); report("overshoot_denied");
    satNoteSlaveConfig(0x00);
    const int again = overshootTicks(20); printf("after_clear_ticks=%d ", again); report("overshoot_allowed");
  } else {
    return 2;
  }
  return 0;
}
"""


def build_source():
    cyc = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    flags, body = sustain.extract(cyc)
    stubs = sustain.STUBS
    stubs = re.sub(r"^struct SatSettings \{.*\};.*$", SAT_SETTINGS, stubs, flags=re.M)
    stubs = re.sub(r"^struct SatState \{.*\};.*$", SAT_STATE, stubs, flags=re.M)
    stubs = "\n".join(l for l in stubs.split("\n") if not any(d in l for d in DROP))
    stubs = re.sub(r"^static void satSetControlMode\(SATControlMode m\).*$", "", stubs, flags=re.M)   # the real one is cut below
    if "struct SatState { SATControlMode eControlMode = SAT_MODE_OFF" not in stubs or "float fPwmSwitchMargin = 3.0f" not in stubs:
        raise AssertionError("tests/test_sat_overshoot_sustain.py STUBS changed shape; update this harness")
    fn = sustain._fn
    real = fn(sat, "static void satSetControlMode(SATControlMode m)")
    handler = fn(sat, "void satHandleControlMode(const char* value)")
    restore = fn(sat, "static void satRestoreControlMode()")
    note = fn(cyc, "void satNoteSlaveConfig(uint8_t hb)")
    if not (real and handler and restore and note):
        raise AssertionError("satSetControlMode / satHandleControlMode / satRestoreControlMode / satNoteSlaveConfig not found")
    return flags + stubs + EXTRA_STUBS + real + "\n" + handler + "\n" + restore + "\n" + body + "\n" + note + "\n" + MAIN


class SlaveLowLoad(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")
        cls.cyc = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, *args):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        rows = [dict(kv.split("=") for kv in line.split()) for line in out.strip().splitlines()]
        return rows if len(rows) > 1 else rows[0]

    # Entries 392/393 (ADR-251): MsgID 3 plays no part in PWM. The 60 s switch (entry 407) fires on the 3rd 30 s tick for every state.
    def test_overshoot_switch_ignores_msgid3(self):
        for valid, denied, why in ((1, 0, "bit 4 = 0"), (1, 1, "bit 4 = 1 (Vitodens 100-W, entry 392)"), (0, 0, "no valid reply yet (entry 393)")):
            with self.subTest(why=why):
                r = self.run_sc("overshoot", valid, denied)
                self.assertEqual((r["mode"], r["ticks"]), ("2", "3"), why)

    # TASK-3151.1: the 30 s fail-safe is gone: the switch needs the full 60 s (3 ticks of 30 s), never the 2nd tick of the old fail-safe.
    def test_a_large_overshoot_switches_after_60s_not_30s(self):
        r = self.run_sc("overshoot", 1, 1)
        self.assertEqual(r["ticks"], "3", "60 s, not the 30 s of the removed fail-safe")

    # TASK-3151.2: the restore after a restart is plain: PWM saved before comes back in the same call, whatever MsgID 3 says.
    def test_restart_restore_of_pwm_ignores_msgid3(self):
        for valid, denied, why in ((1, 0, "permitted"), (1, 1, "bit 4 = 1"), (0, 0, "no valid reply yet (a fresh restart)")):
            with self.subTest(why=why):
                self.assertEqual(self.run_sc("restore", valid, denied, 1)["mode"], "2", why)

    def test_nothing_saved_nothing_restored(self):
        self.assertEqual(self.run_sc("restore", 1, 1, 0)["mode"], "1")

    # TASK-3151.4 (ADR-305 decision 8, entry 519): the control_mode command is ignored whatever its value and whatever MsgID 3 says:
    # the mode stays continuous and the requested mode stays 0 (auto). The old cases (a manual pwm command that ignored MsgID 3) are gone with it.
    def test_control_mode_command_changes_nothing_for_any_msgid3_state(self):
        for value in ("pwm", "2", "continuous", "1", "auto", "0"):
            for valid, denied in ((1, 0), (1, 1), (0, 0)):
                with self.subTest(value=value, valid=valid, denied=denied):
                    r = self.run_sc("command", valid, denied, value)
                    self.assertEqual((r["mode"], r["req"]), ("1", "0"))

    def test_any_other_caller_of_set_control_mode_enters_pwm(self):
        for valid, denied in ((1, 0), (1, 1), (0, 0)):
            with self.subTest(valid=valid, denied=denied):
                self.assertEqual(self.run_sc("setmode", valid, denied)["mode"], "2")

    def test_bit4_rising_during_pwm_does_not_leave_pwm(self):
        rows = self.run_sc("rise")
        self.assertEqual(rows[0]["mode"], "2")
        self.assertEqual(rows[1]["mode"], "2", "bit 4 = 1 while PWM runs: PWM stays (entry 214 is reversed)")

    def test_automatic_switch_ignores_bit4(self):
        rows = self.run_sc("rise")
        self.assertEqual(len(rows), 4)
        self.assertEqual((rows[2]["while_denied_ticks"], rows[2]["mode"]), ("3", "2"), "bit 4 = 1: the switch fires after 60 s")
        self.assertEqual((rows[3]["after_clear_ticks"], rows[3]["mode"]), ("3", "2"), "bit 4 = 0: the same 60 s")

    # The decoder: valid replies set the state, the change time moves only on a change.
    def test_decoder_hook_stores_bit4_and_the_time_of_the_change(self):
        rows = self.run_sc("decode", "00", "00", "10", "10", "EF", "FF")
        self.assertEqual([(r["valid"], r["denied"]) for r in rows],
                         [("1", "0"), ("1", "0"), ("1", "1"), ("1", "1"), ("1", "0"), ("1", "1")])
        self.assertEqual([r["change"] for r in rows], ["1001000", "1001000", "1003000", "1003000", "1005000", "1006000"])

    # --- source-structure checks of what the host cannot run ---

    def test_decoder_feeds_only_the_boilers_valid_read_ack(self):
        core = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = re.search(r"void print_slavememberid\(uint16_t& value\)\n\{.*?\n\}", core, re.S).group(0)
        # TASK-1857/TASK-1903: only a real boiler Read-Ack counts, not an OT-Direct loopback answer or a replayed line.
        self.assertRegex(body, re.compile(r"if \(is_value_valid\(OTdata, OTlookupitem\)\)\{.*?if \(OTdata\.rsptype == OTGW_BOILER && boilerEvidence\) satNoteSlaveConfig\(OTdata\.valueHB\);", re.S))
        self.assertRegex(body, r"const bool boilerEvidence = !OTdata\.bLocalAnswer && !OTdata\.bReplayed;")

    def test_recording_the_bit_does_not_read_the_heating_source(self):
        # ADR-157: manual gas/HP/hybrid selection stays authoritative; the bit is shown, not used as a source detector
        body = sustain._fn(self.cyc, "void satNoteSlaveConfig(uint8_t hb)")
        self.assertTrue(body)
        self.assertNotRegex(body, r"(?i)source|heating_?system|hybrid|bHeatPump")
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertNotRegex(sustain._fn(sat, "static void satRestoreControlMode()"), r"(?i)satGetEffectiveHeatingSource|settings\.sat\.iHeatingSource")

    def test_no_permission_code_remains_in_the_pwm_entry_paths(self):
        # ADR-251 / TASK-3151.4: the names below are gone from the code (the comment that records their removal is not code)
        sat = SAT_INO.read_text(encoding="utf-8")
        code = "\n".join(l for l in (sat + self.cyc).split("\n") if not l.lstrip().startswith("//"))
        for name in ("satPwmPermitted", "satPwmEntryAllowed", "satPwmNoteRefused", "satCycleCheckLowLoadVeto",
                     "satRestoreDeferredStep", "satStartGateActive"):
            self.assertNotIn(name, code, name + " is gone (ADR-251, TASK-3151)")


if __name__ == "__main__":
    unittest.main()
