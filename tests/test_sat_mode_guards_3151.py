#!/usr/bin/env python3
"""TASK-3151.1 (Robert 2026-11-28 "Ik wil dat je 2 en 3 ook implementeert"): the two sustained mode rules carry thermo-nova's guards
inside the functions themselves, so they hold whoever calls them, not only by the order of the calls in the control tick.

thermo-nova 7beaaf5 heating_control.py: _maybe_enable_pwm_on_sustained_overshoot :262-305 (PWM already on :264-266, hot water
now :268-270, within 300 s after it :272-274, heater state ON :276-279, request above COLD_SETPOINT :281-283, flow missing
:284-286) and _maybe_disable_pwm_runtime :307-328 (PWM on :308-311, hot water or 300 s :313-315, heater ON and request above the cold
setpoint :317-319, PWM ON phase :321-323). In the firmware heater ON is: valves open, no summer-simmer pause, and the request above
the per-system cold threshold (satColdThreshold(), ADR-199; thermo-nova uses a fixed 22).

The code under test is the real SATcycles.ino text: the sustain state, satDhwGuardActive(), satHeatingDemandOn(),
satCycleCheckOvershootSwitch() and satCycleCheckPwmExits() are cut out and compiled with a host C++ compiler. Every guard has a
mutant: the same source with that one guard removed. The case must give the guarded result on the real source and the unguarded one on
the mutant, so each test fails without its guard. Without a compiler the compiled cases are skipped.

OTGW_SATCYCLES_INO points at another copy of SATcycles.ino (old-vs-fix).
Run: python tests/test_sat_mode_guards_3151.py
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

CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", REPO / "src" / "OTGW-firmware" / "SATcycles.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define PSTR(x) x
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
static void satNarrate_P(const char*) {}
static void satNarratef_P(const char*, ...) {}
enum SATControlMode { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
struct SatSettings { float fPwmSwitchMargin = 3.0f; bool bSummerSimmer = true; };
struct Settings { SatSettings sat; };
struct SatState { SATControlMode eControlMode = SAT_MODE_CONTINUOUS; bool bDhwActive = false, bPwmFlameRequested = false,
                  bValvesOpen = true, bSummerActive = false; };
struct State { SatState sat; };
static Settings settings; static State state;
static float g_flow = 53.0f;
static float g_cold = 28.2f;
static uint32_t g_offTime = 1;
static float satGetFlowTemp() { return g_flow; }
static bool satIsFlameOn() { return true; }
static float satColdThreshold() { return g_cold; }
static uint32_t satPwmLastOffTimeMs() { return g_offTime; }
static uint32_t satCycleGetFlameOnStartMs() { return 0; }
static int g_switches = 0;
static void satSetControlMode(SATControlMode m) { if (m != state.sat.eControlMode) g_switches++; state.sat.eControlMode = m; }
"""

# One argument per tick: dt_ms,flow,request,valves,summer,dhw,mode,pwmOn,offTime ; "dhwbit=1" / "dhwbit=0" is the loop-pass edge.
MAIN = r"""
int main(int argc, char** argv) {
  uint32_t tick = 1000000UL; const uint32_t t0 = tick;
  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "dhwbit=", 7)) { satCycleNoteDhwBit(argv[i][7] == '1', tick); continue; }
    unsigned dt, valves, summer, dhw, mode, pwmOn; float flow, req; unsigned long off;
    if (sscanf(argv[i], "%u,%f,%f,%u,%u,%u,%u,%u,%lu", &dt, &flow, &req, &valves, &summer, &dhw, &mode, &pwmOn, &off) != 9) return 2;
    tick += dt;
    g_flow = flow; g_offTime = (uint32_t)off;
    state.sat.bValvesOpen = valves != 0; state.sat.bSummerActive = summer != 0; state.sat.bDhwActive = dhw != 0;
    state.sat.eControlMode = mode == 2 ? SAT_MODE_PWM : SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = pwmOn != 0;
    if (dhw) { satCycleNoteDhwTick(tick); continue; }     // the control loop's DHW branch returns before the two rules
    const int before = g_switches;
    satCycleCheckOvershootSwitch(req, tick);
    satCycleCheckPwmExits(req, tick);
    if (g_switches != before) { printf("t=%lu mode=%d\n", (unsigned long)(tick - t0), (int)state.sat.eControlMode); return 0; }
  }
  printf("t=-1 mode=%d\n", (int)state.sat.eControlMode);
  return 0;
}
"""


