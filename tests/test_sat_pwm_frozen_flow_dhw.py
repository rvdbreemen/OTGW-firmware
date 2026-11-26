#!/usr/bin/env python3
"""Entry 499 (Sergeant D, Q-444c A, release 2.0.0; corrects entry 494): after a hot-water draw inside a PWM ON phase the flow stored for
the OFF phase is no longer updated for the rest of that ON phase; the OFF phase freezes the flow of the last tick before the draw
(entries 444, 446). A draw before the first ON tick leaves no value: the live flow is used as before.

The code under test is the REAL PWM block of SATcontrol.ino (satPwmSchedule, satApplyPWM, satPwmOnDhwStart/End,
satPwmNoteDhwFlame, satPwmNoteDhwFlameEdge), compiled by the shared harness (tests/test_sat_hp_pwm.py), driven here with its own
MAIN as tests/test_sat_pwm_on_freeze.py does. Harness limit: the control loop's early return on a tick with the DHW bit set
(satControlLoopRun(), SATcontrol.ino ~8241, "The branch above returns before satPwmSchedule()") sits outside the cut; the driver
does what it does: on a DHW tick it calls satPwmOnDhwStart() on the first one and satPwmNoteDhwFlame(), and skips the schedule;
on the first tick without the bit it calls satPwmOnDhwEnd() before the schedule.

OTGW_SAT_INO points at another copy for old-vs-fix runs.
Run: python tests/test_sat_pwm_frozen_flow_dhw.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_hp_pwm as hp  # noqa: E402

TICK = 30
MAIN = r"""
#include <iostream>
#include <vector>
#include <string>
struct Ev { int sec; std::string kind; double val; };
int main() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  satPwmReset();
  _pwm_effectiveBoilerTemp = 50.0f;
  _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  state.sat.eControlMode = SAT_MODE_PWM; state.sat.bDhwActive = false;
  std::string cmd; int cph = 3; double ratio = 0.22, dur = 0; std::vector<Ev> evs;
  while (std::cin >> cmd) {
    if (cmd == "cfg") std::cin >> cph >> ratio;
    else if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
  }
  settings.sat.bOvershootProtection = true;
  settings.sat.iCyclesPerHour = (uint8_t)cph;
  float pid = 27.2f + (float)ratio * (50.0f - 27.2f);
  unsigned long long baseMs = 100000000ULL; g_ms = (uint32_t)baseMs; const uint32_t base = g_ms;
  g_flame = true; g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
  state.sat.bPwmFlameRequested = false; bool prevDhw = false;
  _pwm_entryPending = true; _pwm_lastUpdateMs = base;
  for (int sec = 0; sec <= (int)dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
      else if (e.kind == "flow") g_flow = (float)e.val;
      else if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;
        satPwmNoteDhwFlameEdge(g_flame, state.sat.bDhwActive);   // the flame-edge handler of satControlLoopRun(), every loop pass
      }
    }
    if (sec % 30 != 0) continue;
    if (state.sat.bDhwActive) {                                  // the control loop's DHW branch: no schedule on this tick
      if (!prevDhw) satPwmOnDhwStart();
      satPwmNoteDhwFlame(g_flame);
      prevDhw = true; continue;
    }
    if (prevDhw) satPwmOnDhwEnd(g_flame);
    prevDhw = false;
    const float cs = satApplyPWM(pid);
    _sat_lastControlCS = cs;
    printf("T %d %d %.1f %.1f %.1f %.4f %lu\n", sec, (int)state.sat.bPwmFlameRequested, cs, g_flow, _pwm_lastOnFlowTemp,
           state.sat.fPwmDutyCycle, (unsigned long)satPwmLastOffTimeMs());
  }
  return 0;
}
"""


class FrozenFlow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        hp.MAIN = MAIN
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = hp.build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def sim(self, events, dur, ratio=0.22):
        lines = [f"cfg 3 {ratio}", f"run {dur}"] + [f"ev {s} {k} {v}" for s, k, v in sorted(events, key=lambda e: e[0])]
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        rows = {}
        for ln in out.splitlines():
            p = ln.split()
            rows[int(p[1])] = dict(on=int(p[2]), cs=float(p[3]), flow=float(p[4]), last=float(p[5]), duty=float(p[6]), off=int(p[7]))
        return rows

    # Gas boiler, 3 cycles/h (period 1200 s), duty 0.22 at flow 50 -> ON 264 s, OFF 936 s. The ON phase starts at tick 0.
    OFF_CLEAN = 936001          # satPwmLastOffTimeMs() with the flow of the heating ticks (50 C)
    DRAW_END_ON = [(195, "flow", 60), (200, "dhw", 1), (250, "dhw", 0)]     # draw over the ticks 210..240, flow draw-heated after it

    def test_without_a_draw_the_frozen_flow_is_the_last_on_tick(self):
        rows = self.sim([], 400)
        self.assertEqual(rows[240]["last"], 50.0)
        self.assertEqual(rows[300]["off"], self.OFF_CLEAN)
        self.assertAlmostEqual(rows[300]["duty"], 0.22, places=3)

    def test_draw_spanning_ticks_at_the_end_of_on_does_not_freeze_the_draw_heated_flow(self):
        # 494: the first tick after the draw (270, DHW bit off, flow 60) is a heating tick by the bit; its flow must not be frozen.
        rows = self.sim(self.DRAW_END_ON, 400)
        self.assertEqual(rows[180]["last"], 50.0)
        self.assertEqual(rows[270]["on"], 0, "the ON phase ended")
        self.assertEqual(rows[270]["last"], 50.0, "frozen: the flow of the last tick before the draw")
        self.assertAlmostEqual(rows[300]["duty"], 0.22, places=3)
        self.assertEqual(rows[300]["off"], self.OFF_CLEAN, "no longer pause (old code: 1016488 = +80 s)")

    def test_known_limit_a_draw_that_no_control_tick_sees_is_not_covered(self):
        # The PWM block learns of a draw only from a control tick with the DHW bit set (satPwmOnDhwStart) or a flame edge with the bit
        # set (satPwmNoteDhwFlameEdge). A draw that starts and ends between two ticks with the flame staying on leaves no trace there,
        # so the first tick after it stores the draw-heated flow. Pinned so a change of that is noticed; the same holds for any rule
        # that needs to know a draw happened.
        rows = self.sim([(241, "dhw", 1), (241, "flow", 60), (255, "dhw", 0)], 400)
        self.assertEqual(rows[270]["last"], 60.0)
        self.assertEqual(rows[300]["off"], 1016488)

    def test_a_hotter_draw_flow_does_not_lengthen_the_pause_more(self):
        rows = self.sim([(195, "flow", 65), (200, "dhw", 1), (250, "dhw", 0)], 400)
        self.assertEqual(rows[300]["off"], self.OFF_CLEAN)

    def test_live_flow_still_drives_the_duty_of_the_on_phase_after_a_draw(self):
        # entry 437/444: only the stored value for the OFF phase is frozen; an ON tick uses the live flow
        rows = self.sim([(100, "dhw", 1), (110, "flow", 60), (125, "dhw", 0)], 400)
        self.assertEqual(rows[150]["on"], 1)
        self.assertAlmostEqual(rows[150]["duty"], 0.153, places=3)
        self.assertEqual(rows[150]["last"], 50.0, "the draw-heated tick is not stored")

    def test_last_tick_before_the_draw_differs_from_the_last_clean_tick(self):
        # Entry 499 reading (ours): "last heating tick" = the tick before the draw. If the flow is clean again before the ON phase ends
        # (52 C after an early draw), a "last clean tick" rule would freeze 52; entry 499 freezes 50 (the tick before the draw).
        rows = self.sim([(100, "dhw", 1), (110, "flow", 60), (125, "dhw", 0), (150, "flow", 52)], 400)
        self.assertEqual(rows[240]["flow"], 52.0)
        self.assertEqual(rows[240]["last"], 50.0)

    def test_a_draw_before_the_first_on_tick_leaves_no_value_and_the_live_flow_is_used(self):
        # nothing to freeze yet: today's fallback (before any ON tick the live flow is used, entry 444) stays
        rows = self.sim([(5, "flow", 60), (5, "dhw", 1), (20, "dhw", 0)], 400)
        self.assertEqual(rows[270]["on"], 0)
        self.assertAlmostEqual(rows[300]["duty"], 0.153, places=3)

    def test_draw_ends_with_the_flame_out_and_the_draw_heated_flow_cools_in_the_wait(self):
        # Review F1: the hot-water flame goes out at the end of the draw. The post-draw wait (entry 103 A, up to 180 s) runs ticks through
        # the ON branch before the resume; they carry the cooling, draw-heated flow and must not be stored either.
        ev = [(195, "flow", 60), (200, "dhw", 1), (250, "dhw", 0), (250, "flame", 0), (270, "flow", 58), (300, "flow", 56),
              (330, "flow", 54), (340, "flame", 1), (340, "flow", 50)]
        rows = self.sim(ev, 900)
        for sec in (270, 300, 330):
            self.assertEqual(rows[sec]["last"], 50.0, f"wait tick {sec}: the stored flow stays the one before the draw")
        off = [s for s in sorted(rows) if s > 340 and rows[s]["on"] == 0]
        self.assertTrue(off, "the ON phase ended")
        self.assertEqual(rows[off[0]]["off"], self.OFF_CLEAN)

    def test_a_draw_before_the_first_on_tick_of_a_later_phase_uses_the_live_flow(self):
        # Review F2 (entries 444, 499): ON phase 2 has no tick before its draw, so there is no earlier value; the value of phase 1 is not
        # carried over. Phase 1 stores 50 and ends at the tick 270; phase 2 begins at the tick 1230, its first storing tick would be 1260, and
        # a draw covers exactly that tick (1245-1275, flow 60 after it).
        ev = [(1245, "dhw", 1), (1245, "flow", 60), (1275, "dhw", 0)]
        rows = self.sim(ev, 1800)
        ticks = [s for s in sorted(rows) if s > 1290 and rows[s]["on"] == 0]
        self.assertTrue(len(ticks) > 1, "phase 2 ended")
        self.assertAlmostEqual(rows[ticks[1]]["duty"], 0.153, places=3, msg="live flow (60 C) for the OFF phase, not phase 1's 50 C")

    def test_the_next_on_phase_stores_again(self):
        # ON phase 2 starts after the OFF phase of 936 s; its heating ticks update the stored flow again
        rows = self.sim(self.DRAW_END_ON + [(1200, "flow", 52)], 1600)
        ticks = [s for s in sorted(rows) if s > 1200 and rows[s]["on"] == 1]
        self.assertTrue(ticks, "a second ON phase ran")
        self.assertEqual(rows[ticks[-1]]["last"], 52.0)

if __name__ == "__main__":
    unittest.main()
