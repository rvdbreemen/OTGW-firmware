#!/usr/bin/env python3
"""TASK-1924 Part 2: the continuous CS follows a rising flow for 60 s after a CH ignition.

Sergeant D entries 336 (the CS must not let a rising flow pass it at start-up), 337 (5 s recompute), Robert entry 338,
Sergeant D entries 344 (60 s, the cycle tracker's warm-up) and 347 (the suppression offset fModSupOffset, 1.0 C; corrects
entry 346, which had the clamp's 2.0 C). The clamp after the window keeps fFlowOffset (2.0 C).

Nothing is re-implemented: the real satApplyContinuous() and satContinuousSetpoint() are cut out of SATcontrol.ino and
compiled with a host C++ compiler, with the real SAT_CONT_STARTUP_MS. The harness runs a 5 s recompute (as
satContinuousClampRefresh() does) with the previous CS fed back, at a constant request and a rising flow.
On code without satContinuousSetpoint() (old-vs-fix run, OTGW_SAT_INO = `git show HEAD:...`) the harness calls
satApplyContinuous(), which is what the tick did before.

Run: python tests/test_sat_continuous_startup.py
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
#include <cstdlib>
#define SAT_MIN_SETPOINT 10.0f
struct SatSettings { float fFlowOffset = 2.0f; float fModSupOffset = 1.0f; float fFlameOffOffset = 18.0f; float fModSupDelay = 20.0f;
                     bool bOvershootProtection = true; };
struct Settings { SatSettings sat; };
static Settings settings;
struct SatState { bool bDhwActive = false; }; struct State { SatState sat; }; static State state;
static bool g_flame = true; static float g_flow = 30.0f, g_ret = 0.0f; static bool g_noret = false; static uint32_t g_ms = 0;
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
static float satGetReturnTemp() { return g_ret; }
static bool satNoReturnTemp() { return g_noret; }
static float satGetColdSetpoint() { return 28.2f; }
static float satColdThreshold() { return 28.2f; }
static float satGetMaxSetpoint() { return 75.0f; }
static uint32_t millis() { return g_ms; }
static uint32_t _sat_chFlameOnMs = 0;
static float _sat_contFlameOffHold = 0.0f;
@CONST@
@HOLD@
@SPAN@
int main(int argc, char** argv) {
  const uint32_t start = (uint32_t)strtoul(argv[1], 0, 10);   // millis() at the CH flame-on edge
  const int mode = atoi(argv[2]);
  const float req = 50.0f;
  float prev = 0.0f;                                           // the first computation after an ignition
  // modes 0-2: flame on from t = 0. Modes 3-6 (TASK-1942): flame off until t = 30 s, CH flame-on edge at t = 30 s;
  // 3 return 30 known, 4 a boiler without a return temperature, 5 overshoot protection off, 6 hot water active.
  const bool late = mode >= 3;
  if (mode == 4) g_noret = true;
  if (mode == 3 || mode == 5 || mode == 6) g_ret = 30.0f;
  if (mode == 5) settings.sat.bOvershootProtection = false;
  if (mode == 6) state.sat.bDhwActive = true;
  _sat_chFlameOnMs = (mode == 1 || late) ? 0 : (start ? start : 1);   // mode 1: no edge seen (window never opens)
  for (uint32_t t = 0; t <= 120; t += 5) {                     // the 5 s recompute
    g_ms = start + t * 1000UL;
    const uint32_t on = late ? (t >= 30 ? t - 30 : 0) : t;
    g_flow = 30.0f + 0.5f * (float)on; if (g_flow > 62.0f) g_flow = 62.0f;   // a boiler warming up, then steady
    if (late) { g_flame = t >= 30; if (t == 30) _sat_chFlameOnMs = g_ms ? g_ms : 1; }
    if (mode == 2 && t >= 40) g_flame = false;                 // the flame goes off inside the window
    prev = @CALL@;
    printf("%lu:%.1f ", (unsigned long)t, prev);
  }
  printf("\n");
  return 0;
}
"""


