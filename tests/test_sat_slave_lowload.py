#!/usr/bin/env python3
"""TASK-1412.4: host test for the slave low-load permission (OpenTherm MsgID 3, HB bit 4) in SAT.

Decisions (docs/sat/decisions/sergeant-d-corrections.md): entry 213 (QD-1 A: bit 4 = 1 blocks every PWM entry path: the 60 s overshoot
switch, PWM restored after a restart, the manual pwm command, the future end-of-cycle switch; SAT stays continuous), entry 214
(QD-2 A: bit 4 turning 1 while PWM runs leaves PWM in that tick, the continuous setpoint goes out as with the underheat exit),
entry 215 (QD-3 B: no PWM until a valid MsgID 3 reply) and ADR-157.
TASK-2000 (2026-11-16): entry 392 ("Don't look at bit 4 at all ... boiler should be able to enter PWM") reverses entries 213 and
214, and entry 393 (Q-392a A) reverses entry 215: MsgID 3 plays no part in PWM. The cases below now expect PWM for every MsgID 3
state (no reply, bit 4 = 0, bit 4 = 1), no refusal (reason 0), and the reply still recorded (the manual heating source stays authoritative; the bit is a
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
                "static bool g_allow = true;            // TASK-1762 tokens R / P: the MsgID 3 permission refuses / permits the entry\n"
                "static bool satPwmEntryAllowed(uint8_t) { return g_allow; }\n"
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
static void satPwmKeptFreeze() {}   // TASK-3039: the real satSetControlMode() freezes the kept ON time before the reset; not under test here
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
static float g_osFlow = 52.5f;   // 52.5 against 50: above request + margin, below the 3.0 K fail-safe (TASK-1762, entry 429), so the 60 s rule fires;
                                 // scenario failsafe uses 55 (the pre-TASK-1762 input): the 30 s fail-safe fires
static int overshootTicks(uint32_t maxTicks) {   // 30 s ticks, flow above request + margin: returns the tick count at the switch or -1
  state.sat.eControlMode = SAT_MODE_CONTINUOUS; g_flow = g_osFlow;
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
  if (!strcmp(sc, "failsafe")) {           // TASK-1762: the 30 s flame-out fail-safe through the same permission
    g_osFlow = 55.0f;
    const int n = overshootTicks(20);
    printf("ticks=%d ", n); report("failsafe");
  } else if (!strcmp(sc, "overshoot")) {   // the automatic 60 s switch
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
    gate = ""
    if "static const uint32_t SAT_START_GATE_MS" in sat:   # TASK-1942: state the cut functions touch; the gate itself is stubbed in sustain.STUBS
        gate = ("static bool _sat_startGate = false, _sat_startPwmWanted = false, _sat_heaterOn = true;\n"
                "static uint32_t _sat_startFlameOnMs = 0;\nstatic void satStartGateOpen() {}\n")
        if "satGetFlowTemp()" in restore and "satGetFlowTemp()" not in stubs + EXTRA_STUBS:
            gate += "static float satGetFlowTemp() { return 50.0f; }\n"
    return flags + stubs + EXTRA_STUBS + gate + real + "\n" + handler + "\n" + restore + "\n" + body + "\n" + perm + MAIN


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

    # QD-3 B (entry 215): no valid reply = no PWM. The permission matrix on the automatic 60 s switch (QD-1 A, entry 213; 60 s since TASK-2005, entry 407).
    def test_overshoot_switch_ignores_msgid3(self):
        allowed = self.run_sc("overshoot", 1, 0)
        self.assertEqual((allowed["mode"], allowed["ticks"]), ("2", "3"), "control: allowed, switch after 60 s (3rd 30 s tick)")
        for valid, denied, why in ((1, 1, "bit 4 = 1 (Vitodens 100-W, entry 392)"), (0, 0, "no valid reply yet (entry 393)")):
            r = self.run_sc("overshoot", valid, denied)
            self.assertEqual((r["mode"], r["ticks"], r["reason"]), ("2", "3", "0"), why)

    # TASK-1762: the old input of the case above (flow 55 against 50, 5 K over) now switches through the flame-out fail-safe after
    # 30 s (2nd 30 s tick), and the MsgID 3 permission (entry 213, real satPwmEntryAllowed) blocks it as it blocks the 60 s rule.
    def test_failsafe_ignores_msgid3(self):
        allowed = self.run_sc("failsafe", 1, 0)
        self.assertEqual((allowed["mode"], allowed["ticks"]), ("2", "2"), "allowed: the fail-safe switches after 30 s (2nd 30 s tick)")
        for valid, denied in ((1, 1), (0, 0)):
            r = self.run_sc("failsafe", valid, denied)
            self.assertEqual((r["mode"], r["ticks"], r["reason"], r["path"]), ("2", "2", "0", "0"), "entries 392/393")

    def test_overshoot_has_no_refusal_for_any_msgid3_state(self):
        for valid, denied in ((1, 1), (0, 0)):
            r = self.run_sc("overshoot", valid, denied)
            self.assertEqual((r["reason"], r["path"]), ("0", "0"), "entries 392/393: no refusal for any MsgID 3 state")
        allowed = self.run_sc("overshoot", 1, 0)
        self.assertEqual((allowed["reason"], allowed["path"]), ("0", "0"), "no refusal when permitted")

    def test_restart_restore_of_pwm_ignores_msgid3(self):
        # TASK-1942 (entries 385/387 reason 1): initSAT() no longer applies the saved PWM while the heater may be OFF; it arms the
        # deferred restore, which brings PWM back on the first pass with a running heater and a known flow, without waiting for
        # MsgID 3 (entries 392/393; the deferred_restore cases below pin that). At init: continuous, nothing refused, whatever MsgID 3 says.
        for valid, denied, why in ((1, 0, "permitted"), (1, 1, "bit 4 = 1"), (0, 0, "no valid reply yet (a fresh restart)")):
            with self.subTest(why=why):
                r = self.run_sc("restore", valid, denied, 1)
                self.assertEqual((r["mode"], r["reason"]), ("1", "0"), "entries 392/393 + TASK-1942: armed, nothing refused: " + why)
        self.assertEqual(self.run_sc("restore", 1, 1, 0)["reason"], "0", "nothing saved: nothing refused")

    # Entries 121 + 213 + 215 together (our reading): the restore waits for the first valid reply after the boot.
    # Entry 393: the restore no longer waits for a MsgID 3 reply, so PWM is back from the first pass after the restart.
    def test_deferred_restore_first_reply_bit4_clear_restores_pwm(self):
        rows = self.run_sc("deferred", 1, "-", "00")
        self.assertEqual([(r["mode"], r["saved"]) for r in rows], [("2", "1"), ("2", "1")], "PWM is back before any reply")

    def test_deferred_restore_bit4_set_restores_pwm(self):
        # the Vitodens case (entry 392): bit 4 = 1 in every reply, PWM restored all the same, no refusal
        rows = self.run_sc("deferred", 1, "-", "10", "10")
        self.assertEqual([(r["mode"], r["saved"], r["reason"]) for r in rows], [("2", "1", "0")] * 3)

    def test_deferred_restore_no_reply_still_restores_pwm(self):
        rows = self.run_sc("deferred", 1, "-", "-", "-", "-")
        self.assertEqual([(r["mode"], r["saved"]) for r in rows], [("2", "1")] * 4, "entry 393: no MsgID 3 reply needed")

    def test_deferred_restore_applies_only_to_the_first_valid_reply(self):
        rows = self.run_sc("deferred", 1, "-", "10", "00", "00")
        self.assertEqual([r["mode"] for r in rows], ["2", "2", "2", "2"], "restored at once; later replies change nothing")

    def test_nothing_saved_nothing_restored(self):
        rows = self.run_sc("deferred", 0, "-", "00")
        self.assertEqual([(r["mode"], r["saved"]) for r in rows], [("1", "0"), ("1", "0")])

    def test_manual_pwm_command_ignores_msgid3(self):
        for value in ("pwm", "2"):
            with self.subTest(value=value):
                for valid, denied in ((1, 0), (1, 1), (0, 0)):
                    r = self.run_sc("command", valid, denied, value)
                    self.assertEqual((r["mode"], r["req"], r["reason"]), ("2", "2", "0"), "entries 392/393 (valid=%d bit4=%d)" % (valid, denied))

    def test_manual_continuous_and_auto_commands_unchanged(self):
        for value, req in (("continuous", "1"), ("auto", "0")):
            for valid, denied in ((1, 0), (1, 1), (0, 0)):
                with self.subTest(value=value, valid=valid, denied=denied):
                    r = self.run_sc("command", valid, denied, value)
                    self.assertEqual((r["mode"], r["req"], r["reason"]), ("1", req, "0"))

    # "A veto cannot be bypassed by another entry path" (AC2): any other caller of satSetControlMode(PWM) hits the same check.
    def test_any_other_caller_of_set_control_mode_enters_pwm(self):
        for valid, denied in ((1, 0), (1, 1), (0, 0)):
            self.assertEqual(self.run_sc("setmode", valid, denied)["mode"], "2", "entries 392/393")

    # Entry 392 reverses entry 214 (QD-2 A): bit 4 turning 1 while PWM runs no longer leaves PWM.
    def test_bit4_rising_during_pwm_does_not_leave_pwm(self):
        rows = self.run_sc("rise")
        allowed, bit4 = rows[0], rows[1]
        self.assertEqual(allowed["mode"], "2", "PWM keeps running")
        self.assertEqual((bit4["mode"], bit4["reason"], bit4["path"]), ("2", "0", "0"), "bit 4 = 1 must not end PWM")

    def test_running_pwm_check_never_ends_pwm(self):
        for valid, denied in ((1, 0), (1, 1), (0, 0)):
            r = self.run_sc("veto", valid, denied)
            self.assertEqual((r["mode"], r["reason"]), ("2", "0"), "entries 392/393: the running-PWM check never ends PWM")

    # Entry 392: the automatic switch works the same with bit 4 = 1 and = 0 (60 s, TASK-2005, entry 407).
    def test_automatic_switch_ignores_bit4(self):
        rows = self.run_sc("rise")
        self.assertEqual(len(rows), 4)
        self.assertEqual((rows[2]["while_denied_ticks"], rows[2]["mode"]), ("3", "2"), "bit 4 = 1: the switch fires after 60 s")
        self.assertEqual((rows[3]["after_clear_ticks"], rows[3]["mode"]), ("3", "2"), "bit 4 = 0: the same 60 s")

    # The decoder: valid replies set the state, the change time moves only on a change.
    def test_decoder_hook_stores_bit4_and_the_time_of_the_change(self):
        rows = self.run_sc("decode", "00", "00", "10", "10", "EF", "FF")
        self.assertEqual([(r["valid"], r["denied"], r["permitted"]) for r in rows],
                         [("1", "0", "1"), ("1", "0", "1"), ("1", "1", "1"), ("1", "1", "1"), ("1", "0", "1"), ("1", "1", "1")])   # recorded, always permitted
        self.assertEqual([r["change"] for r in rows], ["1001000", "1001000", "1003000", "1003000", "1005000", "1006000"])

    # --- source-structure checks of what the host cannot run ---

    def test_decoder_feeds_only_the_boilers_valid_read_ack(self):
        core = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = re.search(r"void print_slavememberid\(uint16_t& value\)\n\{.*?\n\}", core, re.S).group(0)
        # TASK-1857/TASK-1903: only a real boiler Read-Ack counts, not an OT-Direct loopback answer or a replayed line.
        self.assertRegex(body, re.compile(r"if \(is_value_valid\(OTdata, OTlookupitem\)\)\{.*?if \(OTdata\.rsptype == OTGW_BOILER && boilerEvidence\) satNoteSlaveConfig\(OTdata\.valueHB\);", re.S))
        self.assertRegex(body, r"const bool boilerEvidence = !OTdata\.bLocalAnswer && !OTdata\.bReplayed;")

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
        # direct callers: the restore (at init and deferred), the overshoot switch and the end-of-cycle switch (TASK-1331) (the command passes the mode through a ternary, checked below); each asks satPwmEntryAllowed(), and satSetControlMode() checks again
        self.assertEqual(len(calls), 4, calls)
        # TASK-1942: the restore enters PWM in its deferred step (not at init); the heating start satPwmFirstStart() relies on the
        # last-line check inside satSetControlMode() itself, pinned below
        self.assertIn("satPwmPermitted()", sustain._fn(sat, "static void satSetControlMode(SATControlMode m)"))
        for text in (sustain._fn(sat, "static void satRestoreDeferredStep()"), sustain._fn(self.cyc, "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)"),
                     sustain._fn(self.cyc, "static void satCycleApplyEndOfCycle(SATCycleClass cls, bool pwmEnabled)")):
            self.assertIn("satPwmEntryAllowed(", text)
        self.assertIn("satPwmEntryAllowed(3) ? SAT_MODE_PWM", sat)


if __name__ == "__main__":
    unittest.main()
