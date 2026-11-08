#!/usr/bin/env python3
"""TASK-1412.13 (LL-014, AC2 and AC3): every CS= and SH= SAT transmits stays at or below the effective approved ceiling,
and the user limit is the only MaxTSet value that is persisted.

Ceiling = satEffectiveMaxSetpoint(): the boiler's own MaxTSet (MsgID 57 readback), the user or default cap of entries 148/152,
the heat pump cap, the hard ceiling (underfloor 50, radiators 80) and the global cap while no value is chosen.

The real functions are cut out of src/OTGW-firmware/SATcontrol.ino and compiled with a host C++ compiler (nothing re-implemented):
the MaxTSet block with satEffectiveMaxSetpoint() and satThermalSafeSetpoint(), satPwmFlameOffHold() (ignition hold and start-up
flood), satPwmSuppressionSetpoint() (PWM ON phase, 3 s refresh, flood hand-over) and satCapRepeatCS() (5 s resend, PIC keepalive).
A sweep over user value, system, source, MsgID 49 bounds, boiler MaxTSet, global cap, flow and return temperature checks every
value each of those paths can produce against the ceiling. The control tick itself is too large to compile on the host: its
clamps and the inventory of every CS= sender are checked on the source text, and the test says so.

Mutant check: the same harness run on a copy with one cap removed must report violations, so the sweep can see a defect.

Old-vs-fix: OTGW_SAT_INO points at another copy of SATcontrol.ino.
Run: python tests/test_sat_ceiling_paths.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_max_tset as mt  # noqa: E402  (stubs and the real MaxTSet block cut)

REPO = mt.REPO
FW = mt.FW
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
SETTINGS_INO = Path(os.environ.get("OTGW_SETTINGS_INO", FW / "settingStuff.ino"))

EXTRA_STUBS = r"""
#define SAT_MIN_SETPOINT 10.0f
static bool g_noReturn = false; static float g_flow = 0, g_ret = 0;
static bool satNoReturnTemp() { return g_noReturn; }
static float satGetFlowTemp() { return g_flow; }
static float satGetReturnTemp() { return g_ret; }
static float satGetColdSetpoint() { return satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR ? 21.0f : 28.2f; }
"""

MAIN = r"""
static int g_viol = 0, g_checks = 0;
static void chk(const char* what, float v, float ceil) {
  g_checks++;
  if (v > ceil + 0.001f) { g_viol++; if (g_viol <= 5) printf("VIOLATION %s %.1f > %.1f user=%.0f sys=%d src=%d raw=%d boiler=%.0f flow=%.0f ret=%.0f\n", what, v, ceil,
        settings.sat.fMaxTSetUser, settings.sat.iHeatingSystem, settings.sat.iHeatingSource, (int)OTcurrentSystemState.MaxTSetUBMaxTSetLB, OTcurrentSystemState.MaxTSet, g_flow, g_ret); }
}
int main() {
  const float users[] = {0, 20, 25, 40, 55, 70, 90};
  const int systems[] = {SAT_HSYS_RADIATORS, SAT_HSYS_UNDERFLOOR};
  const int sources[] = {SAT_SRC_GAS_BOILER, SAT_SRC_HEAT_PUMP};
  const uint16_t raws[] = {0, (uint16_t)((55 << 8) | 30), (uint16_t)((90 << 8) | 20), (uint16_t)((30 << 8) | 70)};
  const float boilers[] = {0, 35, 55, 80};
  const float globs[] = {65, 50, 10};
  const float temps[] = {0, 20, 45, 70, 95};
  const float offs[] = {0, 10, 18};
  int shChosen = 0;
  for (float u : users) for (int sy : systems) for (int so : sources) for (uint16_t r : raws) for (float b : boilers) for (float g : globs)
  for (float fl : temps) for (float rt : temps) for (float off : offs) for (int nr = 0; nr < 2; nr++) {
    settings = Settings(); OTcurrentSystemState = OTSys();
    settings.sat.fMaxTSetUser = u; settings.sat.iHeatingSystem = sy; settings.sat.iHeatingSource = so; settings.sat.fMaxSetpoint = g;
    OTcurrentSystemState.MaxTSetUBMaxTSetLB = r; OTcurrentSystemState.MaxTSet = b;
    g_flow = fl; g_ret = rt; g_noReturn = nr != 0; g_flameOff = off; g_modSup = off;
    const float eff = satEffectiveMaxSetpoint();
    chk("ignition hold / flood step (satPwmFlameOffHold)", satPwmFlameOffHold(eff), eff);
    chk("PWM ON suppression / 3 s refresh / flood hand-over (satPwmSuppressionSetpoint)", satPwmSuppressionSetpoint(eff), eff);
    chk("thermal safe fallback (satThermalSafeSetpoint)", satThermalSafeSetpoint(), eff);
    state.sat.fLastSentCS = 200.0f; chk("5 s resend / PIC keepalive (satCapRepeatCS)", satCapRepeatCS(200.0f), eff);
    state.sat.fLastSentCS = 30.0f;  chk("repeat of a CS below the ceiling", satCapRepeatCS(30.0f), eff < 30.0f ? eff : 30.0f);
    bool chosen = false;
    const float sh = satMaxTSetBoilerValue(u, sy == SAT_HSYS_UNDERFLOOR, r, satGlobalMaxSetpoint(), &chosen);
    const float hard = sy == SAT_HSYS_UNDERFLOOR ? SAT_HARD_MAX_FLOOR : SAT_HARD_MAX_RAD;
    chk("SH= above the hard ceiling", sh, hard);
    if (chosen) { shChosen++; chk("CS ceiling above the SH= the boiler was told", eff, sh); }
  }
  printf("checks=%d violations=%d shChosen=%d\n", g_checks, g_viol, shChosen);
  return 0;
}
"""


def fn(src, name):
    m = re.search(r"^static (?:float|inline float) " + name + r"\(.*?\n\}\n", src, re.S | re.M)
    if not m:
        raise AssertionError(name + " not found in SATcontrol.ino")
    return m.group(0)


def build(workdir, sat_text):
    src = sat_text
    consts = "\n".join(re.search(r"^static const float\s+" + n + r"\b.*$", src, re.M).group(0)
                       for n in ("SAT_HARD_MAX_FLOOR", "SAT_HARD_MAX_RAD", "SAT_GLOBAL_MAX_SETPOINT", "SAT_MAX_SETPOINT_DEFAULT",
                                 "SAT_THERMAL_SAFE_FLOW"))
    block = mt.cut(src, "// --- User Max flow setpoint", "// Returns heating curve base offset")
    quant = fn(src, "satQuantizeTenths") if "satQuantizeTenths(" in src else ""   # TASK-1738 helper used by satCapRepeatCS
    funcs = fn(src, "satPwmFlameOffHold") + fn(src, "satPwmSuppressionSetpoint") + quant + fn(src, "satCapRepeatCS")
    stubs = mt.STUBS.replace("@CONSTS@", consts).replace("@TYPES@", "")
    stubs = stubs.replace("struct SatSettings {", "static float g_flameOff = 18, g_modSup = 1;\nstruct SatSettings {")
    stubs = stubs.replace("struct SatState { uint8_t iShVerdict = 0; };", "struct SatState { uint8_t iShVerdict = 0; float fLastSentCS = 0; };")
    # settings.sat.fFlameOffOffset / fModSupOffset are plain fields of the real settings: mirror the sweep values into them
    stubs = stubs.replace("float fMaxTSetUser = 0.0f;", "float fMaxTSetUser = 0.0f; float fFlameOffOffset = 18; float fModSupOffset = 1;")
    funcs = funcs.replace("settings.sat.fFlameOffOffset", "g_flameOff").replace("settings.sat.fModSupOffset", "g_modSup")
    cpp = workdir / "ceiling.cpp"
    cpp.write_text(stubs + EXTRA_STUBS + block + funcs + MAIN, encoding="utf-8")
    return mt.base.compile_cpp(cpp, workdir / "ceiling.exe")


def run(exe):
    return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout


class CeilingPaths(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name), cls.src)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def test_sweep_no_path_value_exceeds_the_effective_ceiling(self):
        out = run(self.exe)
        m = re.search(r"checks=(\d+) violations=(\d+) shChosen=(\d+)", out)
        self.assertIsNotNone(m, out)
        self.assertGreater(int(m.group(1)), 1000000, "sweep too small to mean anything")
        self.assertGreater(int(m.group(3)), 1000, "no chosen-value case swept")
        self.assertEqual(m.group(2), "0", out)

    def test_mutant_without_the_suppression_cap_is_caught(self):
        # one cap removed: the harness must see it (the sweep can fail)
        mutant = self.src.replace("  if (cs < SAT_MIN_SETPOINT) cs = SAT_MIN_SETPOINT;\r\n  if (cs > maxSetpoint) cs = maxSetpoint;\r\n  return cs;",
                                  "  if (cs < SAT_MIN_SETPOINT) cs = SAT_MIN_SETPOINT;\r\n  return cs;", 1)
        mutant = mutant.replace("  if (cs < SAT_MIN_SETPOINT) cs = SAT_MIN_SETPOINT;\n  if (cs > maxSetpoint) cs = maxSetpoint;\n  return cs;",
                                "  if (cs < SAT_MIN_SETPOINT) cs = SAT_MIN_SETPOINT;\n  return cs;", 1)
        self.assertNotEqual(mutant, self.src, "mutation did not apply")
        with tempfile.TemporaryDirectory() as d:
            exe = build(Path(d), mutant)
            out = run(exe)
        self.assertNotIn("violations=0", out)
        self.assertIn("VIOLATION PWM ON suppression", out)

    # --- the control tick and the other senders: source text (the tick is not compilable on the host) ---
    def senders(self):
        calls = []
        for m in re.finditer(r"\b(satSendCS|satEnqueueIfChangedCS)\(([^;]*)\);", self.src):
            line_start = self.src.rfind("\n", 0, m.start()) + 1
            if self.src[line_start:m.start()].lstrip().startswith(("static ", "//")):
                continue
            calls.append((m.group(1), m.group(2).strip()))
        return calls

    def test_inventory_of_cs_senders_is_the_audited_list(self):
        # A new CS= sender must be audited against the ceiling and added here on purpose.
        want = sorted([
            ("satEnqueueIfChangedCS", "cs"),                              # 3 s suppression refresh: cs <= satEffectiveMaxSetpoint() just above
            ("satSendCS", "cs"),                                          # flood hand-over: cs <= satEffectiveMaxSetpoint() just above
            ("satSendCS", "cs"),                                          # flood step: satPwmFlameOffHold(satEffectiveMaxSetpoint())
            ("satSendCS", "satCapRepeatCS(state.sat.fLastSentCS)"),       # 5 s resend
            ("satSendCS", "(float)(int)state.sat.fFinalSetpoint"),        # thermal safe fallback: satThermalSafeSetpoint()
            ("satSendCS", "10.0f"),                                       # summer simmer: SAT_MIN_SETPOINT, below any ceiling
            ("satSendCS", "10.0f"),                                       # valves closed
            ("satSendCS", "finalSetpoint"),                               # main control tick: final clamp
        ])
        self.assertEqual(sorted(self.senders()), want)
        # the PIC keepalive and the CS=0 release are the two other senders, outside satSendCS
        self.assertIn('PSTR("CS=%.1f"), satCapRepeatCS(state.sat.fLastSentCS)', self.src)
        self.assertIn('satEnqueueOwnCS("CS=0")', self.src)

    def test_main_tick_clamps_every_mode_to_the_effective_ceiling_before_it_sends(self):
        s = self.src
        a = s.index("float maxSetpoint = satEffectiveMaxSetpoint();\n  float sysMax = satGetMaxSetpoint();".replace("\n", "\r\n" if "\r\n" in s else "\n"))
        i_pid = s.index("if (pidOutput > maxSetpoint) pidOutput = maxSetpoint;", a)
        i_apply_pwm = s.index("finalSetpoint = satApplyPWM(pidOutput, true);", i_pid)       # ignition hold, PWM ON/OFF
        i_apply_cont = s.index("finalSetpoint = satApplyContinuous(pidOutput, _sat_lastControlCS);", i_pid)   # continuous and recovery after an exit
        i_clamp = s.index("if (finalSetpoint > maxSetpoint) finalSetpoint = maxSetpoint;", max(i_apply_pwm, i_apply_cont))
        i_send = s.index("satSendCS(finalSetpoint);", i_clamp)
        self.assertLess(i_clamp, i_send)
        # the cold cutoff after the clamp only lowers CS to SAT_MIN_SETPOINT
        between = s[i_clamp:i_send]
        self.assertRegex(between, r"finalSetpoint = SAT_MIN_SETPOINT;")
        rhs = set(re.findall(r"\bfinalSetpoint\s*=\s*([^=;]+);", between))
        self.assertEqual(rhs, {"maxSetpoint", "SAT_MIN_SETPOINT"}, "something other than a lowering assigns CS after the ceiling clamp")

    def test_refresh_and_flood_clamp_to_the_effective_ceiling_before_sending(self):
        s = self.src
        a = s.index("float cs = satPwmSuppressionSetpoint(satGetMaxSetpoint());")
        seg = s[a:s.index("satEnqueueIfChangedCS(cs);", a)]
        self.assertRegex(seg, r"maxSetpoint = satEffectiveMaxSetpoint\(\);\s*\n\s*if \(cs > maxSetpoint\) cs = maxSetpoint;")
        b = s.index("float cs = satPwmSuppressionSetpoint(satGetMaxSetpoint());", a + 10)
        seg = s[b:s.index("satSendCS(cs);", b)]
        self.assertRegex(seg, r"maxSetpoint = satEffectiveMaxSetpoint\(\);\s*\n\s*if \(cs > maxSetpoint\) cs = maxSetpoint;")
        self.assertIn("satPwmFlameOffHold(satEffectiveMaxSetpoint())", s)

    def test_fixed_values_are_below_every_possible_ceiling(self):
        # 10.0 (summer simmer, valves closed, PWM OFF) is SAT_MIN_SETPOINT; the lowest ceiling is the user lower bound 20
        self.assertRegex(self.src, r"SAT_MIN_SETPOINT\s*=\s*10\.0f")
        self.assertIn("#define SAT_MAXTSET_MIN 20", (FW / "SATtypes.h").read_text(encoding="utf-8"))


class PersistedLimits(unittest.TestCase):
    """AC3: stored user limits persist; measurements and the SH= verdict do not."""

    @classmethod
    def setUpClass(cls):
        cls.text = SETTINGS_INO.read_text(encoding="utf-8")

    def write_settings(self):
        a = self.text.index("bool writeSettings(bool show)")
        return self.text[a:self.text.index("\n}\n", a)]

    def test_user_limit_is_written_and_read_back(self):
        w = self.write_settings()
        self.assertIn('F("SATmaxsetpoint"), settings.sat.fMaxTSetUser', w)
        self.assertRegex(self.text, r'"SATmaxsetpoint"\)\)\s*==\s*0\)\s*settings\.sat\.fMaxTSetUser\s*=\s*parseMaxTSetUser')

    def test_no_measurement_or_transient_state_is_written(self):
        w = self.write_settings()
        for banned in ("MaxTSetUBMaxTSetLB", "OTcurrentSystemState", "iShVerdict", "state.sat.", "state.otBus.", "feedTboiler", "feedTret"):
            self.assertNotIn(banned, w, banned + " must not be persisted as a setting")
        # the only MaxTSet-related keys: the user limit and the pre-SAT restore value of the hand-back (entry 240, TASK-1541)
        keys = sorted(set(re.findall(r'F\("(SAT[A-Za-z0-9]*maxt?set[A-Za-z0-9]*)"\)', w, re.I)))
        self.assertEqual(keys, ["SATmaxsetpoint", "SATpresatmaxtset"])

    def test_verdict_field_is_state_not_settings(self):
        types = (FW / "SATtypes.h").read_text(encoding="utf-8")
        a = types.index("iShVerdict")
        self.assertGreater(types.rfind("struct SATRuntimeSection", 0, a), types.rfind("struct SATSection", 0, a), "iShVerdict belongs to state.sat, not settings.sat")
        self.assertNotIn("iShVerdict", self.text)


if __name__ == "__main__":
    unittest.main()