def build_cpp(src: str) -> str:
    a = src.index("static float satApplyContinuous(")
    b = src.index("//=== Get Room Temperature", a)
    refresh = src.find("\n// TASK-1924 Part 2 (Sergeant D entry 337): while the flame", a, b)
    if refresh != -1:
        b = refresh + 1
    else:
        b = src.rindex("//=====", a, b)
    span = src[a:b]
    new = "static float satContinuousSetpoint(" in span
    m = re.search(r"static const uint32_t SAT_CONT_STARTUP_MS\s*=\s*[^;]+;", src)
    const = m.group(0) if m else ""
    h = src.index("static float satPwmFlameOffHold(float maxSetpoint)")   # TASK-1942: the real PWM flame-off hold
    hold = src[h:src.index("\n}\n", h) + 3]
    call = "satContinuousSetpoint(req, prev)" if new else "satApplyContinuous(req, prev)"
    return STUBS.replace("@CONST@", const).replace("@HOLD@", hold).replace("@SPAN@", span).replace("@CALL@", call)


class ContinuousStartup(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(cls.src), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_mode(self, mode, start=100000):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(start), str(mode)], capture_output=True, text=True, check=True).stdout
        return {int(k): float(v) for k, v in (p.split(":") for p in out.split())}

    def test_cs_follows_the_rising_flow_inside_the_60s_window(self):
        # flow 30 + 0.5 C/s; request 50, start-up offset 1.0 (entry 347). In the window: max(50, flow - 1). At t=40 flow 50 -> 50;
        # at t=55 flow 57.5 -> 56.5. Old code (the entry-112 clamp with the previous CS): the CS stays 50 (entry 336).
        cs = self.run_mode(0)
        self.assertEqual(cs[0], 50.0)
        self.assertEqual(cs[40], 50.0)
        self.assertAlmostEqual(cs[55], 56.5, places=1)
        self.assertGreater(cs[55], cs[45])

    def test_after_the_window_the_clamp_holds_and_does_not_drop_to_the_request(self):
        # From 60 s the entry-112 clamp: request 50 <= previous, so max(50, min(flow - 2, previous)); the CS keeps the
        # value it reached (t=55: 56.5; at t=60 the window has closed: max(50, min(60 - 2.0, 56.5)) = 56.5, then held).
        cs = self.run_mode(0)
        self.assertAlmostEqual(cs[55], 56.5, places=1)
        self.assertGreaterEqual(cs[65], cs[55])
        self.assertEqual(cs[120], cs[65], "outside the window the CS does not follow the rising flow any further")

    def test_no_window_without_a_ch_flame_edge(self):
        # _sat_chFlameOnMs == 0 (flame off, a hot-water flame, or boot with the flame already on): the clamp as before
        cs = self.run_mode(1)
        self.assertEqual(cs[55], 50.0)

    def test_flame_off_in_the_window_gives_the_request(self):
        cs = self.run_mode(2)
        self.assertEqual(cs[40], 50.0)
        self.assertEqual(cs[60], 50.0)

    def test_window_works_across_the_millis_wrap(self):
        cs = self.run_mode(0, start=4294967295 - 20000)
        self.assertAlmostEqual(cs[55], 56.5, places=1)
        self.assertEqual(cs[120], cs[65])

    # --- TASK-1942 (entry 348): the PWM start sequence in continuous mode ---
    def test_start_runs_return_plus_18_then_the_hold_then_the_follow_then_the_clamp(self):
        # flame off to t=30 s (return 30): 30 + 18 = 48; old code (beta.250): the request (50). Flame-on edge at 30 s: the 48 is
        # held for fModSupDelay (20 s: t=30..45); then max(request 50, flow - 1.0): t=80 flow 55 -> 54, t=85 -> 56.5; from 60 s
        # after the flame-on (t=90) the entry-112 clamp holds it.
        cs = self.run_mode(3)
        self.assertEqual(cs[0], 48.0)
        self.assertEqual(cs[25], 48.0)
        self.assertEqual(cs[30], 48.0)
        self.assertEqual(cs[45], 48.0)
        self.assertEqual(cs[50], 50.0)
        self.assertAlmostEqual(cs[85], 56.5, places=1)
        self.assertAlmostEqual(cs[90], 56.5, places=1)
        self.assertEqual(cs[120], cs[90])

    def test_no_return_boiler_uses_the_flow_and_the_hold_follows_it(self):
        # entry 45 / satPwmFlameOffHold: flow + 18; in the hold the live flow + 18 (t=40: flow 35 -> 53)
        cs = self.run_mode(4)
        self.assertEqual(cs[0], 48.0)
        self.assertEqual(cs[40], 53.0)

    def test_overshoot_protection_off_and_hot_water_keep_the_request(self):
        for mode in (5, 6):
            cs = self.run_mode(mode)
            self.assertEqual(cs[0], 50.0, mode)

    def test_the_flood_runs_in_continuous_mode_and_hands_over_to_the_follow(self):
        s = self.src
        allowed = s[s.index("static bool satNoReturnFloodAllowed()"):]
        allowed = allowed[:allowed.index("\n}\n")]
        self.assertIn("state.sat.eControlMode == SAT_MODE_CONTINUOUS", allowed)
        step = s[s.index("static void satNoReturnFloodStep()"):]
        step = step[:step.index("\n}\n")]
        self.assertIn("satContinuousSetpoint(_sat_requestedSetpoint, _sat_lastControlCS)", step)
        refresh = s[s.index("static void satContinuousClampRefresh()"):]
        self.assertIn("if (_flood_active) return;", refresh[:refresh.index("\n}\n")])

    # --- source: where the window starts and where the CS goes out (the control loop cannot be compiled on the host) ---
    def test_window_starts_on_a_ch_flame_edge_only_and_closes_when_the_flame_goes_off(self):
        s = self.src
        self.assertIn("static const uint32_t SAT_CONT_STARTUP_MS = 60000UL;", s)   # entry 344
        # ADR-305 (TASK-3151.2): the block that also anchored the start gate (TASK-1942) is back to the anchor statements only:
        # same edge (CH flame on after a seen flame off), same anchor, nothing else in the block
        self.assertRegex(s, r"if \(currentFlame && !dhwBitNow && _sat_flameOffSeen\) \{(?:[ \t]*//[^\n]*)?\s*"
                            r"const uint32_t n = millis\(\);\s*_sat_chFlameOnMs = n \? n : 1;\s*\}")
        self.assertNotIn("_sat_startGate", s)
        # TASK-1942: a flame already burning when SAT is switched on is no CH start (no window, no continuous flood)
        self.assertIn("if (!currentFlame) _sat_flameOffSeen = true;", s)
        self.assertIn("_sat_flameOffSeen = false;             // TASK-1942", s)
        self.assertIn("if (state.sat.eControlMode == SAT_MODE_CONTINUOUS && !_sat_flameOffSeen) return;", s)
        self.assertIn("else if (!currentFlame)         _sat_chFlameOnMs = 0;", s)

    def test_tick_and_5s_refresh_both_use_the_continuous_setpoint(self):
        s = self.src
        self.assertIn("finalSetpoint = satContinuousSetpoint(pidOutput, _sat_lastControlCS);", s)
        refresh = s[s.index("static void satContinuousClampRefresh()"):]
        refresh = refresh[:refresh.index("\n}\n")]
        self.assertIn("DUE(timerSATContClamp)", refresh)
        self.assertIn("state.sat.eControlMode != SAT_MODE_CONTINUOUS", refresh)
        self.assertIn("float cs = satContinuousSetpoint(_sat_requestedSetpoint, _sat_lastControlCS);", refresh)
        i_cap = refresh.index("if (cs > maxSetpoint) cs = maxSetpoint;")
        self.assertLess(i_cap, refresh.index("satSendCS(cs);"))
        self.assertIn("DECLARE_TIMER_SEC(timerSATContClamp, 5, SKIP_MISSED_TICKS);", s)
        loop = s[s.index("static void satControlLoopRun()"):]
        self.assertLess(loop.index("satPwmSuppressionRefresh();"), loop.index("satContinuousClampRefresh();"))


if __name__ == "__main__":
    unittest.main()
