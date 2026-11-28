#!/usr/bin/env python3
"""TASK-3151.4 (Robert 2026-11-28, ADR-305 decision 8): NO hot-water exception in the PWM duty flow. Entry 499 is replaced.

Entry 499 (Sergeant D, Q-444c A) froze the flow stored for the OFF phase after a hot-water draw inside a PWM ON phase, so a draw-heated flow
did not lengthen the pause. It is gone: the OFF phase uses the flow of the LAST ON-phase tick, whatever a draw did to that flow (entry 444 stays:
the live flow counts only in the ON phase, the OFF phase keeps what the last ON tick saw). The DHW branch of satControlLoopRun() runs
satPwmSchedule() on a draw tick since D4 (cbec269e9), so a draw tick is an ON tick like any other for this value. thermo-nova's duty reads a held
temperature that a late draw cannot move (pwm.py:144-149, :229-239); ours follows the live flow in ON (entries 437, 444), that difference is
the firmware's and is not changed here.

The code under test is the REAL PWM block of SATcontrol.ino (satPwmSchedule, satApplyPWM), compiled by the shared harness
(tests/test_sat_hp_pwm.py), driven here with its own MAIN. Harness limit: the control loop's DHW branch sits outside the cut; the driver does
what it does: on a DHW tick it runs satPwmSchedule() with the last request and sends nothing (command `nodhwsched 1` turns that call off, the
code before D4). The case-by-case numbers are the model's; the rules they pin are in the docstrings of the cases.

Mutants (SATcontrol.ino edited in memory): a hot-water exception in the stored flow (the entry-499 idea) must turn the draw cases red.
OTGW_SAT_INO points at another copy for old-vs-fix runs. Run: python tests/test_sat_pwm_frozen_flow_dhw.py
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
  std::string cmd; int cph = 3; double ratio = 0.22, dur = 0; int noDhwSched = 0; std::vector<Ev> evs;
  while (std::cin >> cmd) {
    if (cmd == "cfg") std::cin >> cph >> ratio;
    else if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
    else if (cmd == "nodhwsched") std::cin >> noDhwSched;   // MUTANT switch: the DHW branch skips the schedule (the code before D4)
  }
  settings.sat.bOvershootProtection = true;
  settings.sat.iCyclesPerHour = (uint8_t)cph;
  float pid = 27.2f + (float)ratio * (50.0f - 27.2f);
  unsigned long long baseMs = 100000000ULL; g_ms = (uint32_t)baseMs; const uint32_t base = g_ms;
  g_flame = true; g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
  state.sat.bPwmFlameRequested = false;
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
      }
    }
    if (sec % 30 != 0) continue;
    if (state.sat.bDhwActive) {                                  // the control loop's DHW branch (D4, cbec269e9): the schedule runs with the last request, nothing is sent
      if (!noDhwSched && state.sat.eControlMode == SAT_MODE_PWM && pid > satColdThreshold()) satPwmSchedule(pid);
      printf("T %d %d %.1f %.1f %.1f %.4f %lu\n", sec, (int)state.sat.bPwmFlameRequested, _sat_lastControlCS, g_flow, _pwm_lastOnFlowTemp,
             state.sat.fPwmDutyCycle, (unsigned long)satPwmLastOffTimeMs());
      continue;
    }
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

    def sim(self, events, dur, ratio=0.22, exe=None, extra=()):
        lines = [f"cfg 3 {ratio}", f"run {dur}", *extra] + [f"ev {s} {k} {v}" for s, k, v in sorted(events, key=lambda e: e[0])]
        out = subprocess.run([str(exe or self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        rows = {}
        for ln in out.splitlines():
            p = ln.split()
            rows[int(p[1])] = dict(on=int(p[2]), cs=float(p[3]), flow=float(p[4]), last=float(p[5]), duty=float(p[6]), off=int(p[7]))
        return rows

    @staticmethod
    def no_dhw(events):
        return [e for e in events if e[1] != "dhw"]

    @staticmethod
    def phase(rows):
        """The rows without the CS: a draw tick prints the last CS (it sends nothing), a heating tick the CS it computes."""
        return {s: {k: v for k, v in r.items() if k != "cs"} for s, r in rows.items()}

    # Gas boiler, 3 cycles/h (period 1200 s), duty 0.22 at flow 50 -> ON 264 s, OFF 936 s. The ON phase starts at tick 0.
    OFF_CLEAN = 936001          # satPwmLastOffTimeMs() with the flow of the heating ticks (50 C)
    OFF_60 = 1016488            # the same with a stored flow of 60 C (duty 0.153)
    DRAW_END_ON = [(195, "flow", 60), (200, "dhw", 1), (250, "dhw", 0)]     # draw over the ticks 210..240, flow draw-heated after it

    def test_without_a_draw_the_stored_flow_is_the_last_on_tick(self):
        rows = self.sim([], 400)
        self.assertEqual(rows[240]["last"], 50.0)
        self.assertEqual(rows[300]["off"], self.OFF_CLEAN)
        self.assertAlmostEqual(rows[300]["duty"], 0.22, places=3)

    def test_a_draw_heated_flow_is_stored_like_any_other_tick(self):
        # Entry 499 is replaced: the draw (flow 60 from 195) is not an exception. The OFF phase keeps the flow of the last ON tick, 60 C, and
        # the pause is the one of duty 0.153. The same run without the DHW bit gives the same rows, tick for tick.
        rows = self.sim(self.DRAW_END_ON, 400)
        self.assertEqual(rows[180]["last"], 50.0)
        self.assertEqual((rows[210]["on"], rows[210]["last"]), (0, 60.0), "the ON phase ends on the draw tick 210 with the heated flow stored")
        self.assertEqual(rows[300]["last"], 60.0)
        self.assertEqual(rows[300]["off"], self.OFF_60)
        self.assertAlmostEqual(rows[300]["duty"], 0.153, places=3)
        self.assertEqual(self.phase(rows), self.phase(self.sim(self.no_dhw(self.DRAW_END_ON), 400)), "the DHW bit must not matter for the stored flow")

    def test_a_draw_that_no_control_tick_sees_gives_the_same_result(self):
        # A draw between two ticks (241..255) leaves no trace in the PWM block, and now needs none: the first tick after it stores the
        # draw-heated flow like any other. (The old limit "a draw no tick sees is not covered" is gone with the exception.)
        ev = [(241, "dhw", 1), (241, "flow", 60), (255, "dhw", 0)]
        rows = self.sim(ev, 400)
        self.assertEqual(rows[270]["last"], 60.0)
        self.assertEqual(rows[300]["off"], self.OFF_60)
        self.assertEqual(self.phase(rows), self.phase(self.sim(self.no_dhw(ev), 400)))

    def test_a_hotter_draw_flow_lengthens_the_pause_more(self):
        # No cap and no freeze: the hotter the stored flow, the longer the pause (duty 0.133 at 65 C).
        rows = self.sim([(195, "flow", 65), (200, "dhw", 1), (250, "dhw", 0)], 400)
        self.assertEqual(rows[300]["last"], 65.0)
        self.assertGreater(rows[300]["off"], self.OFF_60)

    def test_live_flow_drives_the_duty_of_the_on_phase_after_a_draw(self):
        # entries 437/444: an ON tick uses the live flow, and the tick after a draw is stored like every ON tick
        rows = self.sim([(100, "dhw", 1), (110, "flow", 60), (125, "dhw", 0)], 400)
        self.assertEqual(rows[150]["on"], 1)
        self.assertAlmostEqual(rows[150]["duty"], 0.153, places=3)
        self.assertEqual(rows[150]["last"], 60.0)

    def test_the_stored_flow_is_the_last_on_tick_not_a_tick_before_a_draw(self):
        # Entry 499 reading (ours) was "the tick before the draw" (50). Now: the flow is clean again (52 C) before the ON phase ends, so the
        # last ON tick stored 52 and the OFF phase uses it.
        rows = self.sim([(100, "dhw", 1), (110, "flow", 60), (125, "dhw", 0), (150, "flow", 52)], 400)
        self.assertEqual(rows[240]["flow"], 52.0)
        self.assertEqual(rows[240]["last"], 52.0)
        self.assertEqual(rows[300]["last"], 52.0)

    def test_a_draw_before_the_first_on_tick_uses_the_live_flow(self):
        # nothing stored yet: before any ON tick the live flow is used (entry 444); the tick that follows stores it
        rows = self.sim([(5, "flow", 60), (5, "dhw", 1), (20, "dhw", 0)], 400)
        self.assertEqual(rows[270]["on"], 0)
        self.assertAlmostEqual(rows[300]["duty"], 0.153, places=3)

    def test_draw_ends_with_the_flame_out_and_the_cooling_flow_does_not_change_the_stored_value(self):
        # The hot-water flame goes out at the end of the draw and the flow cools (58, 56, 54 C) after the ON phase ended: the OFF phase keeps
        # the flow of the last ON tick (60 C), as every OFF tick does (entry 444). There is no post-draw wait with its own rules any more.
        ev = [(195, "flow", 60), (200, "dhw", 1), (250, "dhw", 0), (250, "flame", 0), (270, "flow", 58), (300, "flow", 56),
              (330, "flow", 54), (340, "flame", 1), (340, "flow", 50)]
        rows = self.sim(ev, 900)
        for sec in (240, 270, 300, 330):
            self.assertEqual(rows[sec]["last"], 60.0, f"tick {sec}: the stored flow stays the one of the last ON tick")
            self.assertEqual(rows[sec]["off"], self.OFF_60, sec)
        self.assertEqual(self.phase(rows), self.phase(self.sim(self.no_dhw(ev), 900)))

    def test_a_draw_before_the_first_on_tick_of_a_later_phase_uses_the_live_flow(self):
        # ON phase 2 begins at the tick 1230; a draw covers 1245..1275 with the flow at 60 C after it. Its ON ticks store the live flow, so the
        # duty of phase 2 is 0.153 from the tick that sees 60 C on.
        ev = [(1245, "dhw", 1), (1245, "flow", 60), (1275, "dhw", 0)]
        rows = self.sim(ev, 1800)
        ticks = [s for s in sorted(rows) if s > 1290 and rows[s]["on"] == 0]
        self.assertTrue(len(ticks) > 1, "phase 2 ended")
        self.assertAlmostEqual(rows[ticks[1]]["duty"], 0.153, places=3, msg="live flow (60 C) for the OFF phase")
        self.assertEqual(self.phase(rows), self.phase(self.sim(self.no_dhw(ev), 1800)))

    def test_the_next_on_phase_stores_again(self):
        # ON phase 2 starts after the OFF phase; its ticks update the stored flow again
        rows = self.sim(self.DRAW_END_ON + [(1200, "flow", 52)], 1600)
        ticks = [s for s in sorted(rows) if s > 1200 and rows[s]["on"] == 1]
        self.assertTrue(ticks, "a second ON phase ran")
        self.assertEqual(rows[ticks[-1]]["last"], 52.0)

    # ---- mutants: each rule must be able to fail ----
    def _mutant_exe(self, name, old, new):
        src = hp.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertEqual(src.count(old), 1, "mutant target must occur exactly once: %r" % old)
        d = Path(self.tmp.name) / name
        d.mkdir(exist_ok=True)
        (d / "SATcontrol.ino").write_text(src.replace(old, new), encoding="utf-8")
        saved = hp.SAT_INO
        hp.SAT_INO = d / "SATcontrol.ino"
        try:
            return hp.build(d)
        finally:
            hp.SAT_INO = saved

    def test_mutant_hot_water_exception_in_the_stored_flow_is_caught(self):
        # the entry-499 idea back in memory: a draw tick does not store its flow
        exe = self._mutant_exe("m_exc", "  if (state.sat.bPwmFlameRequested) {\n    _pwm_lastOnFlowTemp = effectiveTemp;",
                               "  if (state.sat.bPwmFlameRequested && !state.sat.bDhwActive) {   // MUTANT\n    _pwm_lastOnFlowTemp = effectiveTemp;")
        rows = self.sim(self.DRAW_END_ON, 400, exe=exe)
        self.assertNotEqual((rows[210]["on"], rows[210]["last"]), (0, 60.0), "the mutant must not store the draw tick's flow")
        print("\nMUTANT hot-water exception RED: tick 210 on=%d last=%.1f instead of on=0 last=60.0" % (rows[210]["on"], rows[210]["last"]))

    def test_mutant_without_the_schedule_on_draw_ticks_is_caught(self):
        # the code before D4: the DHW branch skips the schedule, so the phase change due in the draw is taken after it
        rows = self.sim(self.DRAW_END_ON, 400, extra=["nodhwsched 1"])
        self.assertNotEqual(self.phase(rows), self.phase(self.sim(self.no_dhw(self.DRAW_END_ON), 400)), "the mutant must differ from the run without the draw")
        print("\nMUTANT no-schedule-in-draw RED: rows differ from the run without the DHW bit")


if __name__ == "__main__":
    unittest.main()
