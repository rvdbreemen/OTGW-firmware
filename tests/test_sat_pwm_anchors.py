#!/usr/bin/env python3
"""TASK-1380 / 1382 / 1383 / 1412: host test for the SAT PWM phase anchors (Sergeant D entries 43, 80, 91, 92).

thermo-nova times every PWM phase from its own switch, not from a flame edge (pwm.py, custom_components/sat):
  OFF  ends when `elapsed >= off_time`, elapsed from _last_update (:134, :155), set only at an ON start (:162),
       at the ignition (:188) and at the ON->OFF switch (:208); no flame-off moves it.
  ON   started without a flame waits for the ignition (:186-188) up to 180 s; the ON time then counts from the
       ignition (_last_update = ignition, :188). After 180 s without a flame the timer starts anyway, from the
       phase start (:197-199). A flame-out and relight inside the phase restarts nothing (:144-146).
  ON   starts only when the mapped on_time is at least 180 s (:155).

Cases (asserts follow thermo-nova, not HEAD). D1 (the second-ON timing), D2, D3, D4, D5 and the two late-ignition
tests D7/D8 fail on HEAD. test_d1_cs_stays_10_through_the_off_wait and D6 are characterization tests: they pass on
HEAD too and only pin behaviour that must not regress.
  D1  OFF wait timed from the last flame-off of ANY kind (hot-water draws move it): gruijter 2026-10-27, entry 92.
  D2  ON phase started with the flame off has no anchor and falls back to the last flame-on (an old hot-water
      ignition), so it ends on its first tick (entry 92, the Q35 path).
  D3  slow ignition: the ON time counts from the ignition.
  D4  no flame at all: no abort at 180 s, the ON time runs from the phase start.
  D5  (Q38) duty range 1 without flame (on_time 0) never starts ON.
  D6  relight inside an ON phase does not restart the ON timer (characterization).
  D7  first flame at +200 s, after the 180 s wait timed out: no resync, but the 180 s flame-on hold applies.
  D8  late ignition at +240 s: the ON time (264 s) expires first, the 180 s flame-on hold keeps the phase ON.

The code under test is NOT re-implemented: the real PWM block of SATcontrol.ino is cut out and compiled exactly as
tests/test_sat_hp_pwm.py does (shared harness tests/test_sat_pwm_override.py), with this file's own MAIN.
Harness limits: the real flame-edge handler satCycleOnFlameChange() (SATcycles.ino:726-875) is NOT cut in (it
drags in the whole cycle tracker); the driver instead performs the two assignments the PWM code reads from it, on
every flame edge: SATcycles.ino:741 `_cycle_flameOnStartMs = now` and :789 `_cycle_flameOffStartMs = now`
(read back through satCycleGetFlameOnStartMs() / satCycleGetFlameOffStartMs(), stubbed to g_flameOnStart /
g_flameOffStart). The control loop is a 30 s tick; while a hot-water draw is active the tick is skipped, as
satControlLoopRun() returns early with DHW active (SATcontrol.ino ~5096-5112). The per-hour start limit is off.

OTGW_SAT_INO / OTGW_SATCYCLES_INO point at other copies for old-vs-fix runs.
Run: python tests/test_sat_pwm_anchors.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_hp_pwm as hp  # noqa: E402  (builds the real PWM cut; its MAIN is replaced below)

TICK = 30  # control loop period in seconds

MAIN = r"""
#include <iostream>
#include <vector>
#include <string>
struct Ev { int sec; std::string kind; int val; };
int main() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  satPwmReset();                                   // the real reset: ON/OFF phase, anchors, ignition wait
  _pwm_effectiveBoilerTemp = 50.0f;                // BT learned in an earlier cycle (= the flow), so the duty is exact
  _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  state.sat.eControlMode = SAT_MODE_PWM; state.sat.bDhwActive = false;
  std::string cmd; int cph = 3; double ratio = 0.22, dur = 0; std::string init = "burning"; std::vector<Ev> evs;
  while (std::cin >> cmd) {
    if (cmd == "cfg") std::cin >> cph >> ratio;
    else if (cmd == "init") std::cin >> init;
    else if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
  }
  settings.sat.iCyclesPerHour = (uint8_t)cph;
  const float pid = 27.2f + (float)ratio * (50.0f - 27.2f);
  g_ms = 100000000UL; const uint32_t base = g_ms;
  if (init == "burning") {                          // flame lit, last burn long ago, OFF phase: the first tick starts ON with the flame
    g_flame = true;  g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
  } else {                                          // "cold": flame off; the last flame-on is an old hot-water ignition (1500 s ago)
    g_flame = false; g_flameOnStart = base - 1500000UL; g_flameOffStart = base - 1400000UL;
  }
  state.sat.bPwmFlameRequested = false;
  for (int sec = 0; sec <= (int)dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
      else if (e.kind == "flame" && (e.val != 0) != g_flame) {   // flame edge: what satCycleOnFlameChange() records
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;
      }
    }
    if (sec % 30 != 0 || state.sat.bDhwActive) continue;         // control tick; skipped during a hot-water draw
    const float cs = satApplyPWM(pid);
    _sat_lastControlCS = cs;
    printf("T %d %d %.1f %d\n", sec, (int)state.sat.bPwmFlameRequested, cs, (int)g_flame);
  }
  return 0;
}
"""


class Timeline:
    def __init__(self, rows):
        self.rows = rows  # (sec, req, cs, flame)

    def req_at(self, sec):
        return [r[1] for r in self.rows if r[0] == sec][0]

    def edges(self):
        prev, ons, offs = 0, [], []
        for sec, req, _cs, _fl in self.rows:
            if req and not prev:
                ons.append(sec)
            if prev and not req:
                offs.append(sec)
            prev = req
        return ons, offs

    def dump(self, upto=None):
        return " ".join(f"{s}:{'ON' if q else 'off'}/{cs:.0f}" for s, q, cs, _ in self.rows if upto is None or s <= upto)


class SatPwmAnchors(unittest.TestCase):
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

    def sim(self, cph, ratio, init, events, dur):
        lines = [f"cfg {cph} {ratio}", f"init {init}", f"run {dur}"]
        lines += [f"ev {s} {k} {v}" for s, k, v in sorted(events)]
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        rows = []
        for ln in out.splitlines():
            p = ln.split()
            rows.append((int(p[1]), int(p[2]), float(p[3]), int(p[4])))
        return Timeline(rows)

    # TASK-1407: PWM edges in the SAT log, with pwm_on / pwm_off in seconds (Sergeant D, entry 93) ------
    def test_edges_are_narrated_with_pwm_on_and_pwm_off_seconds(self):
        # cycles/hour 3 -> period 1200 s, duty 0.22 -> on 264 s, off 936 s: OFF->ON at 0, ON->OFF at 270, OFF->ON again at 1230
        lines = ["cfg 3 0.22", "init burning", "run 1300"]
        r = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        narr = [ln[2:] for ln in r.stderr.splitlines() if ln.startswith("N ")]
        self.assertEqual(narr[:3], ["PWM ON: pwm_on=264 s pwm_off=936 s duty=0.22",
                                    "PWM OFF: pwm_on=264 s pwm_off=936 s duty=0.22",
                                    "PWM ON: pwm_on=264 s pwm_off=936 s duty=0.22"], narr)
        self.assertEqual(len(narr), 3, narr)
        self.assertLess(max(len(x) for x in narr), 96)   # SAT_NARRATE_BUF

    # D1 ------------------------------------------------------------------------------------------------
    def test_d1_off_wait_ignores_hot_water_flame_offs(self):
        # cycles/hour 3 -> period 1200 s, duty 0.22 -> on 264 s, off 936 s. ON phase starts burning at 0, ends on
        # the tick at 270 (t0); the flame goes out 8 s later. Five hot-water draws (flame + DHW) follow, offsets from t0.
        t0 = 270
        ev = [(t0 + 8, "flame", 0)]
        for a, b in ((491, 510), (641, 668), (698, 743), (1225, 1275), (1795, 1804)):
            ev += [(t0 + a, "dhw", 1), (t0 + a + 5, "flame", 1), (t0 + b, "flame", 0), (t0 + b, "dhw", 0)]
        tl = self.sim(3, 0.22, "burning", ev, 4000)
        ons, offs = tl.edges()
        self.assertEqual(ons[0], 0)
        self.assertEqual(offs[0], t0, f"precondition: first ON->OFF at {t0}, got {offs[0]}")
        # thermo-nova pwm.py:134,:155,:208: the next ON is the first tick at elapsed >= 936 s from the ON->OFF switch
        expected = t0 + 960
        self.assertGreater(len(ons), 1, f"no second ON phase in 4000 s (expected t0+960): {tl.dump()}")
        self.assertLessEqual(abs(ons[1] - expected), TICK,
                             f"second ON phase at t0+{ons[1] - t0} s, expected t0+{expected - t0} s (+-{TICK}); "
                             f"HEAD times it from the last hot-water flame-off")

    def test_d1_cs_stays_10_through_the_off_wait(self):
        # Characterization: passes on HEAD too (CS stays 10 in the OFF phase there as well).
        t0 = 270
        ev = [(t0 + 8, "flame", 0), (t0 + 491, "dhw", 1), (t0 + 496, "flame", 1), (t0 + 510, "flame", 0), (t0 + 510, "dhw", 0)]
        tl = self.sim(3, 0.22, "burning", ev, 1500)
        bad = [(s, cs) for s, q, cs, _ in tl.rows if t0 <= s < t0 + 936 and q == 0 and cs != 10.0 and s != t0 + 0]
        # the tick that ends ON (s == t0) already returns the OFF setpoint
        self.assertEqual(bad, [], f"OFF phase ticks with CS != 10: {bad}")

    # D2 / D3 -------------------------------------------------------------------------------------------
    def _ignition_case(self, ignite_at, label):
        tl = self.sim(3, 0.22, "cold", [(ignite_at, "flame", 1)], 900)
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0], f"{label}: ON phase did not start at 0: {tl.dump(120)}")
        # thermo-nova pwm.py:162,:186-188,:201-203: ON waits for the flame, then lasts on_time (264 s) from the ignition
        # (ignition seen at the first tick >= ignite_at)
        seen = ((ignite_at + TICK - 1) // TICK) * TICK
        exp_end = seen + 264
        self.assertTrue(offs, f"{label}: ON phase never ended")
        for s, q, cs, _ in tl.rows:
            if s < ignite_at + 180:
                self.assertEqual(q, 1, f"{label}: ON phase ended early at t={s}s (ignition at {ignite_at}s); {tl.dump(240)}")
                self.assertNotEqual(cs, 10.0, f"{label}: CS=10 inside the ON phase at t={s}s")
        self.assertLessEqual(abs(offs[0] - exp_end), TICK,
                             f"{label}: ON ended at t={offs[0]} s, expected about {exp_end} s (ignition + 264 s)")

    def test_d2_on_phase_without_flame_does_not_end_on_its_first_tick(self):
        self._ignition_case(60, "D2 ignition at +60")

    def test_d3_slow_ignition_at_120_s_times_on_from_the_ignition(self):
        self._ignition_case(120, "D3 ignition at +120")

    # D4 ------------------------------------------------------------------------------------------------
    def test_d4_no_flame_within_180_s_does_not_abort_on(self):
        # duty 0.5 -> on 600 s. pwm.py:197-199: no abort, "starts the timer anyway" from the phase start; ON ends at 600.
        tl = self.sim(3, 0.5, "cold", [], 900)
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0])
        self.assertTrue(offs, "ON phase never ended")
        self.assertLessEqual(abs(offs[0] - 600), TICK,
                             f"ON ended at t={offs[0]} s with no flame, expected 600 s (on_time from the phase start); "
                             f"{tl.dump(300)}")

    # D5 (Q38, TASK-1383) --------------------------------------------------------------------------------
    def test_d5_on_time_below_180_s_never_starts_on(self):
        # cph 3: duty 0.05 < dutyMin 0.075 -> range 1, no flame: on_time 0. pwm.py:155: on_time >= 180 or no start.
        tl = self.sim(3, 0.05, "cold", [], 6000)
        ons, _ = tl.edges()
        self.assertEqual(ons, [], f"ON phase started with on_time 0 at t={ons[:3]} s")

    # D6 ------------------------------------------------------------------------------------------------
    def test_d6_relight_inside_on_does_not_restart_the_timer(self):
        # Characterization: passes on HEAD too (the relight does not move the ON timer there either).
        # burning start at 0, duty 0.5 -> on 600 s; flame out at 100, relight at 250. pwm.py:144-146: no restart; ON ends at 600.
        tl = self.sim(3, 0.5, "burning", [(100, "flame", 0), (250, "flame", 1)], 1200)
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0])
        self.assertTrue(offs, "ON phase never ended")
        self.assertLessEqual(abs(offs[0] - 600), TICK,
                             f"ON ended at t={offs[0]} s, expected 600 s from the phase start; {tl.dump(700)}")

    # D7 / D8 -------------------------------------------------------------------------------------------
    def _late_ignition(self, ignite_at, label):
        # duty 0.22 -> on 264 s. No flame for the first 180 s: the wait times out and the ON timer runs anyway
        # (pwm.py:197-199). The first flame then arrives at ignite_at; it anchors the 180 s hold (pwm.py:144-146,
        # :201-203) but does not resync the phase (pwm.py:188 only fires while waiting).
        tl = self.sim(3, 0.22, "cold", [(ignite_at, "flame", 1)], 900)
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0], f"{label}: ON phase did not start at 0")
        self.assertTrue(offs, f"{label}: ON phase never ended: {tl.dump(600)}")
        seen = ((ignite_at + TICK - 1) // TICK) * TICK
        hold_end = seen + 180
        self.assertGreater(hold_end, 264 + TICK, "precondition: the hold, not the ON time, must end the phase")
        self.assertLess(hold_end, seen + 264 - TICK, "precondition: ending at the hold means no resync")
        for s, q, cs, _ in tl.rows:
            if s < hold_end:
                self.assertEqual(q, 1, f"{label}: ON ended early at t={s}s (hold until {hold_end}s); {tl.dump(600)}")
                self.assertNotEqual(cs, 10.0, f"{label}: CS=10 inside the ON phase at t={s}s")
        self.assertLessEqual(abs(offs[0] - hold_end), TICK,
                             f"{label}: ON ended at t={offs[0]} s, expected {hold_end} s (flame-on + 180 s hold)")

    def test_d7_first_flame_after_the_wait_timed_out_does_not_resync_but_holds_180_s(self):
        self._late_ignition(200, "D7 first flame at +200")

    def test_d8_late_ignition_keeps_on_until_180_s_after_flame_on(self):
        self._late_ignition(240, "D8 first flame at +240")


if __name__ == "__main__":
    unittest.main(verbosity=2)
