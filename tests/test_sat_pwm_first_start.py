#!/usr/bin/env python3
"""TASK-3151.2, ADR-305 (entries 517, 519, 520): a SAT heating start runs in CONTINUOUS. PWM comes only from the overshoot rules.

This file was the PWM-first test of Sergeant D entries 385 and 387 (2026-11-16): every heating start entered PWM and a 60 s start gate
silenced the PWM -> continuous exits. ADR-305 replaces ADR-252 in full. Thermo-nova starts in continuous (heating_control.py:229-231:
without heating demand PWM is off, so every start begins with PWM disabled) and reaches PWM only through the sustained overshoot
(heating_control.py:262-305) or an OVERSHOOT end-of-cycle verdict (pwm.py:219-227). The inverted cases below pin that.

REAL CODE: the whole SATcontrol.ino / SATcycles.ino / SATpid.ino and the PIC command queue, driven by tests/sat_loop_build.py and the
stub-clock driver tests/sat_loop_driver.inc (see tests/test_sat_loop_integration.py for what is real and what is a stub). This file adds
five driver commands by text patch of an in-memory copy of the driver (the shared driver file is not edited):
  mutecfg 1|0   the boiler stops/starts answering MsgID 3
  noflame 1|0   the burner cannot ignite
  holdflame 1|0 the burner stays lit whatever CS/CH say (a boiler that was already burning when SAT came on)
  burn          the burner is lit at the start of the run, flow 50 C, ready for holdflame
  cmode <m>     the control_mode command (auto, continuous, pwm) through satHandleControlMode()
The boiler is the integration driver's first-order model, a plausible combi boiler, not a measurement.

What "continuous" shows on the wire in this driver: the MODE event stays 1 and every MM= command is MM=100 (the maximum relative
modulation; a PWM stint sends its own MM values).

Old-vs-fix: extract the parent of the first ADR-305 commit (`git archive f3a3ff52c~1 src tests`), copy this file over its tests/ copy
and run it there: eight cases fail (a, a2, b, c, d, e, f, i), g and g2 pass on both (SAT coming on mid-burn never entered PWM). Do not use
OTGW_SAT_REF for this: the old SAT sources do not compile against the new type headers. The mutants edit the new code in memory (they put
a PWM start back); they are skipped under OTGW_SAT_REF.

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
    d.mkdir(exist_ok=True)   # TASK-3020: a failed or skipped first build leaves the directory
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


def pwm_entries(r, a=0, b=10 ** 12):
    return [t for t, m in r.modes(a, b) if m == 2]


def assert_continuous_start(tc, r, what):
    """No PWM mode at any time, and MM stays at the maximum relative modulation of the continuous mode."""
    tc.assertEqual(pwm_entries(r), [], "PWM entered %s:\n%s" % (what, r.dump()))
    mm = [x for _, x in r.tx(0, 10 ** 12, "MM=")]
    tc.assertTrue(mm, "no MM= command sent at all:\n" + r.dump())
    tc.assertEqual(set(mm), {"MM=100"}, "MM left the continuous value %s:\n%s" % (what, r.dump()))


class ContinuousFirstStart(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = build("base")

    # (a) auto: the heater going OFF -> ON starts in continuous, the flame lights in continuous
    def test_a_heater_off_to_on_stays_continuous(self):
        r = Run(self.exe, OFF + ["run 150000", "mark on", "room 15", "run 300000"])   # OFF first: the room is known before the first tick
        on = r.mark("on")
        assert_continuous_start(self, r, "at the heating start")
        self.assertTrue(r.flame_on(on), "the start never lit the burner:\n" + r.dump())
        self.assertEqual([m for _, m in r.modes(on)], [], "the mode changed after the heating start:\n" + r.dump(on))

    # (a2) requested mode 'pwm' before the start: the heater OFF ends PWM (ADR-199, thermo-nova pwm.disable()), and the next heating
    # start is again in continuous. (The control_mode command itself is removed in a later release, entry 519.)
    def test_a2_requested_pwm_ends_with_the_heater_off_and_the_start_is_continuous(self):
        r = Run(self.exe, OFF + ["cmode pwm", "run 150000", "mark on", "room 15", "run 300000"])
        on = r.mark("on")
        self.assertEqual(r.modes(60001, on), [], "mode changed while the heater was OFF after the first tick:\n" + r.dump(0, on))
        self.assertEqual(r.modes(0, on)[-1][1], 1, "PWM left on while the heater was OFF:\n" + r.dump(0, on))
        self.assertEqual(pwm_entries(r, on), [], "the heating start entered PWM:\n" + r.dump(on))
        self.assertTrue(r.flame_on(on), r.dump())

    # (i) a boiler without a return temperature (MsgID 28 Unknown-DataID) gets the start-up flood (flame-off hold CS and PM=25 for
    # fModSupDelay, TASK-1371) at the CH flame edge, in continuous. Right after a boot there is no flow temperature yet (entry 388:
    # nothing runs in the 60 s boot silence); the start waits for it without any PWM idle state.
    def run_flood_case(self, r):
        assert_continuous_start(self, r, "at the start of a boiler without a return temperature")
        fl = r.flame_on(0)
        self.assertTrue(fl, r.dump())
        starts = [t for t, x in r.kind("FLOOD") if x == "1"]
        self.assertTrue(starts and 0 <= starts[0] - fl[0] <= 2000, "no start-up flood at the flame edge of the start (flame %s, flood %s):\n%s" % (fl, starts, r.dump()))
        self.assertGreaterEqual(len(r.tx(starts[0], starts[0] + 20000, "PM=25")), 8)

    def test_i_no_return_start_floods_at_the_flame_edge_in_continuous(self):
        self.run_flood_case(Run(self.exe, ["ret28unknown 1", "mute25 1", "run 70000", "mute25 0", "run 260000"]))

    # (b) MsgID 3 plays no part in the mode (entries 392/393): a boiler that sets the low-off bit (HB bit 4) gets the same start
    def test_b_msgid3_low_off_bit_changes_nothing(self):
        self.run_flood_case(Run(self.exe, ["lowoff 1", "ret28unknown 1", "run 200000"]))

    # (c) a boot whose MsgID 3 is never answered starts in continuous after the 60 s boot silence (entry 388)
    def test_c_boot_without_any_msgid3_reply_is_continuous(self):
        r = Run(self.exe, ["mutecfg 1", "run 200000"])
        assert_continuous_start(self, r, "at a boot without a MsgID 3 reply")
        self.assertTrue(r.flame_on(0), r.dump())

    # (d) cold start with the request at the maximum: continuous from the first tick, no PWM stint, no exit to wait for. thermo-nova
    # disables PWM at a request at or above the maximum (heating_control.py:203-204), now without the 60 s start gate (ADR-305 rule 3).
    def test_d_request_at_the_maximum_is_continuous_from_the_start(self):
        r = Run(self.exe, COLD + ["run 240000"])
        assert_continuous_start(self, r, "at a request at the maximum")
        self.assertEqual([m for _, m in r.modes()], [1], "expected one continuous mode event:\n" + r.dump())
        self.assertTrue(r.flame_on(0), r.dump())

    # (e) a start whose burner does not light: nothing enters PWM while it waits (the old saturation count ran from a PWM entry at
    # the start), and the late ignition is a continuous start too
    def test_e_start_without_a_flame_stays_continuous(self):
        r = Run(self.exe, OFF + ["noflame 1", "run 60000", "room 15", "run 700000", "mark lit", "noflame 0", "run 900000"])
        lit = r.mark("lit")
        assert_continuous_start(self, r, "while the burner did not light")
        self.assertTrue(r.flame_on(lit), r.dump())

    # (f) a low-off bit that appears in the middle of a burn changes nothing (the mid-burn switch of the old PWM-first start is gone):
    # one burn, one start-up flood, no mode event, no second PM=25
    def test_f_mid_burn_low_off_bit_runs_no_second_start_sequence(self):
        r = Run(self.exe, OFF + ["ret28unknown 1", "run 40000", "room 15", "run 125000", "mark veto", "lowoff 1", "run 330000"])
        self.assertEqual([x for _, x in r.modes()], [1], "expected one continuous mode event: %r\n%s" % (r.modes(), r.dump()))
        fl = r.flame_on(0)
        self.assertEqual(len(fl), 1, "more than one ignition in the run, the case needs one burn:\n" + r.dump())
        starts = [t for t, x in r.kind("FLOOD") if x == "1"]
        self.assertEqual(len(starts), 1, "the start-up flood ran %d times in one burn: %r\n%s" % (len(starts), starts, r.dump()))
        self.assertEqual(r.tx(r.mark("veto"), 10 ** 12, "PM=25"), [], "PM=25 of a start-up flood after the low-off bit:\n" + r.dump())

    # (g) SAT switched on while a flame already burns: no PWM entry (passes on the old code too; the guard is _sat_flameOffSeen)
    def test_g_enabled_mid_burn_stays_continuous(self):
        r = Run(self.exe, ["burn", "holdflame 1", "run 50000"])
        self.assertNotIn(2, [m for _, m in r.modes()], "PWM although SAT came on while the flame burned:\n" + r.dump())

    # (g2) a pause (OpenTherm loss) after which SAT resumes while the boiler burns is no start either
    G2 = OFF + ["run 150000", "link 0", "burn", "holdflame 1", "run 250000", "room 15", "link 1", "run 330000"]

    def test_g2_resume_after_a_pause_mid_burn_stays_continuous(self):
        r = Run(self.exe, self.G2)
        self.assertNotIn(2, [m for _, m in r.modes()], "PWM although SAT resumed while the flame burned:\n" + r.dump())


@unittest.skipIf(REF, "the mutants edit the new code; they have no target in the reference source")
class Mutants(unittest.TestCase):
    """A rewritten case must still be able to fail: put a PWM start back in memory and the cases of ContinuousFirstStart go red."""

    # the heating-start entry of ADR-252 in its simplest form: heating demand and continuous -> PWM
    MUT = {"SATcontrol.ino": [("  if (state.sat.bModSuppressed) { if (state.sat.iModSuppressionSinceMs == 0)",
                               "  if (!coldCutoff && state.sat.eControlMode == SAT_MODE_CONTINUOUS) satSetControlMode(SAT_MODE_PWM);   // MUTANT\n"
                               "  if (state.sat.bModSuppressed) { if (state.sat.iModSuppressionSinceMs == 0)")]}

    def red(self, name, label):
        t = ContinuousFirstStart(name)
        t.exe = build("pwm_start_mut", self.MUT)
        with self.assertRaises(AssertionError) as cm:
            getattr(t, name)()
        print("\nMUTANT %s RED: %s" % (label, " ".join(str(cm.exception).split())[:200]))

    def test_a_mutant_pwm_at_the_heating_start_is_caught(self):
        self.red("test_a_heater_off_to_on_stays_continuous", "a")

    def test_d_mutant_pwm_at_the_maximum_request_is_caught(self):
        self.red("test_d_request_at_the_maximum_is_continuous_from_the_start", "d")

    def test_i_mutant_pwm_at_a_no_return_start_is_caught(self):
        self.red("test_i_no_return_start_floods_at_the_flame_edge_in_continuous", "i")


if __name__ == "__main__":
    unittest.main(verbosity=2)
