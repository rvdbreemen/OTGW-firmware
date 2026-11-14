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
struct SatSettings { float fFlowOffset = 2.0f; float fModSupOffset = 1.0f; };
struct Settings { SatSettings sat; };
static Settings settings;
static bool g_flame = true; static float g_flow = 30.0f; static uint32_t g_ms = 0;
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
static uint32_t millis() { return g_ms; }
static uint32_t _sat_chFlameOnMs = 0;
@CONST@
@SPAN@
int main(int argc, char** argv) {
  const uint32_t start = (uint32_t)strtoul(argv[1], 0, 10);   // millis() at the CH flame-on edge
  const int mode = atoi(argv[2]);
  const float req = 50.0f;
  float prev = 0.0f;                                           // the first computation after an ignition
  _sat_chFlameOnMs = (mode == 1) ? 0 : (start ? start : 1);    // mode 1: no edge seen (window never opens)
  for (uint32_t t = 0; t <= 120; t += 5) {                     // the 5 s recompute
    g_ms = start + t * 1000UL;
    g_flow = 30.0f + 0.5f * (float)t; if (g_flow > 62.0f) g_flow = 62.0f;   // a boiler warming up, then steady
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
    call = "satContinuousSetpoint(req, prev)" if new else "satApplyContinuous(req, prev)"
    return STUBS.replace("@CONST@", const).replace("@SPAN@", span).replace("@CALL@", call)


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

    # --- source: where the window starts and where the CS goes out (the control loop cannot be compiled on the host) ---
    def test_window_starts_on_a_ch_flame_edge_only_and_closes_when_the_flame_goes_off(self):
        s = self.src
        self.assertIn("static const uint32_t SAT_CONT_STARTUP_MS = 60000UL;", s)   # entry 344
        self.assertIn("if (currentFlame && !dhwBitNow) { const uint32_t n = millis(); _sat_chFlameOnMs = n ? n : 1; }", s)
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