def _fn(src, head):
    m = re.search(r"^" + re.escape(head) + r"\s*\{.*?^\}[ \t]*$", src, re.S | re.M)
    if not m:
        raise AssertionError("%s not found in %s" % (head, CYCLES_INO))
    return m.group(0)


def extract(src):
    consts = re.findall(r"^static const \w+\s+SAT_(?:OVERSHOOT|UNDERSHOOT|UNDERHEAT|SATURATION|DHW)\w*\s*=[^;]*;", src, re.M)
    a = src.index("// --- Sustained State Detection ---")
    b = src.index("// --- Cycle History ---", a)
    return "\n".join(consts) + "\n" + src[a:b] + "\n" + _fn(src, "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)") \
        + "\n" + _fn(src, "bool satCycleCheckPwmExits(float requestedSetpoint, uint32_t tickMs)") + "\n"


def build(src_text, tmp, name):
    body = extract(src_text)
    code = STUBS + body + MAIN
    for need in ("void satCycleNoteDhwTick(uint32_t tickMs)", "void satCycleNoteDhwBit(bool dhwBit, uint32_t nowMs)"):
        if need not in body:   # defined in the cycle section for the real code: keep the cut small, add them from the source
            code = code.replace(MAIN, "")
            code += "\n" + _fn(src_text, need) + "\n" + MAIN
    cpp = Path(tmp) / (name + ".cpp")
    cpp.write_text(code, encoding="utf-8")
    return compile_cpp(cpp, Path(tmp) / (name + ".exe"))


def mutate(src, old, new):
    assert src.count(old) == 1, (old, src.count(old))
    return src.replace(old, new)


GUARDS = {
    "dhw": ("satDhwGuardActive(tickMs) || !satHeatingDemandOn(requestedSetpoint)) {\n    _sustain_overshootRunning = false;",
            "!satHeatingDemandOn(requestedSetpoint)) {\n    _sustain_overshootRunning = false;"),
    "demand_switch": ("satDhwGuardActive(tickMs) || !satHeatingDemandOn(requestedSetpoint)) {\n    _sustain_overshootRunning = false;",
                      "satDhwGuardActive(tickMs)) {\n    _sustain_overshootRunning = false;"),
    "valves": ("  if (!state.sat.bValvesOpen) return false;\n", ""),
    "summer": ("  if (state.sat.bSummerActive && settings.sat.bSummerSimmer) return false;\n", ""),
    "cold": ("  return requestedSetpoint > satColdThreshold();", "  return true;"),
    "demand_exits": ("satDhwGuardActive(tickMs) || !satHeatingDemandOn(requestedSetpoint) ||\n      !state.sat.bPwmFlameRequested) {",
                     "satDhwGuardActive(tickMs) ||\n      !state.sat.bPwmFlameRequested) {"),
}


def over(n, flow=53.0, req=50.0, valves=1, summer=0, dhw=0, dt=10000):
    """n continuous ticks of dt ms, flow = request + 3.0 by default"""
    return ["%d,%.1f,%.1f,%d,%d,%d,1,0,1" % (dt, flow, req, valves, summer, dhw)] * n


def under(n, flow=40.0, req=50.0, valves=1, summer=0, dt=10000):
    """n PWM ON-phase ticks, flow = request - 10"""
    return ["%d,%.1f,%.1f,%d,%d,0,2,1,1" % (dt, flow, req, valves, summer)] * n


