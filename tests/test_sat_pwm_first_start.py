#!/usr/bin/env python3
"""Sergeant D entries 385 and 387 (2026-11-16): every SAT heating start runs in PWM first.

Thermo-nova starts in continuous and reaches PWM only through the overshoot switch (heating_control.py:229-231, :262-305; pwm.py:63, :89).
The firmware differs on purpose: a heating start (the heater going OFF -> ON in the control tick, or a boot with no flame burning)
enters PWM; PWM or continuous is chosen only after the start sequence, which ends 60 s after the first CH flame-on of the start
(the 60 s start-up follow of entries 344 and 347). Until then the PWM -> continuous exits stay quiet (max setpoint, underheat,
saturation, the underheat verdict at flame off); the boiler's MsgID 3 veto (entry 214) keeps working.

REAL CODE: the whole SATcontrol.ino / SATcycles.ino / SATpid.ino and the PIC command queue, driven by tests/sat_loop_build.py and the
stub-clock driver tests/sat_loop_driver.inc (see tests/test_sat_loop_integration.py for what is real and what is a stub). This file adds
six driver commands by text patch of an in-memory copy of the driver (the shared driver file is not edited):
  mutecfg 1|0   the boiler stops/starts answering MsgID 3 (the MsgID 3 permission is unknown while muted)
  noflame 1|0   the burner cannot ignite
  holdflame 1|0 the burner stays lit whatever CS/CH say (a boiler that was already burning when SAT came on)
  burn          the burner is lit at the start of the run, flow 50 C, ready for holdflame
  cmode <m>     the control_mode command (auto, continuous, pwm) through satHandleControlMode()
  vetostate     the veto record of the PWM entry gate (path, millis) as one STATE line
The boiler is the integration driver's first-order model, a plausible combi boiler, not a measurement.

Old-vs-fix: OTGW_SAT_REF=HEAD compiles the SAT sources from `git show HEAD:...` instead of the working tree; every case that
describes new behaviour fails there (see the commit notes). Cases (g) and the veto side of (b) describe behaviour that is unchanged or
only recorded differently; (g) has a mutant that drops its guard.

Run: python tests/test_sat_pwm_first_start.py
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
import sat_loop_build as B  # noqa: E402

REF = os.environ.get("OTGW_SAT_REF", "")
_TMP = tempfile.TemporaryDirectory()
_CACHE = {}
SAT_FILES = ("SATcontrol.ino", "SATcycles.ino", "SATpid.ino")


def patched_driver():
    d = B.rd(HERE / "sat_loop_driver.inc")

    def sub(old, new):
        nonlocal d
        assert d.count(old) == 1, old
        d = d.replace(old, new)
    sub("static bool g_linkDown", "static bool g_muteCfg = false, g_noFlame = false, g_holdFlame = false;\nstatic bool g_linkDown")
    sub("case 3:  frame(3", "case 3:  if (g_muteCfg) break; frame(3")
    sub("if (bo.ch && bo.cs > bo.flow + 0.5f &&", "if (!g_noFlame && bo.ch && bo.cs > bo.flow + 0.5f &&")
    sub("} else if (!bo.ch || bo.flow >= bo.cs + bo.hystK) {", "} else if (!g_holdFlame && (!bo.ch || bo.flow >= bo.cs + bo.hystK)) {")
    sub('else if (!strcmp(cmd, "room")) g_room = (float)v;', 'else if (!strcmp(cmd, "room")) { g_room = (float)v; tr(); }')
    sub("    else return 2;", """    else if (!strcmp(cmd, "mutecfg")) g_muteCfg = atoi(a) != 0;
    else if (!strcmp(cmd, "noflame")) g_noFlame = atoi(a) != 0;
    else if (!strcmp(cmd, "holdflame")) g_holdFlame = atoi(a) != 0;
    else if (!strcmp(cmd, "burn")) { bo.flame = true; bo.ch = true; bo.cs = 60.0f; bo.flow = 50.0f; bo.flameChangeMs = g_ms; statusFrame(); }
    else if (!strcmp(cmd, "cmode")) { char m[16]; sscanf(line, "%*s %15s", m); satHandleControlMode(m); }
    else if (!strcmp(cmd, "vetostate")) ev("STATE", "veto path=%u ms=%u valid=%d", (unsigned)state.sat.iPwmVetoPath, (unsigned)state.sat.iPwmVetoMs, (int)state.sat.bSlaveCfgValid);
    else return 2;""")
    return d


def ref_overrides():
    if not REF:
        return {}
    out = {}
    for f in SAT_FILES:
        txt = subprocess.run(["git", "show", "%s:src/OTGW-firmware/%s" % (REF, f)], cwd=B.REPO, capture_output=True, text=True, check=True).stdout
        out[f] = txt.replace("\r\n", "\n")
    return out


def build(name="base", edits=None):
    """edits: {file: [(old, new), ...]} applied to the (working tree or REF) source in memory; each old must occur exactly once."""
    if name in _CACHE:
        return _CACHE[name]
    ov = ref_overrides()
    for f, pairs in (edits or {}).items():
        src = ov[f] if f in ov else B.rd(B.FW / f)
        for old, new in pairs:
            assert src.count(old) == 1, "edit target must occur exactly once in %s: %r (%d)" % (f, old, src.count(old))
            src = src.replace(old, new)
        ov[f] = src
    d = Path(_TMP.name) / name
    d.mkdir()
    (d / "t.cpp").write_text(B.assemble(driver=patched_driver(), overrides=ov), encoding="utf-8")
    rc, out = B.cl_compile(d / "t.cpp", d / "t.exe")
    if rc is None:
        raise unittest.SkipTest("no host C++ compiler (MSVC) found")
    if rc != 0:
        raise RuntimeError("host compile failed for %s:\n%s" % (name, out[-3000:]))
    _CACHE[name] = d / "t.exe"
    return _CACHE[name]


class Run:
    def __init__(self, exe, script):
        out = subprocess.run([str(exe)], input="\n".join(script) + "\n", capture_output=True, text=True, check=True).stdout
        self.ev = []
        for line in out.splitlines():
            m = re.match(r"EV (\d+) (\w+) ?(.*)", line)
            if m:
                self.ev.append((int(m.group(1)), m.group(2), m.group(3)))

    def kind(self, kind, a=0, b=10 ** 12):
        return [(t, x) for t, k, x in self.ev if k == kind and a <= t < b]

    def modes(self, a=0, b=10 ** 12):
        return [(t, int(x)) for t, x in self.kind("MODE", a, b)]

    def tx(self, a=0, b=10 ** 12, prefix=""):
        return [(t, x) for t, k, x in self.ev if k == "TX" and a <= t < b and x.startswith(prefix)]

    def flame_on(self, a=0):
        return [t for t, x in self.kind("BOILER", a) if x.startswith("flame on")]

    def mark(self, tag):
        for t, k, x in self.ev:
            if k == "MARK" and x == tag:
                return t
        raise AssertionError("no mark %s" % tag)

    def state(self, tag):
        for t, k, x in self.ev:
            if k == "STATE" and x.startswith(tag + " "):
                return t, dict(p.split("=") for p in x.split()[1:])
        raise AssertionError("no state %s" % tag)

    def dump(self, a=0, b=10 ** 12, limit=40):
        rows = [e for e in self.ev if a <= e[0] < b and e[1] in ("MODE", "BOILER", "FLOOD", "MARK", "PWMPHASE")]
        return "\n".join("  %d %s %s" % e for e in rows[:limit])


COLD = ["outside -25", "room 5"]        # a request at the maximum setpoint
OFF = ["room 25"]                       # request below the cold setpoint: the heater is OFF


class PwmFirstStart(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = build("base")

    # (a) auto: the heater going OFF -> ON enters PWM before the flame of that start, never while the heater is OFF
    def test_a_heater_off_to_on_enters_pwm_before_the_flame(self):
        r = Run(self.exe, OFF + ["run 150000", "mark on", "room 15", "run 300000"])   # OFF first: the room is known before the first tick
        on = r.mark("on")
        self.assertNotIn(2, [m for _, m in r.modes(0, on)], "PWM mode while the heater was OFF:\n" + r.dump(0, on))
        pwm = [t for t, m in r.modes(on) if m == 2]
        self.assertTrue(pwm, "no PWM entry at the heating start:\n" + r.dump(on))
        self.assertLessEqual(pwm[0] - on, 35000, "PWM entry later than one control tick after the demand came:\n" + r.dump(on))
        fl = r.flame_on(on)
        self.assertTrue(fl and pwm[0] <= fl[0], "the start must be in PWM when its flame lights: pwm=%s flame=%s\n%s" % (pwm, fl, r.dump(on)))

    # (a2) requested mode 'pwm' also gets PWM first: the heater OFF ends PWM (ADR-199), the next start enters it again
    def test_a2_requested_pwm_enters_pwm_at_the_next_start(self):
        r = Run(self.exe, OFF + ["cmode pwm", "run 150000", "mark on", "room 15", "run 300000"])
        on = r.mark("on")
        # entries 392/393: the 'pwm' command now enters PWM at once (no MsgID 3 refusal at boot); the first tick after the 60 s boot
        # silence (entry 388, at 60000) leaves it again because the heater is OFF (ADR-199). From the next tick on nothing may change.
        self.assertEqual([m for t, m in r.modes(60001, on)], [], "mode changed while the heater was OFF after the first tick:\n" + r.dump(0, on))
        self.assertEqual(r.modes(0, on)[-1][1], 1, "PWM left on while the heater was OFF:\n" + r.dump(0, on))
        pwm = [t for t, m in r.modes(on) if m == 2]
        self.assertTrue(pwm, "no PWM entry at the heating start with control_mode pwm:\n" + r.dump(on))
        fl = r.flame_on(on)
        self.assertTrue(fl and pwm[0] <= fl[0], "the start must be in PWM when its flame lights: pwm=%s flame=%s" % (pwm, fl))

    # (i) the first PWM phase of a PWM-first entry is ON in the entry tick, and the no-return start-up flood runs at the ignition.
    # Entry 43 (pwm.py:154-155): the first schedule step after a PWM entry starts ON without the off wait. Entry 387 reason 2: the PWM
    # start is the warm-up of the boiler components. TASK-1371: a boiler without a return temperature (MsgID 28 Unknown-DataID) gets the
    # flood (flame-off hold CS and PM=25 for fModSupDelay) from the CH flame edge of an ON phase. Right after a boot there is no flow
    # temperature yet; PWM would idle and send the plain request (heating_control.py:149-150), the boiler would light on it and no flood
    # would run. The start therefore waits in continuous until the flow is known and enters PWM on a tick that has it.
    def test_i_first_phase_after_the_pwm_first_entry_is_on_and_the_flood_runs(self):
        # entry 388: nothing runs in the 60 s boot silence, so the flow must still be unknown after it for the guard to matter
        self.run_case(Run(self.exe, ["ret28unknown 1", "mute25 1", "run 70000", "mute25 0", "run 260000"]))

    def run_case(self, r):
        pwm = [t for t, m in r.modes() if m == 2]
        self.assertTrue(pwm, "no PWM entry:\n" + r.dump())
        phase = [x for t, x in r.kind("PWMPHASE") if t == pwm[0]]
        self.assertEqual(phase[-1:], ["ON"], "the first phase in the PWM entry tick is %r, not ON:\n%s" % (phase, r.dump()))
        fl = r.flame_on(0)
        self.assertTrue(fl, r.dump())
        self.assertGreaterEqual(fl[0], pwm[0], "the boiler lit before the PWM entry, on a plain request:\n" + r.dump())
        starts = [t for t, x in r.kind("FLOOD") if x == "1"]
        self.assertTrue(starts and 0 <= starts[0] - fl[0] <= 2000, "no start-up flood at the flame edge of the start (flame %s, flood %s):\n%s" % (fl, starts, r.dump()))
        self.assertGreaterEqual(len(r.tx(starts[0], starts[0] + 20000, "PM=25")), 8)

    # (b) entries 392/393: MsgID 3 plays no part in PWM, so a boiler that sets the low-off bit (HB bit 4) still gets the PWM-first
    # start, with its first phase ON and the no-return flood at the ignition (as case i). Before 392 this bit refused the entry.
    def test_b_msgid3_low_off_bit_does_not_refuse_the_pwm_start(self):
        r = Run(self.exe, ["lowoff 1", "ret28unknown 1", "run 200000"])
        self.run_case(r)

    # (c) entries 392/393: a boot whose MsgID 3 is never answered still starts in PWM after the 60 s boot silence (entry 388);
    # before 392 the start waited for the first valid MsgID 3 reply.
    def test_c_boot_pwm_without_any_msgid3_reply(self):
        r = Run(self.exe, ["mutecfg 1", "run 200000"])
        pwm = [t for t, m in r.modes() if m == 2]
        self.assertTrue(pwm, "no PWM-first start while MsgID 3 stays unanswered:\n" + r.dump())
        fl = r.flame_on(0)
        self.assertTrue(fl and pwm[0] <= fl[0], "the start must be in PWM when its flame lights: pwm=%s flame=%s" % (pwm, fl))

    # (d) cold start with the request at the maximum: no PWM -> continuous exit before 60 s after the flame-on; the max exit after
    def test_d_max_exit_waits_for_the_start_sequence(self):
        r = Run(self.exe, COLD + ["run 240000"])
        m = r.modes()
        self.assertTrue([1 for _, x in m if x == 2], "no PWM start at the maximum request:\n" + r.dump())
        t_pwm = [t for t, x in m if x == 2][0]
        fl = r.flame_on(0)
        self.assertTrue(fl, "no flame:\n" + r.dump())
        left = [t for t, x in m if x == 1 and t > t_pwm]
        self.assertTrue(left, "the max-setpoint exit never fired after the start sequence:\n" + r.dump())
        self.assertGreaterEqual(left[0] - fl[0], 60000, "left PWM %d ms after the flame-on, inside the start sequence:\n%s" % (left[0] - fl[0], r.dump()))
        self.assertLessEqual(left[0] - fl[0], 60000 + 35000, "the max exit did not fire on the first tick after the sequence:\n" + r.dump())

    # (e) saturation: a start whose flame does not light holds PWM (the off-time-0 count runs from the start); after the flame the exit works
    def test_e_saturation_exit_blocked_until_the_flame_and_60s(self):
        script = OFF + ["noflame 1", "run 60000", "room 15", "run 700000", "mark lit", "noflame 0", "run 900000"]
        r = Run(self.exe, script)
        lit = r.mark("lit")
        t_pwm = [t for t, x in r.modes() if x == 2]
        self.assertTrue(t_pwm, "no PWM start:\n" + r.dump())
        self.assertEqual([t for t, x in r.modes(t_pwm[0], lit) if x == 1], [], "a PWM -> continuous exit fired while the flame of the start had not lit:\n" + r.dump(0, lit + 1))
        fl = r.flame_on(lit)
        self.assertTrue(fl, r.dump())
        left = [t for t, x in r.modes(lit) if x == 1]
        self.assertTrue(left, "the exit never fired after the start sequence:\n" + r.dump())
        self.assertGreaterEqual(left[0] - fl[0], 60000)
        self.assertLessEqual(left[0] - fl[0], 60000 + 35000, "the exit did not fire on the first tick after the sequence:\n" + r.dump())
        # control: without the guards in satCycleCheckPwmExits() the saturation exit fires long before the flame lit
        nog = build("noguard", {"SATcycles.ino": [("SAT_UNDERHEAT_SUSTAIN_MS) && !satStartGateActive()) {", "SAT_UNDERHEAT_SUSTAIN_MS)) {"),
                                                   ("SAT_SATURATION_SUSTAIN_MS) && !satStartGateActive()) {", "SAT_SATURATION_SUSTAIN_MS)) {")]})
        r2 = Run(nog, script)
        t2 = [t for t, x in r2.modes() if x == 2]
        early = [t for t, x in r2.modes(t2[0], r2.mark("lit")) if x == 1] if t2 else []
        self.assertTrue(early, "control run without the guard did not exit before the flame lit; the test would prove nothing:\n" + r2.dump())

    # (f) PWM -> continuous in the middle of the burn is no new start: no second 348 flood
    def test_f_mid_burn_switch_runs_no_second_start_sequence(self):
        r = Run(self.exe, OFF + ["ret28unknown 1", "run 40000", "room 15", "run 125000", "mark veto", "lowoff 1", "run 330000"])
        m = r.modes()
        self.assertEqual([x for _, x in m], [1, 2, 1], "expected continuous, PWM start, veto back to continuous: %r\n%s" % (m, r.dump()))
        veto = r.mark("veto")
        self.assertGreater(m[2][0], veto)
        fl = r.flame_on(0)
        self.assertEqual(len(fl), 1, "more than one ignition in the run, the case needs one burn:\n" + r.dump())
        starts = [t for t, x in r.kind("FLOOD") if x == "1"]
        self.assertEqual(len(starts), 1, "the start-up flood ran %d times in one burn: %r\n%s" % (len(starts), starts, r.dump()))
        self.assertEqual(r.tx(m[2][0] - 500, 10 ** 12, "PM=25"), [], "PM=25 of a start-up flood after the mid-burn switch:\n" + r.dump())

    # (g) SAT switched on while a flame already burns: unchanged, no PWM entry (mutant below shows the guard is what holds it)
    def test_g_enabled_mid_burn_stays_continuous(self):
        r = Run(self.exe, ["burn", "holdflame 1", "run 50000"])
        self.assertNotIn(2, [m for _, m in r.modes()], "PWM although SAT came on while the flame burned:\n" + r.dump())

    # (g2) a pause (OpenTherm loss) after which SAT resumes while the boiler burns is no start either
    G2 = OFF + ["run 150000", "link 0", "burn", "holdflame 1", "run 250000", "room 15", "link 1", "run 330000"]

    def test_g2_resume_after_a_pause_mid_burn_stays_continuous(self):
        r = Run(self.exe, self.G2)
        self.assertNotIn(2, [m for _, m in r.modes()], "PWM although SAT resumed while the flame burned:\n" + r.dump())

    # (h) removed: the entry 214 low-load veto that ended PWM inside the start sequence no longer exists (entries 392/393,
    # TASK-2000); with entry 394 nothing ends PWM inside the 60 s, which cases d and e pin.


@unittest.skipIf(REF, "the mutants edit the new code; they have no target in the reference source")
class Mutants(unittest.TestCase):
    def test_i_mutant_entering_pwm_without_a_flow_temperature_loses_the_flood(self):
        # the first version of the entry: PWM at the first tick although the flow is unknown (PWM idles, plain request, boiler lights, no flood)
        exe = build("i_mut", {"SATcontrol.ino": [("  if (satGetFlowTemp() <= 0.0f) { _sat_startPwmWanted = true; return; }\n  satSetControlMode(SAT_MODE_PWM);", "  satSetControlMode(SAT_MODE_PWM);")]})
        r = Run(exe, ["ret28unknown 1", "mute25 1", "run 70000", "mute25 0", "run 260000"])
        with self.assertRaises(AssertionError) as cm:
            PwmFirstStart("test_i_first_phase_after_the_pwm_first_entry_is_on_and_the_flood_runs").run_case(r)
        print("\nMUTANT i RED: " + " ".join(str(cm.exception).split())[:300])

    def test_g2_mutant_without_the_flame_reset_in_satdisable_enters_pwm_on_resume(self):
        exe = build("g2_mut", {"SATcontrol.ino": [("  _sat_flameOffSeen = false;           // a pause (OpenTherm loss, trip, fallback lift)", "  // MUTANT (pause)")]})
        r = Run(exe, PwmFirstStart.G2)
        self.assertIn(2, [m for _, m in r.modes()], "mutant should enter PWM on resume mid-burn:\n" + r.dump())

    def test_g_mutant_without_the_flame_off_guard_enters_pwm_mid_burn(self):
        exe = build("g_mut", {"SATcontrol.ino": [("SAT_MODE_CONTINUOUS || !_sat_flameOffSeen) {", "SAT_MODE_CONTINUOUS) {")]})
        r = Run(exe, ["burn", "holdflame 1", "run 120000"])   # past the 60 s boot silence of entry 388
        self.assertIn(2, [m for _, m in r.modes()], "mutant should enter PWM mid-burn:\n" + r.dump())

    def test_e_underheat_exit_is_gated(self):
        # With the start sequence lengthened to 200 s (in memory), the underheat exit (request above flow for 60 s) is held back
        # by the gate and fires only after it. The real 60 s gate cannot show this: the underheat count starts at the first
        # tick with the flame on, so it needs 60 s too.
        edits = {"SATcontrol.ino": [("static const uint32_t SAT_START_GATE_MS = 60000UL;", "static const uint32_t SAT_START_GATE_MS = 200000UL;")]}
        long_exe = build("gate200", edits)
        # TASK-2993 (entries 437/438): room 19.5 gives a request of 42.4 C against a flow of 31-33 C, a real underheat. With the default
        # room the flow passes the request at 180 s and the live-flow duty leaves range 5, so no exit exists to hold back
        # (test_boiler_above_the_request_gets_no_saturation_exit pins that).
        r = Run(long_exe, ["room 19.5", "run 400000"])
        t_pwm = [t for t, x in r.modes() if x == 2]
        fl = r.flame_on(0)
        self.assertTrue(t_pwm and fl, r.dump())
        left = [t for t, x in r.modes() if x == 1 and t > t_pwm[0]]
        self.assertTrue(left, "no exit at all:\n" + r.dump())
        self.assertGreaterEqual(left[0] - fl[0], 200000, "an exit fired %d ms after the flame-on, inside the 200 s gate:\n%s" % (left[0] - fl[0], r.dump()))
        # control: without the gate guards the same run exits earlier
        nog = build("gate200_noguard", {"SATcontrol.ino": edits["SATcontrol.ino"],
                                         "SATcycles.ino": [("SAT_UNDERHEAT_SUSTAIN_MS) && !satStartGateActive()) {", "SAT_UNDERHEAT_SUSTAIN_MS)) {"),
                                                           ("SAT_SATURATION_SUSTAIN_MS) && !satStartGateActive()) {", "SAT_SATURATION_SUSTAIN_MS)) {")]})
        r2 = Run(nog, ["room 19.5", "run 400000"])
        left2 = [t for t, x in r2.modes() if x == 1 and t > [t2 for t2, y in r2.modes() if y == 2][0]]
        self.assertTrue(left2 and left2[0] - r2.flame_on(0)[0] < 200000, "control run without the guard did not exit early:\n" + r2.dump())


class LiveFlowDuty2993(unittest.TestCase):
    def test_boiler_above_the_request_gets_no_saturation_exit(self):
        # TASK-2993 (Sergeant D entries 437/438): the default heating start (request 31.8 C) with the simulated boiler: the flow rises
        # past the request at about 180 s (CS = flow - 1). The start sequence is lengthened to 200 s (in memory) so the underheat exit
        # (request above the flow at 90-150 s) is held back until the flow is above the request; what is left is the saturation exit.
        # The duty uses the live flow, so it leaves range 5 (off time 0) once the flow is above the request and no saturation exit
        # follows; with the held BT (ignition flow ~30 C) the duty stayed at range 5 and the saturation exit ended PWM (the
        # indigo_light 19 Nov 21:32:30 pattern). Red on the code before TASK-2993.
        edits = {"SATcontrol.ino": [("static const uint32_t SAT_START_GATE_MS = 60000UL;", "static const uint32_t SAT_START_GATE_MS = 200000UL;")]}
        r = Run(build("gate200", edits), ["run 400000"])
        pwm = [t for t, x in r.modes() if x == 2]
        self.assertTrue(pwm and r.flame_on(0), r.dump())
        self.assertEqual([t for t, x in r.modes() if x == 1 and t > pwm[0]], [],
                         "PWM ended although the boiler delivers above the request: " + r.dump())


if __name__ == "__main__":
    unittest.main(verbosity=2)
