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

NL = chr(10)
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
  int ovs = 1, entryArm = 0; unsigned long long baseMs = 100000000ULL;
  while (std::cin >> cmd) {
    if (cmd == "cfg") std::cin >> cph >> ratio;
    else if (cmd == "init") std::cin >> init;
    else if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
    else if (cmd == "ovs") std::cin >> ovs;                 // overshoot protection on/off
    else if (cmd == "entry") std::cin >> entryArm;          // PWM just entered: the first step skips the off wait (entry 43)
    else if (cmd == "src") std::cin >> g_source;           // heating source (2 = heat pump: minimum burn 600 s, entry 197)
    else if (cmd == "base") std::cin >> baseMs;            // millis() at second 0 (LL-019 rollover: a base just below 2^32 wraps inside the run)
  }
  settings.sat.bOvershootProtection = ovs != 0;
  settings.sat.iCyclesPerHour = (uint8_t)cph;
  float pid = 27.2f + (float)ratio * (50.0f - 27.2f);
  g_ms = (uint32_t)baseMs; const uint32_t base = g_ms;
  if (init == "burning") {                          // flame lit, last burn long ago, OFF phase: the first tick starts ON with the flame
    g_flame = true;  g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
  } else {                                          // "cold": flame off; the last flame-on is an old hot-water ignition (1500 s ago)
    g_flame = false; g_flameOnStart = base - 1500000UL; g_flameOffStart = base - 1400000UL;
  }
  state.sat.bPwmFlameRequested = false; bool prevDhw = false;
  if (entryArm) { _pwm_entryPending = true; _pwm_lastUpdateMs = base; }   // satSetControlMode() arms this; the off wait runs from the entry
  for (int sec = 0; sec <= (int)dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
      else if (e.kind == "flow") g_flow = (float)e.val;
      else if (e.kind == "ratio") pid = 27.2f + (float)e.val / 100.0f * (50.0f - 27.2f);
      else if (e.kind == "ovs") settings.sat.bOvershootProtection = e.val != 0;   // LL-017: protection toggled mid-run
      else if (e.kind == "flame" && (e.val != 0) != g_flame) {   // flame edge: what satCycleOnFlameChange() records
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;
        @FLAMEEDGE@                                              // satControlLoopRun(): the every-loop flame-edge path, with the live DHW bit
      }
    }
    if (sec % 30 != 0) continue;                                 // control tick
    if (state.sat.bDhwActive) {                                  // hot water: the loop returns early, the schedule is skipped
      if (!prevDhw) { @DHWSTART@ }                               // satControlLoopRun(): rising edge, before the early return
      @DHWFLAME@                                                 // satControlLoopRun(): a flame burning with the DHW bit set is a hot-water flame
      prevDhw = true; continue;
    }
    if (prevDhw) { @DHWEND@ }                                    // falling edge: before satApplyPWM(), as in satControlLoopRun()
    prevDhw = false;
    const float cs = satApplyPWM(pid);
    _sat_lastControlCS = cs;
    printf("T %d %d %.1f %d %.1f\n", sec, (int)state.sat.bPwmFlameRequested, cs, (int)g_flame, _pwm_effectiveBoilerTemp);
    // the CH bit the control tick sends for this CS (satControlLoopRun: satChEnableDecide with satPwmDrivesSetpoint)
    fprintf(stderr, "H %d %d\n", sec, (int)satChEnableDecide(pid, satColdThreshold(), satPwmDrivesSetpoint(pid), state.sat.bPwmFlameRequested));
    fprintf(stderr, "M %d %d\n", sec, @ATMAX@);   // TASK-1581: duty range 5 (SAT sends MM=T); 0 on a source without the flag
  }
  fprintf(stderr, "C %d\n", g_pwmStarts);          // satCycleRecordPwmStart() calls = PWM starts counted (TASK-1421)
  return 0;
}
"""


class Timeline:
    def __init__(self, rows):
        self.rows = rows  # (sec, req, cs, flame, effective BT)

    def req_at(self, sec):
        return [r[1] for r in self.rows if r[0] == sec][0]

    def edges(self):
        prev, ons, offs = 0, [], []
        for sec, req, _cs, _fl, _bt in self.rows:
            if req and not prev:
                ons.append(sec)
            if prev and not req:
                offs.append(sec)
            prev = req
        return ons, offs

    def dump(self, upto=None):
        return " ".join(f"{s}:{'ON' if q else 'off'}/{cs:.0f}" for s, q, cs, _, _bt in self.rows if upto is None or s <= upto)


class SatPwmAnchors(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        hooks = "satPwmOnDhwStart" in hp.SAT_INO.read_text(encoding="utf-8")
        hp.MAIN = (MAIN.replace("@ATMAX@", "(int)satPwmAtMaxDuty()" if "satPwmAtMaxDuty" in hp.SAT_INO.read_text(encoding="utf-8") else "0")
                       .replace("@DHWSTART@", "satPwmOnDhwStart();" if hooks else "")
                       .replace("@DHWEND@", "satPwmOnDhwEnd(g_flame);" if hooks else "")
                       .replace("@FLAMEEDGE@", "satPwmNoteDhwFlameEdge(g_flame, state.sat.bDhwActive);" if "satPwmNoteDhwFlameEdge" in hp.SAT_INO.read_text(encoding="utf-8") else "")
                       .replace("@DHWFLAME@", "satPwmNoteDhwFlame(g_flame);" if "satPwmNoteDhwFlame" in hp.SAT_INO.read_text(encoding="utf-8") else ""))
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = hp.build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def starts(self, cph, ratio, init, events, dur, extra=()):
        """Number of satCycleRecordPwmStart() calls (PWM starts counted) and the timeline of the same run."""
        lines = [f"cfg {cph} {ratio}", f"init {init}", f"run {dur}", *extra]
        lines += [f"ev {s} {k} {v}" for s, k, v in sorted(events)]
        r = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        n = [int(ln.split()[1]) for ln in r.stderr.splitlines() if ln.startswith("C ")][0]
        return n, self.sim(cph, ratio, init, events, dur, extra)

    def sim_ch(self, cph, ratio, init, events, dur, extra=()):
        lines = [f"cfg {cph} {ratio}", f"init {init}", f"run {dur}", *extra]
        lines += [f"ev {s} {k} {v}" for s, k, v in sorted(events)]
        r = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        rows = [(int(p[1]), int(p[2]), float(p[3]), int(p[4]), float(p[5])) for p in (ln.split() for ln in r.stdout.splitlines())]
        ch = {int(p[1]): int(p[2]) for p in (ln.split() for ln in r.stderr.splitlines() if ln.startswith("H "))}
        return Timeline(rows), ch

    def sim(self, cph, ratio, init, events, dur, extra=()):
        lines = [f"cfg {cph} {ratio}", f"init {init}", f"run {dur}", *extra]
        lines += [f"ev {s} {k} {v}" for s, k, v in sorted(events)]
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        rows = []
        for ln in out.splitlines():
            p = ln.split()
            rows.append((int(p[1]), int(p[2]), float(p[3]), int(p[4]), float(p[5])))
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
        bad = [(s, cs) for s, q, cs, _, _bt in tl.rows if t0 <= s < t0 + 936 and q == 0 and cs != 10.0 and s != t0 + 0]
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
        for s, q, cs, _, _bt in tl.rows:
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
        for s, q, cs, _, _bt in tl.rows:
            if s < hold_end:
                self.assertEqual(q, 1, f"{label}: ON ended early at t={s}s (hold until {hold_end}s); {tl.dump(600)}")
                self.assertNotEqual(cs, 10.0, f"{label}: CS=10 inside the ON phase at t={s}s")
        self.assertLessEqual(abs(offs[0] - hold_end), TICK,
                             f"{label}: ON ended at t={offs[0]} s, expected {hold_end} s (flame-on + 180 s hold)")

    def test_d7_first_flame_after_the_wait_timed_out_does_not_resync_but_holds_180_s(self):
        self._late_ignition(200, "D7 first flame at +200")

    def test_d8_late_ignition_keeps_on_until_180_s_after_flame_on(self):
        self._late_ignition(240, "D8 first flame at +240")

    # TASK-1421 (entries 59, 89, 90, 95; pwm.py:159-160): a start counts at the ON start, no flame counts ------
    def test_e1_unlit_on_start_counts_once(self):
        # Red on HEAD (the count waited for a first flame). duty 0.5 -> on 600 s; the flame never lights; the ON phase
        # runs to 600 s and ends. Exactly the one ON start is counted (run ends before the next ON at 1200 s).
        n, tl = self.starts(3, 0.5, "cold", [], 900)
        ons, _ = tl.edges()
        self.assertEqual(ons, [0], tl.dump(60))
        self.assertEqual(n, 1, f"an ON start whose flame never lights must count once, got {n}")

    def test_e2_ch_ignition_and_relight_inside_on_add_no_count(self):
        # Characterization (green on HEAD: one count per phase there as well, taken at the first flame). Unlit start,
        # CH ignition at +60, flame-out at +200, relight at +300, inside one 600 s ON phase: exactly 1 count.
        n, tl = self.starts(3, 0.5, "cold", [(60, "flame", 1), (200, "flame", 0), (300, "flame", 1)], 900)
        self.assertEqual(tl.edges()[0], [0], tl.dump(60))
        self.assertEqual(n, 1, f"CH ignition and relight must not add a count, got {n}")

    def test_e3_hot_water_flame_inside_on_adds_no_count(self):
        # Characterization (green on HEAD: the start was already counted at its lit start). Burning start at 0, then a
        # hot-water draw (DHW bit + flame off/on) at +100..+160 skips the PWM tick as the control loop does: still 1.
        ev = [(100, "flame", 0), (105, "dhw", 1), (110, "flame", 1), (160, "flame", 0), (160, "dhw", 0)]
        n, tl = self.starts(3, 0.5, "burning", ev, 900)
        self.assertEqual(tl.edges()[0], [0], tl.dump(60))
        self.assertEqual(n, 1, f"a hot-water flame must not add a count, got {n}")

    def test_e4_on_start_at_the_end_of_a_draw_with_flame_lit_is_held_until_the_flame_is_out(self):
        # TASK-1424, entries 105 and 106 (replaces the entry 101 reading of TASK-1421, which counted this start at once):
        # an ON start on the first tick after a draw, with the hot-water flame still lit, is held. Burning start at 0
        # (count 1), ON to 600, OFF to 1200; draw 1190..1215 with the flame lit; tick 1230 holds; flame out at 1250;
        # tick 1260 starts the ON and counts it (total 2, as without the draw).
        base, _ = self.starts(3, 0.5, "burning", [], 1500)
        n, tl = self.starts(3, 0.5, "burning", [(1190, "dhw", 1), (1215, "dhw", 0), (1250, "flame", 0)], 1500)
        self.assertEqual(tl.edges()[0], [0, 1260], tl.dump(1300))
        self.assertEqual((base, n), (2, 2), f"the delayed ON start must count exactly 1, got base={base} n={n}")

    # TASK-1424 (entries 105, 106) ----------------------------------------------------------------------------
    def test_h1_no_on_start_while_the_hot_water_flame_burns(self):
        # Red on alpha.518 (starts at 1230 and counts there). The flame stays lit past the draw: ticks 1230..1320 hold.
        n, tl = self.starts(3, 0.5, "burning", [(1190, "dhw", 1), (1215, "dhw", 0), (1350, "flame", 0)], 1500)
        ons = tl.edges()[0]
        self.assertEqual(ons, [0, 1350], tl.dump(1400))   # flame out at 1350: the first tick after it starts
        self.assertEqual(n, 2, f"one count for the delayed start, got {n}")

    def test_h2_ch_flame_at_the_end_of_the_off_wait_starts_at_once(self):
        # Characterization: a CH-lit flame (no draw) is never held; the OFF wait ends at 1200.
        n, tl = self.starts(3, 0.5, "burning", [], 1500)
        self.assertEqual(tl.edges()[0], [0, 1230], tl.dump(1300))
        self.assertEqual(n, 2)

    def test_h3_range_5_is_held_the_same_way(self):
        # Red on alpha.518 (starts at 30). Duty at the maximum (continuous ON). Draw 0..20 with the flame lit; tick 30
        # holds; flame out at 50; tick 60 starts and counts once.
        n, tl = self.starts(3, 0.95, "burning", [(0, "dhw", 1), (20, "dhw", 0), (50, "flame", 0)], 200)
        self.assertEqual(tl.edges()[0], [60], tl.dump(100))
        self.assertEqual(n, 1)

    def test_h4_the_hold_is_narrated_once(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True, check=True,
                           input=NL.join(["cfg 3 0.5", "init burning", "run 1500", "ev 1190 dhw 1", "ev 1215 dhw 0", "ev 1350 flame 0"]) + NL)
        if "PWM ON held" not in hp.SAT_INO.read_text(encoding="utf-8"):
            self.skipTest("old source")
        self.assertEqual(r.stderr.count("N PWM ON held: hot-water flame still burning"), 1, r.stderr)

    # Review fixes of TASK-1424 --------------------------------------------------------------------------------
    def test_k1_a_draw_shorter_than_one_tick_still_marks_its_flame(self):
        # Red on 8372293 (starts at 1230). Flame out at 700 (OFF phase); the draw begins and ends between two control ticks (DHW bit
        # 1205..1215, flame lit at 1206 and still burning at the tick 1230 with the bit off): the flame-edge path marks it, the start waits
        # for the flame-out at 1350. The harness calls the real satPwmNoteDhwFlameEdge() at its flame edge, as satControlLoopRun() does.
        ev = [(700, "flame", 0), (1205, "dhw", 1), (1206, "flame", 1), (1215, "dhw", 0), (1350, "flame", 0)]
        n, tl = self.starts(3, 0.5, "burning", ev, 1500)
        self.assertEqual(tl.edges()[0], [0, 1350], tl.dump(1400))
        self.assertEqual(n, 2)

    def test_k2_range_5_hold_keeps_the_entry_skip_of_the_off_wait(self):
        # PWM just entered (entry 43), duty in range 5, draw 0..20 with the flame lit: tick 30 holds. The duty then drops to range 3 and
        # the flame goes out at 50: tick 60 starts ON at once; the 936 s off wait (from the entry) must not apply.
        ev = [(0, "dhw", 1), (20, "dhw", 0), (40, "ratio", 22), (50, "flame", 0)]
        n, tl = self.starts(3, 0.95, "burning", ev, 200, ("entry 1",))
        self.assertEqual(tl.edges()[0], [60], tl.dump(120))
        self.assertEqual(n, 1)

    def test_k3_without_overshoot_protection_the_start_is_held_too(self):
        # Entry 234 (Q-1412.6a B, TASK-1562; changes entry 133): since entry 230 every OFF phase stops the boiler without protection
        # too, so the start is held while the hot-water flame burns, as with protection on (e4: start at 1350). beta.11/12: 1230.
        n, tl = self.starts(3, 0.5, "burning", [(1190, "dhw", 1), (1215, "dhw", 0), (1350, "flame", 0)], 1500, ("ovs 0",))
        self.assertEqual(tl.edges()[0], [0, 1350], tl.dump(1400))
        self.assertEqual(n, 2)

    def test_k4_resume_with_the_flame_lit_clears_the_hot_water_mark(self):
        # Entry 103 C: the flame that burns after the draw is the CH flame of the ON phase. Duty 0.9 (range 4: on 1620 s, off 180 s),
        # draw bit 100..400 with the flame lit throughout, resume at the tick 420, ON ends at 1920 with the flame still burning; the
        # next start comes after the 180 s off wait (2100), not after a flame-out.
        n, tl = self.starts(3, 0.9, "burning", [(100, "dhw", 1), (400, "dhw", 0)], 2200)
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0], tl.dump(60))
        self.assertTrue(offs, tl.dump(2200))
        self.assertEqual(len(ons), 2, tl.dump(2200))
        self.assertLessEqual(abs(ons[1] - (offs[0] + 180)), TICK, f"ON ends {offs[0]}, next start {ons[1]}; {tl.dump(2200)}")

    # TASK-1422 (entries 95, 103): hot water inside an ON phase freezes the ON clock ------------------------
    # cycles/hour 3, duty 0.22: ON 264 s, burning start at 0. Draw bit 100..400 (the first loop tick with it is 120, the first
    # tick after it 420: used ON time 120 s, 144 s left). The DHW flame stays lit through the draw unless noted.
    def _draw(self, extra, dur=1000):
        return self.sim(3, 0.22, "burning", [(100, "dhw", 1), (400, "dhw", 0)] + extra, dur)

    def test_f1_draw_in_on_phase_resumes_with_the_remaining_on_time_after_the_ch_relight(self):
        # Red on HEAD: ON ends at the first tick after the draw (420). Fix: DHW flame out at 400, CH relight at 430 (seen at
        # the tick at 450): anchor = 450 - 120, ON ends at 330 + 264 = 594 (tick 600).
        tl = self._draw([(400, "flame", 0), (430, "flame", 1)])
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0], tl.dump(60))
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 594), TICK, f"ON ended at {offs[0]} s, expected 594 s (430 + 164); {tl.dump(700)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 590), tl.dump(700))

    def test_f2_no_ch_flame_after_the_draw_resumes_180_s_after_the_draw_end(self):
        # The draw ends at the tick 420; no CH flame: the clock resumes at 600, 144 s left: ON ends at 744.
        tl = self._draw([(400, "flame", 0)])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 744), TICK, f"ON ended at {offs[0]} s, expected 744 s (600 + 144); {tl.dump(800)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 740), tl.dump(800))

    def test_f3_flame_still_lit_after_the_draw_resumes_at_once(self):
        # The flame burns through the draw and after it: anchor = 420 - 120, ON ends at 300 + 264 = 564.
        tl = self._draw([])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 564), TICK, f"ON ended at {offs[0]} s, expected 564 s (420 + 144); {tl.dump(700)}")
        self.assertGreater(offs[0], 420 + TICK, "ON must not end right after the draw")

    def test_f4_draw_in_off_phase_keeps_the_off_wait_from_the_switch(self):
        # Characterization (green on HEAD): ON 0..270, off wait 936 s, next ON at 270 + 960. A draw in the OFF phase changes nothing.
        ev = [(278, "flame", 0), (500, "dhw", 1), (505, "flame", 1), (800, "flame", 0), (800, "dhw", 0)]
        tl = self.sim(3, 0.22, "burning", ev, 1700)
        ons, offs = tl.edges()
        self.assertEqual(offs[0], 270, tl.dump(300))
        self.assertLessEqual(abs(ons[1] - (270 + 960)), TICK, f"second ON at {ons[1]}; {tl.dump(1400)}")

    def test_f5_no_bt_reseed_across_the_draw(self):
        # BT learned at 50 C stays 50 C at every tick after the draw although the flow is 62 C: no flame sync, no new EMA window.
        tl = self._draw([(400, "flame", 0), (400, "flow", 62), (430, "flame", 1)], 700)
        bts = {bt for s, _q, _c, _f, bt in tl.rows if s >= 420}
        self.assertEqual(bts, {50.0}, f"BT changed across the draw: {tl.rows}")

    # Entry 132 (changes entry 104): a draw during the wait for the first CH flame throws the wait away. The ON time and the 180 s
    # hold start at the first CH flame after the draw, or the ON time 180 s after the draw end without one; no BT seed, no EMA.
    # Cold start: the ON phase begins at 0 with the flame off (waiting).
    def _cold(self, extra, dur=900):
        return self.sim(3, 0.22, "cold", extra, dur)

    def test_g1_draw_during_the_first_ignition_wait_starts_on_time_and_hold_at_the_first_ch_flame(self):
        # Draw bit 40..200, the DHW flame lights at 45 and burns on after the draw: it counts as the CH flame (entry 103 C), the
        # resume tick is 210, ON time (264 s) and hold start there: ON ends at 474 (tick 480), BT stays 50 (no sync reseed, no EMA).
        tl = self._cold([(40, "dhw", 1), (45, "flame", 1), (200, "dhw", 0)])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 474), TICK, f"ON ended at {offs[0]} s, expected 474 s (210 + 264); {tl.dump(600)}")
        self.assertEqual({bt for s, _q, _c, _f, bt in tl.rows if s >= 210}, {50.0}, tl.rows)

    def test_g2_no_flame_after_a_draw_in_the_ignition_wait_starts_the_timer_180_s_after_the_draw_end(self):
        # Draw 40..200, no flame at all. Draw end tick 210, the timer starts at 390 and runs the full 264 s: ON ends at 654 (tick 660).
        tl = self._cold([(40, "dhw", 1), (200, "dhw", 0)])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 654), TICK, f"ON ended at {offs[0]} s, expected 654 s (390 + 264); {tl.dump(800)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 650), tl.dump(800))

    # TASK-1502 AC1: the 170 s example. The draw interrupts the wait after 150 s; the wait before it must not count as ON time.
    def test_m1_ch_flame_after_the_draw_starts_on_time_and_hold_at_that_flame(self):
        # Draw bit 140..400 (first loop tick with it 150), DHW flame at 145 out at 400, CH flame at 430 (seen at the tick 450).
        # Fix: ON time and hold start at 450, ON ends at 714 (tick 720). HEAD counted the 150 s wait as used: anchor 450 - 150, end 564.
        tl = self._cold([(140, "dhw", 1), (145, "flame", 1), (400, "dhw", 0), (400, "flame", 0), (430, "flame", 1)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 714), TICK, f"ON ended at {offs[0]} s, expected 714 s (450 + 264); {tl.dump(800)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 700), tl.dump(800))

    def test_m1b_on_time_runs_from_the_ch_flame_and_bt_is_not_reseeded(self):
        # Range 2 (duty 0.10): ON time 180 s. Same draw; CH flame seen at 450: ON ends at 630. The flow is 62 C after the draw: BT must
        # stay 50. (The ON time and the hold coincide here; the hold itself is pinned by test_m1c.)
        tl = self.sim(3, 0.10, "cold", [(140, "dhw", 1), (145, "flame", 1), (400, "dhw", 0), (400, "flame", 0), (400, "flow", 62), (430, "flame", 1)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 630), TICK, f"ON ended at {offs[0]} s, expected 630 s; {tl.dump(800)}")
        self.assertEqual({bt for s, _q, _c, _f, bt in tl.rows if s >= 420}, {50.0}, tl.rows)

    def test_m1c_the_180_s_hold_starts_at_the_ch_flame_after_the_draw(self):
        # Probe P1. Draw 140..400, CH flame at 430 (seen at 450), flame out and duty range 1 (5 %, ON time 0) at 480. The hold from the
        # flame at 450 keeps the phase ON until 630. Without the hold (HEAD, or the hold dropped for a resumed phase) it ends at 480.
        tl = self._cold([(140, "dhw", 1), (145, "flame", 1), (400, "dhw", 0), (400, "flame", 0), (430, "flame", 1), (480, "flame", 0), (480, "ratio", 5)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 630), TICK, f"ON ended at {offs[0]} s, expected 630 s (450 + 180 hold); {tl.dump(800)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 600), tl.dump(800))

    def test_m2b_a_late_ch_flame_after_the_fallback_gets_the_180_s_hold(self):
        # Probe P2. Draw 140..400 (end tick 420), no flame: the timer starts anyway at 600. A CH flame at 610 still anchors
        # the hold as in thermo-nova (pwm.py:144-146, :201-203); flame out and range 1 at 640: ON ends at 810. Without the late anchor 660.
        tl = self._cold([(140, "dhw", 1), (145, "flame", 1), (400, "dhw", 0), (400, "flame", 0), (610, "flame", 1), (640, "flame", 0), (640, "ratio", 5)], 1100)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 810), TICK, f"ON ended at {offs[0]} s, expected 810 s; {tl.dump(1000)}")

    # Entry 211 (Q-1502b A, TASK-1558): a draw that begins after the 180 s wait timed out, before any CH flame. The CH flame after the
    # draw gets the 180 s hold, as without a draw (thermo-nova pwm.py:144-146, :201-203).
    def test_m4_ch_flame_after_a_draw_after_the_timed_out_wait_gets_the_180_s_hold(self):
        # Wait times out at 180, draw 190..250 (end tick 270, Wait), CH flame 280 (seen at 300), out 310, range 1 (ON time 0).
        # Fix: hold from 300, ON ends at 480. HEAD: no hold anchor in a resumed phase, ends at 330.
        tl = self._cold([(190, "dhw", 1), (250, "dhw", 0), (280, "flame", 1), (310, "flame", 0), (310, "ratio", 5)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 480), TICK, f"ON ended at {offs[0]} s, expected 480 s (300 + 180 hold); {tl.dump(600)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 470), tl.dump(600))

    def test_m6_ch_flame_after_the_fallback_of_a_draw_after_the_timed_out_wait_gets_the_180_s_hold(self):
        # Entry 211, late flame: wait times out at 180, draw 190..250 (end tick 270), no flame: the clock resumes at 450. CH flame at
        # 460 (seen at 480), out 490, range 1: hold from 480, ON ends at 660. HEAD: no anchor, ends at 510.
        tl = self._cold([(190, "dhw", 1), (250, "dhw", 0), (460, "flame", 1), (490, "flame", 0), (490, "ratio", 5)], 1000)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 660), TICK, f"ON ended at {offs[0]} s, expected 660 s (480 + 180 hold); {tl.dump(800)}")

    # TASK-1502 AC2: no CH flame
    def test_m2_no_ch_flame_within_180_s_of_the_draw_end_starts_the_timer_anyway(self):
        # Same draw, no CH flame. Draw end tick 420, the timer starts at 600: ON ends at 864 (tick 870). HEAD: anchor 600 - 150, end 714.
        tl = self._cold([(140, "dhw", 1), (145, "flame", 1), (400, "dhw", 0), (400, "flame", 0)], 1000)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 864), TICK, f"ON ended at {offs[0]} s, expected 864 s (600 + 264); {tl.dump(1000)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 840), tl.dump(1000))

    # TASK-1502 AC3 (case 2): a short draw inside the 180 s hold; the rest of the hold still counts after the draw.
    def test_m3_short_draw_inside_the_hold_keeps_the_rest_of_the_hold(self):
        # ON time 264 s, cold start, the flame lights late at 200 (after the 180 s wait timed out, so the ON timer runs
        # from 0 and the hold anchor is the tick that sees the flame, 210). Draw bit 250..290 (first tick with it 270: 60 s of the hold
        # used, the 264 s of ON time used up), flame lit throughout: resume at the draw-end tick 300 with 120 s of hold left, ON ends at 420.
        # HEAD dropped the hold and ended the phase at the resume tick 300. No BT seed (flow 62 after the draw).
        tl = self.sim(3, 0.22, "cold", [(200, "flame", 1), (250, "dhw", 1), (260, "flow", 62), (290, "dhw", 0)], 700)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 420), TICK, f"ON ended at {offs[0]} s, expected 420 s (300 + 120); {tl.dump(600)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 410), tl.dump(600))
        self.assertEqual({bt for s, _q, _c, _f, bt in tl.rows if s >= 300}, {50.0}, tl.rows)

    # Entry 158 (Q-1502a B, TASK-1518): the hold protects a burning flame only. No CH flame after the draw: the hold lapses and the
    # ON phase ends on its normal ON time.
    def test_m5_no_ch_flame_after_the_draw_lets_the_hold_lapse(self):
        # Burning start at 0, draw 50..90 with the flame out for good, range 1 at 95 (ON time 0). Draw seen at 60 (60 s used), end tick
        # 90, the 180 s wait runs to 270: ON ends at 270. HEAD resumed the frozen hold (120 s left) and ended at 390.
        tl = self.sim(3, 0.22, "burning", [(50, "dhw", 1), (55, "flame", 0), (90, "dhw", 0), (95, "ratio", 5)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 270), TICK, f"ON ended at {offs[0]} s, expected 270 s (hold lapsed); {tl.dump(600)}")

    def test_m5b_ch_flame_in_the_post_draw_wait_keeps_the_rest_of_the_hold(self):
        # Control for entry 158: same draw, CH flame back at 100 (seen at 120) and out at 130, range 1. The hold had run 60 s at the draw:
        # it continues from the resume, 120 s left, ON ends at 240. Unchanged by the fix.
        tl = self.sim(3, 0.22, "burning", [(50, "dhw", 1), (55, "flame", 0), (90, "dhw", 0), (95, "ratio", 5), (100, "flame", 1), (130, "flame", 0)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 240), TICK, f"ON ended at {offs[0]} s, expected 240 s (120 + 120 hold left); {tl.dump(600)}")

    def test_m5c_second_draw_after_a_lapsed_hold_gives_no_fresh_hold(self):
        # Review finding (entries 103 B, 158): draw 1 50..90 with the flame out, the hold lapses at 270; CH relight at 300 (no new hold,
        # thermo-nova keeps its first anchor, pwm.py:144-146). Draw 2 330..360 with the flame lit, range 1 at 365 (ON time 180 with a flame).
        # Draw 2 seen at 330 (120 s used), resume at 360: ON ends at 360 - 120 + 180 = 420. A fresh hold after draw 2 would end at 540.
        tl = self.sim(3, 0.22, "burning", [(50, "dhw", 1), (55, "flame", 0), (90, "dhw", 0), (300, "flame", 1),
                                            (330, "dhw", 1), (360, "dhw", 0), (365, "ratio", 5)], 900)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 420), TICK, f"ON ended at {offs[0]} s, expected 420 s (no fresh hold); {tl.dump(700)}")

    # The same three draw cases for a heat pump (satsource 2): the minimum burn is 600 s (entry 197), the post-draw wait stays 180 s.
    def test_m7_heat_pump_no_ch_flame_after_the_draw_lets_the_600_s_hold_lapse(self):
        # As m5: ON ends at the fallback, 270 s. HEAD resumed the frozen 600 s hold (540 s left) and ended at 810.
        tl = self.sim(3, 0.22, "burning", [(50, "dhw", 1), (55, "flame", 0), (90, "dhw", 0), (95, "ratio", 5)], 1200, extra=["src 2"])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 270), TICK, f"ON ended at {offs[0]} s, expected 270 s (hold lapsed); {tl.dump(900)}")

    def test_m7b_heat_pump_ch_flame_in_the_post_draw_wait_keeps_the_rest_of_the_600_s_hold(self):
        # As m5b: 60 s of the 600 s used at the draw, resume at 120 with 540 s left: ON ends at 660.
        tl = self.sim(3, 0.22, "burning", [(50, "dhw", 1), (55, "flame", 0), (90, "dhw", 0), (95, "ratio", 5), (100, "flame", 1), (130, "flame", 0)],
                      1200, extra=["src 2"])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 660), TICK, f"ON ended at {offs[0]} s, expected 660 s (120 + 540); {tl.dump(900)}")

    def test_m7c_heat_pump_ch_flame_after_a_draw_after_the_timed_out_wait_gets_the_600_s_hold(self):
        # As m4: CH flame seen at 300, the 600 s hold keeps ON until 900. HEAD: no anchor, ends at 330.
        tl = self.sim(3, 0.22, "cold", [(190, "dhw", 1), (250, "dhw", 0), (280, "flame", 1), (310, "flame", 0), (310, "ratio", 5)], 1200,
                      extra=["src 2"])
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 900), TICK, f"ON ended at {offs[0]} s, expected 900 s (300 + 600 hold); {tl.dump(1100)}")

    # LL-017 (TASK-1412.6, Sergeant D entry 230, QD-4 B): every PWM OFF phase stops the boiler (CS=10, CH=0), whatever overshoot
    # protection is. Protection off only drops the ON-phase ignition hold and suppression; the ON phase then keeps the previous CS.
    # cycles/hour 3, duty 0.22 (requested 32.2): ON 0..270, OFF 270..1230 (the timing does not depend on protection).
    def test_ll017_off_phase_stops_the_boiler_without_overshoot_protection(self):
        # Red on HEAD: the OFF phase kept the previous CS (32.2) with CH=1.
        tl, ch = self.sim_ch(3, 0.22, "burning", [], 1300, ("ovs 0",))
        ons, offs = tl.edges()
        self.assertEqual((ons[:2], offs[:1]), ([0, 1230], [270]), tl.dump(1300))
        off = [(s, cs) for s, q, cs, _f, _b in tl.rows if 270 <= s < 1230]
        self.assertTrue(off and all(cs == 10.0 for _s, cs in off), f"OFF-phase CS not 10: {off}")
        self.assertTrue(all(ch[s] == 0 for s, _cs in off), f"OFF-phase CH not 0: {[(s, ch[s]) for s, _c in off]}")

    def test_ll017_on_phase_without_protection_has_no_hold_or_suppression(self):
        # ON ticks: CH=1 and the CS is the requested setpoint (32.2), never a flame-off hold (return + offset) or a suppression CS
        # (flow - offset). The second ON phase starts from CS=10: it takes the requested setpoint again, not the 10.
        tl, ch = self.sim_ch(3, 0.22, "burning", [(1240, "flame", 0)], 1500, ("ovs 0",))
        on = [(s, cs) for s, q, cs, _f, _b in tl.rows if q]
        self.assertTrue(on and all(abs(cs - 32.2) < 0.06 for _s, cs in on), f"ON-phase CS: {on}")
        self.assertTrue(all(ch[s] == 1 for s, _cs in on), f"ON-phase CH not 1: {[(s, ch[s]) for s, _c in on]}")

    def test_ll017_toggling_protection_keeps_the_phase_timing_and_the_off_phase_actuation(self):
        # Protection on at the start, off at 400 (OFF phase), on again at 1300 (second ON phase): the edges are the ones of a run with
        # protection on throughout, the OFF phase sends CS=10/CH=0 on both sides of the toggle, and the second ON phase (protection back
        # on) gets the ON-phase override again (CH=1, CS above 10).
        ref, _ = self.sim_ch(3, 0.22, "burning", [], 1600)
        tl, ch = self.sim_ch(3, 0.22, "burning", [(400, "ovs", 0), (1300, "ovs", 1)], 1600)
        self.assertEqual(tl.edges(), ref.edges(), tl.dump(1600))
        off = [(s, cs) for s, q, cs, _f, _b in tl.rows if 270 <= s < 1230]
        self.assertTrue(all(cs == 10.0 and ch[s] == 0 for s, cs in off), f"OFF phase across the toggle: {[(s, cs, ch[s]) for s, cs in off]}")
        on2 = [(s, cs) for s, q, cs, _f, _b in tl.rows if q and s >= 1300]
        self.assertTrue(on2 and all(cs > 10.0 and ch[s] == 1 for s, cs in on2), f"second ON phase: {[(s, cs, ch[s]) for s, cs in on2]}")

    def test_ll017_protection_off_inside_an_on_phase_keeps_its_cs_until_the_off_phase(self):
        # Characterization (same on HEAD, entry 231 reading 1): protection on, turned off at 150 s inside the ON phase 0..270. The ON phase
        # keeps its last protected CS (no new override, no switch to the requested setpoint) until it ends; the OFF phase then sends 10/CH=0.
        tl, ch = self.sim_ch(3, 0.22, "burning", [(150, "ovs", 0)], 1300)
        before = [cs for s, q, cs, _f, _b in tl.rows if q and s == 120][0]
        kept = [cs for s, q, cs, _f, _b in tl.rows if q and 150 <= s < 270]
        self.assertTrue(kept and all(cs == before for cs in kept), f"CS after the toggle: {kept}, before {before}")
        self.assertTrue(all(cs == 10.0 and ch[s] == 0 for s, q, cs, _f, _b in tl.rows if 270 <= s < 1230), tl.dump(1300))

    def test_ll017_heat_pump_off_phase_holds_cs10_for_the_600_s_minimum_off_without_protection(self):
        # Heat pump (src 2), protection off. Duty 0.5, period 1800 s: ON 0..900. At 905 the demand jumps to range 5 (continuous ON);
        # the TASK-1542 minimum OFF time (600 s, entry 198) keeps the OFF phase until 1500, and every OFF tick sends CS=10/CH=0.
        tl, ch = self.sim_ch(3, 0.5, "burning", [(905, "ratio", 95)], 1700, ("ovs 0", "src 2"))
        ons, offs = tl.edges()
        self.assertEqual(offs[:1], [900], tl.dump(1700))
        self.assertTrue(len(ons) >= 2 and abs(ons[1] - 1500) <= TICK, f"second ON at {ons[1:2]}, expected 1500; {tl.dump(1700)}")
        off = [(s, cs) for s, q, cs, _f, _b in tl.rows if 900 <= s < ons[1]]
        self.assertTrue(off and all(cs == 10.0 and ch[s] == 0 for s, cs in off), f"OFF phase: {[(s, cs, ch[s]) for s, cs in off]}")

    def test_ll017_continuous_path_does_not_read_overshoot_protection(self):
        # Structural guard, not a behaviour test: the continuous step (satApplyContinuous) never reads the option, so toggling it in
        # continuous mode cannot change the CS. A read added there later fails this test.
        src = hp.SAT_INO.read_text(encoding="utf-8")
        a = src.index("static float satApplyContinuous(")
        b = src.index("\n}\n", a)
        self.assertNotIn("bOvershootProtection", src[a:b])

    def test_ll017_cold_start_without_protection_waits_with_the_requested_setpoint(self):
        # Startup: cold start, protection off, the ON phase waits for ignition with CH=1 and the requested setpoint (no flame-off hold);
        # after the ON phase the OFF phase sends CS=10/CH=0.
        tl, ch = self.sim_ch(3, 0.22, "cold", [(100, "flame", 1)], 1300, ("ovs 0",))
        wait = [(s, cs) for s, q, cs, f, _b in tl.rows if q and not f]
        self.assertTrue(wait and all(abs(cs - 32.2) < 0.06 and ch[s] == 1 for s, cs in wait), f"waiting ON ticks: {wait}")
        _ons, offs = tl.edges()
        self.assertTrue(offs, tl.dump())
        self.assertTrue(all(cs == 10.0 and ch[s] == 0 for s, q, cs, _f, _b in tl.rows if s >= offs[0] and not q), tl.dump(1300))

    def test_g3_short_draw_inside_the_start_up_window_leaves_bt_alone(self):
        # Burning start at 0 (window 0..180, BT 50). Draw 50..85 with the flow at 62 C afterwards (first tick with the bit 60, resume at
        # 90, 60 s used): the EMA must not follow the hot flow, BT stays 50; ON ends at 90 - 60 + 264 = 294 (tick 300).
        tl = self.sim(3, 0.22, "burning", [(50, "dhw", 1), (50, "flow", 62), (85, "dhw", 0)], 700)
        self.assertEqual({bt for s, _q, _c, _f, bt in tl.rows if s >= 90}, {50.0}, tl.rows)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 294), TICK, f"ON ended at {offs[0]} s, expected 294 s; {tl.dump(500)}")

    def test_g4_second_draw_in_the_post_draw_wait_counts_the_180_s_from_the_last_draw_end(self):
        # Draw 1 40..200 (tick 210 = end, Wait), draw 2 bit 250..320 (ticks 270..300 skipped, end tick 330). No flame. The wait runs
        # 330..510, the timer starts at 510 and runs the full 264 s: ON ends at 774 (tick 780). Counting from the first draw end it would be 654.
        tl = self._cold([(40, "dhw", 1), (200, "dhw", 0), (250, "dhw", 1), (320, "dhw", 0)], 1000)
        _ons, offs = tl.edges()
        self.assertTrue(offs, f"ON never ended: {tl.dump()}")
        self.assertLessEqual(abs(offs[0] - 774), TICK, f"ON ended at {offs[0]} s, expected 774 s; {tl.dump(900)}")
        self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < 760), tl.dump(900))

    def test_f6_pause_and_resume_are_narrated_once_each(self):
        lines = ["cfg 3 0.22", "init burning", "run 700", "ev 100 dhw 1", "ev 400 dhw 0"]
        r = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        narr = [ln[2:] for ln in r.stderr.splitlines() if ln.startswith("N ") and "hot water" in ln]
        self.assertEqual(narr, ["PWM ON paused for hot water (used 120 s of 264 s)",
                                "PWM ON resumed after hot water (144 s left)"], narr)
        self.assertLess(max(len(x) for x in narr), 96)

    # TASK-1412.9, LL-019 matrix per source. Gas cells are the tests above; these are the heat pump (src 2: 1800 s period, minimum ON
    # and OFF 600 s, no hourly limit, entries 40, 50, 81, 197, 198) and hybrid (src 3: heat-pump period, gas minimums 180 s / 0 s,
    # hourly limit kept, entries 188, 197, 198) cells. Every run is the real satApplyPWM() through the harness above.
    SRC_HP, SRC_HYB = 2, 3

    def test_n1_unlit_on_start_counts_once_for_heat_pump_and_hybrid(self):
        # Entry 90 (pwm.py:160): the count is taken at the ON start, lit or not. Duty 0.5, period 1800 s: ON 900 s, the flame never lights.
        for src in (self.SRC_HP, self.SRC_HYB):
            with self.subTest(src=src):
                n, tl = self.starts(3, 0.5, "cold", [], 1000, (f"src {src}",))
                ons, offs = tl.edges()
                self.assertEqual((ons, n), ([0], 1), tl.dump(60))
                self.assertLessEqual(abs(offs[0] - 900), TICK, f"ON ended at {offs}, expected 900 s from the start (no abort at 180 s)")

    def test_n2_ch_ignition_and_relight_add_no_count_for_heat_pump_and_hybrid(self):
        for src in (self.SRC_HP, self.SRC_HYB):
            with self.subTest(src=src):
                n, tl = self.starts(3, 0.5, "cold", [(60, "flame", 1), (200, "flame", 0), (300, "flame", 1)], 1000, (f"src {src}",))
                self.assertEqual((tl.edges()[0], n), ([0], 1), tl.dump(60))

    def test_n3_range_5_start_counts_once_for_heat_pump_and_hybrid(self):
        # Duty 0.97 is over the maximum for both: continuous ON, one counted start, the ignition and a relight add none (entry 90).
        for src in (self.SRC_HP, self.SRC_HYB):
            with self.subTest(src=src):
                n, tl = self.starts(3, 0.97, "cold", [(60, "flame", 1), (200, "flame", 0), (300, "flame", 1)], 600, (f"src {src}",))
                self.assertEqual((tl.edges()[0], n), ([0], 1), tl.dump(60))

    def test_n4_zero_on_time_never_starts_for_heat_pump_and_hybrid(self):
        # Entry 80 (Q38 A, pwm.py:155): the mapped ON time must reach the minimum. Duty 0.02 is below the minimum duty of both, no flame: ON 0.
        for src in (self.SRC_HP, self.SRC_HYB):
            with self.subTest(src=src):
                n, tl = self.starts(3, 0.02, "cold", [], 6000, (f"src {src}",))
                self.assertEqual((tl.edges()[0], n), ([], 0), tl.dump(300))

    def test_n5_minimum_on_is_600_s_for_a_heat_pump_and_180_s_for_a_hybrid(self):
        # Duty 0.07 with the flame burning from the start. Hybrid: range 2, ON 180 s (decided: entry 82 Q37 A gas timing with minimum ON 180 s, entry 188 only the PWM cycle comes from the heat pump).
        # Heat pump: below its minimum duty 0.1667, range 1 with a flame: ON = the 600 s minimum (entry 197). Gas is test_gas_on_phase_min_on_180.
        # The hybrid 180 s pins Sergeant D's decision: entry 82 (Q37 A), entry 188 ("everything else stays as for a gas boiler"), entry 198 (600 s only for heat pumps).
        for src, expected in ((self.SRC_HYB, 180), (self.SRC_HP, 600)):
            with self.subTest(src=src):
                tl = self.sim(3, 0.07, "burning", [], 900, (f"src {src}",))
                offs = tl.edges()[1]
                self.assertTrue(offs, tl.dump(700))
                self.assertLessEqual(abs(offs[0] - expected), TICK, f"ON ended at {offs[0]} s, expected {expected} s; {tl.dump(700)}")
                self.assertTrue(all(q == 1 for s, q, _c, _f, _b in tl.rows if s < expected - TICK), tl.dump(700))

    def test_n6_heat_pump_entry_starts_on_at_once_and_holds_600_s(self):
        # Entry 43 with a heat pump: the first phase after PWM is entered is ON at once, no 900 s off wait; with the burner lit and the demand
        # in range 1 the ON phase lasts the 600 s minimum (entry 197), not 180 s. Entry with ON time 0 (flame off, range 1) does not start.
        tl = self.sim(3, 0.5, "cold", [], 300, ("entry 1", f"src {self.SRC_HP}"))
        self.assertEqual(tl.edges()[0][:1], [0], tl.dump(60))
        tl = self.sim(3, 0.07, "burning", [], 900, ("entry 1", f"src {self.SRC_HP}"))
        ons, offs = tl.edges()
        self.assertEqual(ons[:1], [0], tl.dump(60))
        self.assertLessEqual(abs(offs[0] - 600), TICK, tl.dump(700))
        tl = self.sim(3, 0.02, "cold", [], 300, ("entry 1", f"src {self.SRC_HP}"))
        self.assertEqual(tl.edges()[0], [], tl.dump(300))

    def test_n7_heat_pump_failed_and_delayed_ignition_keep_the_phase_timing(self):
        # No flame at all: no abort at 180 s, the ON time (900 s) runs from the phase start (pwm.py:197-199, entry 80).
        tl = self.sim(3, 0.5, "cold", [], 1000, (f"src {self.SRC_HP}",))
        offs = tl.edges()[1]
        self.assertLessEqual(abs(offs[0] - 900), TICK, tl.dump(1000))
        # Ignition at 120 s: the ON time counts from the ignition (pwm.py:186-188): ON ends at 120 + 900.
        tl = self.sim(3, 0.5, "cold", [(120, "flame", 1)], 1200, (f"src {self.SRC_HP}",))
        offs = tl.edges()[1]
        self.assertLessEqual(abs(offs[0] - 1020), TICK, tl.dump(1200))

    def test_n8_hybrid_range_5_restarts_without_the_600_s_minimum_off(self):
        # Entry 198: 600 s minimum OFF for heat pumps only ("Only heat pump. Gas boilers are not so sensitive"); a hybrid keeps the gas rules, OFF 0 s (decided: entries 82 and 188).
        # Duty 0.5, ON 0..900, then range 5 at 905: a hybrid restarts at the next tick, a heat pump after 600 s
        # (the heat-pump half is test_ll017_heat_pump_off_phase_holds_cs10_for_the_600_s_minimum_off_without_protection).
        tl = self.sim(3, 0.5, "burning", [(905, "ratio", 95)], 1700, (f"src {self.SRC_HYB}",))
        ons, offs = tl.edges()
        self.assertEqual(offs[:1], [900], tl.dump(1000))
        self.assertTrue(len(ons) >= 2 and ons[1] <= 960, f"second ON at {ons[1:2]}, expected about 930; {tl.dump(1000)}")

    def test_n9_hot_water_in_the_off_phase_does_not_move_the_heat_pump_off_wait(self):
        # Entry 97 (Q51b) for a heat pump: ON 0..900, OFF wait 900 s from the switch. A draw with a flame in the OFF phase changes nothing.
        ev = [(1000, "dhw", 1), (1005, "flame", 1), (1090, "flame", 0), (1090, "dhw", 0)]
        tl = self.sim(3, 0.5, "burning", [(905, "flame", 0)] + ev, 2200, (f"src {self.SRC_HP}",))
        ons, offs = tl.edges()
        self.assertEqual(offs[:1], [900], tl.dump(1000))
        self.assertTrue(len(ons) >= 2 and abs(ons[1] - 1800) <= TICK, f"second ON at {ons[1:2]}, expected 1800; {tl.dump(2000)}")

    def at_max_duty(self, cph, ratio, init, events, dur, extra=()):
        """{tick second: 1 when the real schedule left the duty in range 5 (SAT sends MM=T)}."""
        lines = [f"cfg {cph} {ratio}", f"init {init}", f"run {dur}", *extra]
        lines += [f"ev {s} {k} {v}" for s, k, v in sorted(events)]
        r = subprocess.run([str(self.exe)], input=NL.join(lines) + NL, capture_output=True, text=True, check=True)
        return {int(p[1]): int(p[2]) for p in (ln.split() for ln in r.stderr.splitlines() if ln.startswith("M "))}

    def test_n11_mm_t_flag_is_up_exactly_while_the_duty_is_in_range_5(self):
        # TASK-1581, entries 242 and 243: duty 0.5 (range 3), then 0.95 (range 5) from 905, then 0.5 again from 1805.
        # Any phase counts (a heat pump restart wait is OFF): the flag follows the duty only.
        for src in (None, self.SRC_HP, self.SRC_HYB):
            with self.subTest(src=src):
                extra = (f"src {src}",) if src else ()
                m = self.at_max_duty(3, 0.5, "burning", [(905, "ratio", 95), (1805, "ratio", 50)], 2400, extra)
                self.assertTrue(all(m[s] == 0 for s in m if s < 905), m)
                self.assertTrue(all(m[s] == 1 for s in range(930, 1800, 30)), m)
                self.assertTrue(all(m[s] == 0 for s in range(1830, 2400, 30)), m)

    def test_n10_phase_edges_survive_the_millis_rollover(self):
        # LL-019 timestamp rollover: the same runs with millis() just below 2^32 (the wrap falls inside the run, at second 400) give the
        # same edges and the same counted starts as with a base far from the wrap. Phase anchors, ignition wait, hold and OFF wait are all
        # elapsed = millis() - anchor in uint32 arithmetic.
        wrap = f"base {2**32 - 400000}"
        cases = (("gas", 3, 0.22, "burning", [], 3000, ()),
                 ("gas cold", 3, 0.5, "cold", [(120, "flame", 1)], 3000, ()),
                 ("heat pump", 3, 0.5, "burning", [], 4000, (f"src {self.SRC_HP}",)),
                 ("hybrid draw", 3, 0.22, "burning", [(100, "dhw", 1), (400, "dhw", 0)], 3000, (f"src {self.SRC_HYB}",)))
        for name, cph, ratio, init, ev, dur, extra in cases:
            with self.subTest(case=name):
                n0, ref = self.starts(cph, ratio, init, ev, dur, extra)
                n1, wrapped = self.starts(cph, ratio, init, ev, dur, (*extra, wrap))
                self.assertGreaterEqual(len(ref.edges()[0]), 2, ref.dump(dur))
                self.assertEqual(wrapped.edges(), ref.edges(), f"{wrapped.dump(dur)}\nvs\n{ref.dump(dur)}")
                self.assertEqual(n1, n0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