class Guards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = {}
        real = build(cls.src, cls.tmp.name, "real")
        cls.exe["real"] = real
        if real:
            for k, (old, new) in GUARDS.items():
                try:
                    cls.exe[k] = build(mutate(cls.src, old, new), cls.tmp.name, "mut_" + k)
                except AssertionError as e:
                    cls.exe[k] = e   # a guard that is not in the source: that case fails below, with the reason

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, which, args):
        exe = self.exe[which]
        if isinstance(exe, AssertionError):
            self.fail("the guard text for mutant %r is not in SATcycles.ino: %s" % (which, exe))
        if not exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(exe)] + args, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout.strip()

    def guarded(self, mutant, args):
        """real source: no switch; mutant without the guard: the unguarded switch happens"""
        mode = [a for a in args if "," in a][-1].split(",")[6]
        self.assertEqual(self.run_exe("real", args), "t=-1 mode=" + mode)
        got = self.run_exe(mutant, args)
        self.assertNotIn("t=-1", got, "the mutant without the guard must switch, so the case proves the guard")

    # --- control: nothing blocks, the rule fires 60 s (180 s) after the first qualifying tick, which is the tick at t=10 s
    def test_control_overshoot_switches_after_60s(self):
        self.assertEqual(self.run_exe("real", over(8)), "t=70000 mode=2")

    def test_control_underheat_switches_after_180s(self):
        self.assertEqual(self.run_exe("real", under(20)), "t=190000 mode=1")

    # --- the switch
    def test_hot_water_now_clears_the_start(self):
        # 3 ticks over, a draw tick, 5 ticks over: without the clear the 60 s run would continue through the draw
        args = over(3) + ["10000,53.0,50.0,1,0,1,1,0,1"] + over(5)
        self.assertEqual(self.run_exe("real", args), "t=-1 mode=1")

    def test_300s_after_the_draw_is_guarded(self):
        args = ["dhwbit=1", "dhwbit=0"] + over(20)             # 200 s of 3.0 C over right after a draw: inside the 300 s
        self.guarded("dhw", args)

    def test_heater_state_valves_closed(self):
        self.guarded("valves", over(10, valves=0))

    def test_heater_state_summer_simmer_pause(self):
        self.guarded("summer", over(10, summer=1))

    def test_request_at_or_below_the_cold_setpoint(self):
        # request 20 is below the cold threshold 28.2: flow 23 is 3.0 C over it
        self.guarded("cold", over(10, flow=23.0, req=20.0))

    def test_demand_guard_is_in_the_switch_function(self):
        self.guarded("demand_switch", over(10, valves=0))

    # --- the exits
    def test_exits_need_the_heater_on_valves(self):
        self.assertEqual(self.run_exe("real", under(25, valves=0)), "t=-1 mode=2")
        self.assertNotIn("t=-1", self.run_exe("demand_exits", under(25, valves=0)))

    def test_exits_need_a_request_above_the_cold_setpoint(self):
        args = under(25, flow=10.0, req=20.0)                  # flow 10 C under a request that is under the cold setpoint
        self.assertEqual(self.run_exe("real", args), "t=-1 mode=2")
        self.assertNotIn("t=-1", self.run_exe("demand_exits", args))

    def test_exits_saturation_obeys_the_guard_too(self):
        # saturation (off time 0 for 300 s) with the valves closed must not end PWM
        args = ["10000,50.0,50.0,0,0,0,2,1,0"] * 35
        self.assertEqual(self.run_exe("real", args), "t=-1 mode=2")
        self.assertNotIn("t=-1", self.run_exe("demand_exits", args))

    def test_missing_flow_clears_the_start(self):
        # 40 s over, one tick without a flow temperature, 40 s over again: no 60 s run
        args = over(4) + ["10000,0.0,50.0,1,0,0,1,0,1"] + over(4)
        self.assertEqual(self.run_exe("real", args), "t=-1 mode=1")


if __name__ == "__main__":
    unittest.main(verbosity=2)
