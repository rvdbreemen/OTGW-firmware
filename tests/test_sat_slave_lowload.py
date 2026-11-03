#!/usr/bin/env python3
"""TASK-1412.4: host test for the slave low-load permission (OpenTherm MsgID 3, HB bit 4) in SAT.

Decisions (docs/sat/sergeant-d-corrections.md): entry 213 (QD-1 A: bit 4 = 1 blocks every PWM entry path: the 60 s overshoot
switch, PWM restored after a restart, the manual pwm command, the future end-of-cycle switch; SAT stays continuous), entry 214
(QD-2 A: bit 4 turning 1 while PWM runs leaves PWM in that tick, the continuous setpoint goes out as with the underheat exit),
entry 215 (QD-3 B: no PWM until a valid MsgID 3 reply) and ADR-157 (the manual heating source stays authoritative; the bit is a
permission, not a source detector). OT spec v4.2, slave configuration flags (ID 3), HB bit 4: "Master low-off & pump control
function [allowed, not allowed]", 1 = not allowed.

The code under test is NOT re-implemented: the real satCycleCheckOvershootSwitch / satCycleCheckPwmExits /
satCycleCheckLowLoadVeto / satNoteSlaveConfig / satPwmPermitted / satPwmNoteRefused / satPwmEntryAllowed are cut out of
SATcycles.ino (through tests/test_sat_overshoot_sustain.extract plus the permission block) and the real satSetControlMode,
satRestoreControlMode and satHandleControlMode out of SATcontrol.ino, and compiled with a host C++ compiler. Without a compiler
the compiled cases are skipped. The decoder call in OTGW-Core.ino and the control-loop call are source-structure checks (the host
cannot run print_slavememberid() or the control loop).

Old-vs-fix: OTGW_SAT_INO / OTGW_SATCYCLES_INO point at `git show <rev>:src/OTGW-firmware/...` copies. On code from before the fix
the harness models what the old code has: no veto call in the loop, no restore function (the two inline restore statements are
used instead), and it sets the permission fields directly, which the old code ignores.

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

SAT_STATE_OLD = "struct SatState { SATControlMode eControlMode = SAT_MODE_CONTINUOUS; bool bDhwActive = false, bPwmFlameRequested = false; };"
SAT_STATE_NEW = ("struct SatState { SATControlMode eControlMode = SAT_MODE_OFF; bool bDhwActive = false, bPwmFlameRequested = false;\n"
                 "  uint8_t iRequestedControlMode = 0; bool bSafetyTripped = false; bool bSlaveCfgValid = false, bSlaveLowOffDenied = false; uint32_t iSlaveCfgChangeMs = 0;\n"
                 "  uint8_t iPwmVetoReason = 0, iPwmVetoPath = 0; uint32_t iPwmVetoMs = 0; };")
PERMIT_STUBS = ("// TASK-1412.4: the slave low-load permission is not under test here (tests/test_sat_slave_lowload.py): PWM is permitted\n"
                "static bool satPwmPermitted() { return true; }\n"
                "static void satPwmNoteRefused(uint8_t) {}\n"
                "static bool satPwmEntryAllowed(uint8_t) { return true; }\n"
                "static bool satCycleCheckLowLoadVeto() { return false; }\n")   # the shared harness stubs; this test uses the real functions
STUB_SETCONTROL = "static void satSetControlMode(SATControlMode m) { if (m != state.sat.eControlMode) g_switches++; state.sat.eControlMode = m; }"

EXTRA_STUBS = r"""
#include <cctype>
#define snprintf_P snprintf
static int strcasecmp_P(const char* a, const char* b) {
  while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
static bool g_enabled = true;
static bool satIsEnabled() { return g_enabled; }
static void satHandleEnabled(const char*) {}
static void sendWebSocketJSON(const char*) {}
static bool g_savedPwm = false;
static bool satLoadPwmEnabled() { return g_savedPwm; }
static void satSavePwmEnabled(bool enabled) { g_savedPwm = enabled; }
static uint8_t _sat_pwmRestore = 0;   // the .ino declares it next to satRestoreControlMode()
static int g_pwmResets = 0;
static void satPwmReset() { g_pwmResets++; }
static bool _pwm_idle = true, _pwm_entryPending = false;
void satCycleSkipAutoSwitch();
bool satPwmPermitted();
void satPwmNoteRefused(uint8_t path);
bool satPwmEntryAllowed(uint8_t path);
"""

MAIN = r"""
static void setperm(int valid, int denied) { state.sat.bSlaveCfgValid = valid != 0; state.sat.bSlaveLowOffDenied = denied != 0; }
static void report(const char* tag) {
  printf("tag=%s mode=%d req=%d valid=%d denied=%d reason=%d path=%d\n", tag, (int)state.sat.eControlMode, (int)state.sat.iRequestedControlMode,
         (int)state.sat.bSlaveCfgValid, (int)state.sat.bSlaveLowOffDenied, (int)state.sat.iPwmVetoReason, (int)state.sat.iPwmVetoPath);
}
static void pushHb(uint8_t hb) {
#ifdef HAS_SLAVE_NOTE
  satNoteSlaveConfig(hb);
#else
  setperm(1, (hb & 0x10) != 0);          // the old code has no decoder hook: the fields are set directly and stay unread
#endif
}
static int overshootTicks(uint32_t maxTicks) {   // 30 s ticks, flow far above the request: returns the tick count at the switch or -1
  state.sat.eControlMode = SAT_MODE_CONTINUOUS; g_flow = 55.0f;
  for (uint32_t i = 1; i <= maxTicks; i++) {
    g_ms += 30000UL;
    satCycleCheckOvershootSwitch(50.0f, g_ms);
    if (state.sat.eControlMode == SAT_MODE_PWM) return (int)i;
  }
  return -1;
}
static int permittedNow() {
#ifdef HAS_SLAVE_NOTE
  return (int)satPwmPermitted();
#else
  return -1;                             // the old code has no permission function
#endif
}
static void controlPass() {              // the two restore/save steps the control loop runs once per pass
#ifdef HAS_DEFER
  satRestoreDeferredStep();
  satPwmSaveStep();
#else
  if (state.sat.eControlMode != SAT_MODE_OFF) satSavePwmEnabled(state.sat.eControlMode == SAT_MODE_PWM);   // the old inline line
#endif
}
static void veto() {
#ifdef HAS_VETO
  satCycleCheckLowLoadVeto();
#endif
}
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  const char* sc = argv[1];
  if (!strcmp(sc, "decode")) {             // decode hb1 hb2 ... : valid replies in sequence, state after each
    for (int i = 2; i < argc; i++) {
      g_ms += 1000UL; pushHb((uint8_t)strtoul(argv[i], 0, 16));
      printf("hb=%s valid=%d denied=%d change=%lu permitted=%d\n", argv[i], (int)state.sat.bSlaveCfgValid, (int)state.sat.bSlaveLowOffDenied,
             (unsigned long)state.sat.iSlaveCfgChangeMs, permittedNow());
    }
    return 0;
  }
  const int valid = argc > 2 ? atoi(argv[2]) : 1, denied = argc > 3 ? atoi(argv[3]) : 0;
  setperm(valid, denied);
  if (!strcmp(sc, "overshoot")) {          // the automatic 60 s switch
    const int n = overshootTicks(20);
    printf("ticks=%d ", n); report("overshoot");
  } else if (!strcmp(sc, "restore")) {     // PWM saved before the restart
    g_savedPwm = argc > 4 ? atoi(argv[4]) != 0 : true;
#ifdef HAS_RESTORE
    satRestoreControlMode();
#else
    if (satLoadPwmEnabled()) satSetControlMode(SAT_MODE_PWM); else state.sat.eControlMode = SAT_MODE_CONTINUOUS;   // the old inline statements
#endif
    report("restore");
  } else if (!strcmp(sc, "command")) {     // control_mode command: argv[4] = value
    g_enabled = argc > 5 ? atoi(argv[5]) != 0 : true;
    satHandleControlMode(argc > 4 ? argv[4] : "pwm");
    report("command");
  } else if (!strcmp(sc, "deferred")) {    // deferred saved hb...: a restart with PWM saved; one control pass after each step ("-" = no reply yet)
    setperm(0, 0);
    g_savedPwm = atoi(argv[2]) != 0;
    state.sat.eControlMode = SAT_MODE_OFF;
#ifdef HAS_RESTORE
    satRestoreControlMode();
#else
    if (satLoadPwmEnabled()) satSetControlMode(SAT_MODE_PWM); else state.sat.eControlMode = SAT_MODE_CONTINUOUS;   // the old inline statements
#endif
    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "-")) pushHb((uint8_t)strtoul(argv[i], 0, 16));
      controlPass();
      printf("step=%d saved=%d ", i - 2, (int)g_savedPwm); report("deferred");
    }
  } else if (!strcmp(sc, "setmode")) {     // any other caller of satSetControlMode(PWM), e.g. the future end-of-cycle switch
    satSetControlMode(SAT_MODE_PWM);
    report("setmode");
  } else if (!strcmp(sc, "veto")) {        // PWM runs (ON phase), the control tick asks the veto
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    veto();
    report("veto");
  } else if (!strcmp(sc, "rise")) {        // bit 4 turns 1 while PWM runs, later back to 0
    pushHb(0x00);
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    veto(); report("allowed");
    pushHb(0x10); veto(); report("denied");
    const int still = overshootTicks(18); printf("while_denied_ticks=%d ", still); report("overshoot_denied");
    pushHb(0x00);
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
    if SAT_STATE_OLD not in sustain.STUBS or STUB_SETCONTROL not in sustain.STUBS or PERMIT_STUBS not in sustain.STUBS:
        raise AssertionError("tests/test_sat_overshoot_sustain.py STUBS changed; update this harness")
    stubs = sustain.STUBS.replace(SAT_STATE_OLD, SAT_STATE_NEW).replace(STUB_SETCONTROL, "").replace(PERMIT_STUBS, "")
    if "bool satCycleCheckLowLoadVeto()" in cyc:
        flags += "#define HAS_VETO 1\n"
    if "void satNoteSlaveConfig(uint8_t hb)" in cyc:
        flags += "#define HAS_SLAVE_NOTE 1\n"
    if "static void satRestoreControlMode()" in sat:
        flags += "#define HAS_RESTORE 1\n"
    fn = sustain._fn
    real = fn(sat, "static void satSetControlMode(SATControlMode m)")
    handler = fn(sat, "void satHandleControlMode(const char* value)")
    restore = fn(sat, "static void satRestoreControlMode()")
    if "static void satPwmSaveStep()" in sat:
        flags += "#define HAS_DEFER 1\n"
        restore += "\n" + fn(sat, "static void satRestoreDeferredStep()") + "\n" + fn(sat, "static void satPwmSaveStep()")
    if not real or not handler:
        raise AssertionError("satSetControlMode / satHandleControlMode not found in %s" % SAT_INO)
    perm = "".join(fn(cyc, h) + "\n" for h in (
        "void satNoteSlaveConfig(uint8_t hb)", "bool satPwmPermitted()", "void satPwmNoteRefused(uint8_t path)",
        "bool satPwmEntryAllowed(uint8_t path)", "bool satCycleCheckLowLoadVeto()"))
    # the old harness sources (before the fix) lack the permission functions: the matching fn() calls return "" then
    return flags + stubs + EXTRA_STUBS + real + "\n" + handler + "\n" + restore + "\n" + body + "\n" + perm + MAIN


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

    # QD-3 B (entry 215): no valid reply = no PWM. The permission matrix on the automatic 60 s switch (QD-1 A, entry 213).
    def test_overshoot_switch_needs_a_valid_reply_with_bit4_clear(self):
        allowed = self.run_sc("overshoot", 1, 0)
        self.assertEqual((allowed["mode"], allowed["ticks"]), ("2", "3"), "control: allowed, switch after 60 s (3rd 30 s tick)")
        denied = self.run_sc("overshoot", 1, 1)
        self.assertEqual((denied["mode"], denied["ticks"]), ("1", "-1"), "bit 4 = 1 must keep SAT continuous")
        unknown = self.run_sc("overshoot", 0, 0)
        self.assertEqual((unknown["mode"], unknown["ticks"]), ("1", "-1"), "no valid reply must keep SAT continuous")

    def test_overshoot_refusal_is_traced_with_reason_and_path(self):
        denied = self.run_sc("overshoot", 1, 1)
        self.assertEqual((denied["reason"], denied["path"]), ("2", "1"), "reason 2 = bit 4 = 1, path 1 = overshoot switch")
        unknown = self.run_sc("overshoot", 0, 0)
        self.assertEqual((unknown["reason"], unknown["path"]), ("1", "1"), "reason 1 = no valid reply yet")
        allowed = self.run_sc("overshoot", 1, 0)
        self.assertEqual((allowed["reason"], allowed["path"]), ("0", "0"), "no refusal when permitted")

    def test_restart_restore_of_pwm_is_blocked_without_permission(self):
        self.assertEqual(self.run_sc("restore", 1, 0, 1)["mode"], "2", "control: saved PWM comes back when permitted")
        for valid, denied, why in ((1, 1, "bit 4 = 1"), (0, 0, "no valid reply yet (a fresh restart)")):
            with self.subTest(why=why):
                r = self.run_sc("restore", valid, denied, 1)
                self.assertEqual(r["mode"], "1", "restore must stay continuous: " + why)
                self.assertEqual(r["path"], "2")
        self.assertEqual(self.run_sc("restore", 1, 1, 0)["reason"], "0", "nothing saved: nothing refused")

    # Entries 121 + 213 + 215 together (our reading): the restore waits for the first valid reply after the boot.
    def test_deferred_restore_first_reply_bit4_clear_restores_pwm(self):
        rows = self.run_sc("deferred", 1, "-", "00")
        self.assertEqual((rows[0]["mode"], rows[0]["saved"]), ("1", "1"), "no reply yet: continuous, the saved flag is kept")
        self.assertEqual((rows[1]["mode"], rows[1]["saved"]), ("2", "1"), "first reply, bit 4 clear: PWM is back")

    def test_deferred_restore_first_reply_bit4_set_keeps_continuous_and_the_flag(self):
        rows = self.run_sc("deferred", 1, "-", "10", "10")
        for r in rows:
            self.assertEqual((r["mode"], r["saved"]), ("1", "1"))
        self.assertEqual((rows[1]["reason"], rows[1]["path"]), ("2", "2"), "refusal traced: bit 4 = 1 on the restore path")

    def test_deferred_restore_no_reply_keeps_continuous_and_does_not_overwrite_the_flag(self):
        rows = self.run_sc("deferred", 1, "-", "-", "-", "-")
        for r in rows:
            self.assertEqual((r["mode"], r["saved"]), ("1", "1"))

    def test_deferred_restore_applies_only_to_the_first_valid_reply(self):
        rows = self.run_sc("deferred", 1, "-", "10", "00", "00")
        self.assertEqual([r["mode"] for r in rows], ["1", "1", "1", "1"], "a later bit 4 = 0 does not restore: the normal entry rules apply")

    def test_nothing_saved_nothing_restored(self):
        rows = self.run_sc("deferred", 0, "-", "00")
        self.assertEqual([(r["mode"], r["saved"]) for r in rows], [("1", "0"), ("1", "0")])

    def test_manual_pwm_command_is_blocked_without_permission(self):
        for value in ("pwm", "2"):
            with self.subTest(value=value):
                self.assertEqual(self.run_sc("command", 1, 0, value)["mode"], "2", "control: permitted")
                for valid, denied in ((1, 1), (0, 0)):
                    r = self.run_sc("command", valid, denied, value)
                    self.assertEqual(r["mode"], "1", "the command must not enter PWM (valid=%d denied=%d)" % (valid, denied))
                    self.assertEqual(r["req"], "2", "the request stays recorded (sat/control_mode)")
                    self.assertEqual(r["path"], "3")

    def test_manual_continuous_and_auto_commands_unchanged(self):
        for value, req in (("continuous", "1"), ("auto", "0")):
            for valid, denied in ((1, 0), (1, 1), (0, 0)):
                with self.subTest(value=value, valid=valid, denied=denied):
                    r = self.run_sc("command", valid, denied, value)
                    self.assertEqual((r["mode"], r["req"], r["reason"]), ("1", req, "0"))

    # "A veto cannot be bypassed by another entry path" (AC2): any other caller of satSetControlMode(PWM) hits the same check.
    def test_any_other_caller_of_set_control_mode_is_blocked(self):
        self.assertEqual(self.run_sc("setmode", 1, 0)["mode"], "2", "control")
        for valid, denied in ((1, 1), (0, 0)):
            self.assertEqual(self.run_sc("setmode", valid, denied)["mode"], "1")

    # QD-2 A (entry 214): bit 4 turning 1 while PWM runs leaves PWM in that tick.
    def test_bit4_rising_during_pwm_leaves_pwm_in_that_tick(self):
        rows = self.run_sc("rise")
        allowed, denied = rows[0], rows[1]
        self.assertEqual(allowed["mode"], "2", "permitted PWM keeps running")
        self.assertEqual(denied["mode"], "1", "bit 4 = 1 must end PWM in the same tick")
        self.assertEqual((denied["reason"], denied["path"]), ("2", "4"))

    def test_veto_exit_does_not_touch_a_running_pwm_with_permission(self):
        r = self.run_sc("veto", 1, 0)
        self.assertEqual((r["mode"], r["reason"]), ("2", "0"))
        for valid, denied in ((1, 1), (0, 0)):
            self.assertEqual(self.run_sc("veto", valid, denied)["mode"], "1")

    # When bit 4 returns to 0 the entry paths simply work again by their own rules: the automatic switch needs a fresh 60 s.
    def test_after_bit4_clears_the_automatic_switch_needs_a_fresh_60s(self):
        rows = self.run_sc("rise")
        self.assertEqual(len(rows), 4)
        self.assertEqual((rows[2]["while_denied_ticks"], rows[2]["mode"]), ("-1", "1"), "no switch while bit 4 = 1")
        self.assertEqual((rows[3]["after_clear_ticks"], rows[3]["mode"]), ("3", "2"), "60 s after the permission is back, not earlier")

    # The decoder: valid replies set the state, the change time moves only on a change.
    def test_decoder_hook_stores_bit4_and_the_time_of_the_change(self):
        rows = self.run_sc("decode", "00", "00", "10", "10", "EF", "FF")
        self.assertEqual([(r["valid"], r["denied"], r["permitted"]) for r in rows],
                         [("1", "0", "1"), ("1", "0", "1"), ("1", "1", "0"), ("1", "1", "0"), ("1", "0", "1"), ("1", "1", "0")])
        self.assertEqual([r["change"] for r in rows], ["1001000", "1001000", "1003000", "1003000", "1005000", "1006000"])

    # --- source-structure checks of what the host cannot run ---

    def test_decoder_feeds_only_the_boilers_valid_read_ack(self):
        core = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = re.search(r"void print_slavememberid\(uint16_t& value\)\n\{.*?\n\}", core, re.S).group(0)
        self.assertRegex(body, re.compile(r"if \(is_value_valid\(OTdata, OTlookupitem\)\)\{.*?if \(OTdata\.rsptype == OTGW_BOILER\) satNoteSlaveConfig\(OTdata\.valueHB\);", re.S))

    def test_permission_does_not_read_the_heating_source(self):
        # ADR-157: manual gas/HP/hybrid selection stays authoritative; the bit is a permission, not a source detector
        for head in ("bool satPwmPermitted()", "void satNoteSlaveConfig(uint8_t hb)", "bool satCycleCheckLowLoadVeto()"):
            body = sustain._fn(self.cyc, head)
            self.assertTrue(body, head)
            self.assertNotRegex(body, r"(?i)source|heating_?system|hybrid|bHeatPump")
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertNotRegex(sustain._fn(sat, "static void satRestoreControlMode()"), r"(?i)satGetEffectiveHeatingSource|settings\.sat\.iHeatingSource")

    def test_control_loop_asks_the_veto_before_the_overshoot_switch(self):
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = sat.index("satCycleCheckLowLoadVeto();")
        b = sat.index("satCycleCheckOvershootSwitch(pidOutput, _sat_tickMs);")
        c = sat.index("satPwmSchedule(pidOutput);", b)
        self.assertLess(a, b)
        self.assertLess(b, c)

    def test_every_pwm_mode_entry_asks_the_permission(self):
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        calls = re.findall(r"satSetControlMode\(SAT_MODE_PWM\)", sat + self.cyc)
        # direct callers: the restore (at init and deferred) and the overshoot switch (the command passes the mode through a ternary, checked below); each asks satPwmEntryAllowed(), and satSetControlMode() checks again
        self.assertEqual(len(calls), 3, calls)
        for text in (sustain._fn(sat, "static void satRestoreControlMode()"), sustain._fn(self.cyc, "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)")):
            self.assertIn("satPwmEntryAllowed(", text)
        self.assertIn("satPwmEntryAllowed(3) ? SAT_MODE_PWM", sat)


if __name__ == "__main__":
    unittest.main()
