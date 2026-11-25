#!/usr/bin/env python3
"""TASK-3039 / ADR-296: host test for the one PWM ON clock per flame (Sergeant D entries 475, 476, 478, 479).

Inside one flame SAT can flip between PWM and continuous (entry 429: only the fail-safe comes back to PWM inside the burn).
Before the change every PWM re-entry opened a fresh ON phase with a full new ON time. The rule now:
  - leaving PWM while the flame burns freezes the ON time used so far (only PWM time counts, entry 476);
  - the next ON phase of the same flame resumes it by setting the ON anchor back, as the hot-water freeze does (entry 95);
  - the kept time is cleared when the flame goes out, when an ON phase ends by its own timer, and on heater off / SAT disable;
  - the minimum burn (180 s, 600 s heat pump) counts from the real flame-on of the flame across flips (entry 479).

The code under test is NOT re-implemented: the real PWM block of SATcontrol.ino (satPwmBeginOn, satPwmSchedule, satPwmEndOn,
satPwmReset, satSetControlMode, satPwmDisableOnHeaterOff, the hot-water hooks) is compiled as tests/test_sat_pwm_anchors.py
does (shared harness tests/test_sat_pwm_override.py / test_sat_hp_pwm.py), with this file's own MAIN.

Harness limits (listed as bench gaps in TASK-3039):
  - The real flame-edge handler of satControlLoopRun() (SATcontrol.ino ~8109-8126) and satDisable() (~4385-4400) sit outside the
    PWM cut. The driver performs what they do at the flame edge (`_sat_chFlameOnMs` set/cleared, the kept-time clear) and for
    "disable" (`satSetControlMode(OFF)`, `satPwmReset()`, the kept-time clear). That the real handlers make exactly these calls,
    in this order, is pinned by the source-read tests at the end of this file.
  - The end-of-cycle verdict (satCycleApplyEndOfCycle, SATcycles.ino:1033-1046) is not cut in: the "eoc" event performs its
    `satSetControlMode(SAT_MODE_CONTINUOUS)` after the flame-edge hook, which is the order of satCycleOnFlameChange().
  - The control tick is 30 s; ticks are skipped in continuous mode and during a hot-water draw (satControlLoopRun()).
The two edge cases F8 (end-of-cycle verdict at flame-off) and the order of the clear after the mode switch (heater off) were
also run against a mutant of the source (see the TASK-3039 notes): they go red there.

OTGW_SAT_INO points at another copy for old-vs-fix runs.
Run: python tests/test_sat_pwm_on_freeze.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_hp_pwm as hp  # noqa: E402  (builds the real PWM cut; its MAIN is replaced below)

TICK = 30
SRC = hp.SAT_INO.read_text(encoding="utf-8")
HAS_FREEZE = "satPwmKeptClear" in SRC

MAIN = r"""
#include <iostream>
#include <vector>
#include <string>
@CHVAR@
struct Ev { int sec; std::string kind; int val; };
static long long rel(uint32_t v, uint32_t base) { return v == 0 ? -999999LL : (long long)(int32_t)(v - base) / 1000; }
int main() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  satPwmReset();
  _pwm_effectiveBoilerTemp = 50.0f;
  _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  state.sat.eControlMode = SAT_MODE_PWM; state.sat.bDhwActive = false;
  std::string cmd; int cph = 3; double ratio = 0.22, dur = 0; std::string init = "burning"; std::vector<Ev> evs;
  int nochon = 0; unsigned long long baseMs = 100000000ULL;
  while (std::cin >> cmd) {
    if (cmd == "cfg") std::cin >> cph >> ratio;
    else if (cmd == "init") std::cin >> init;
    else if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
    else if (cmd == "src") std::cin >> g_source;
    else if (cmd == "nochon") std::cin >> nochon;      // the CH flame-on edge records nothing (_sat_chFlameOnMs stays 0, SATcontrol.ino ~8121-8125)
  }
  settings.sat.bOvershootProtection = true;
  settings.sat.iCyclesPerHour = (uint8_t)cph;
  float pid = 27.2f + (float)ratio * (50.0f - 27.2f);
  g_ms = (uint32_t)baseMs; const uint32_t base = g_ms;
  if (init == "burning") {                       // flame lit long ago (real flame-on 4000 s ago), the first tick starts ON
    g_flame = true; g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
    _sat_chFlameOnMs = nochon ? 0 : base - 4000000UL;
  } else {                                       // "cold": flame off
    g_flame = false; g_flameOnStart = base - 1500000UL; g_flameOffStart = base - 1400000UL; _sat_chFlameOnMs = 0;
  }
  state.sat.bPwmFlameRequested = false; bool prevDhw = false;
  _pwm_entryPending = true; _pwm_lastUpdateMs = base;        // satSetControlMode(PWM) armed this: the first step starts ON (entry 43)
  for (int sec = 0; sec <= (int)dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
      else if (e.kind == "ratio") pid = 27.2f + (float)e.val / 100.0f * (50.0f - 27.2f);
      else if (e.kind == "mode") satSetControlMode(e.val == 2 ? SAT_MODE_PWM : SAT_MODE_CONTINUOUS);   // control_mode command, exits, fail-safe
      else if (e.kind == "eoc") satSetControlMode(SAT_MODE_CONTINUOUS);                                 // satCycleApplyEndOfCycle(): after the flame-off hook
      else if (e.kind == "heateroff") satPwmDisableOnHeaterOff();
      else if (e.kind == "disable") { satSetControlMode(SAT_MODE_OFF); satPwmReset(); @KEPTCLEAR@ }   // satDisable(): SATcontrol.ino ~4395-4398
      else if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) { g_flameOnStart = g_ms; if (!nochon && !state.sat.bDhwActive) _sat_chFlameOnMs = g_ms; }
        else { g_flameOffStart = g_ms; _sat_chFlameOnMs = 0; @KEPTCLEAR@ }                              // flame-edge handler, SATcontrol.ino ~8118-8125
      }
    }
    if (sec % 30 != 0) continue;
    if (state.sat.eControlMode != SAT_MODE_PWM) continue;       // continuous: the PWM schedule does not run
    if (state.sat.bDhwActive) { if (!prevDhw) { @DHWSTART@ } prevDhw = true; continue; }
    if (prevDhw) { @DHWEND@ }
    prevDhw = false;
    const float cs = satApplyPWM(pid);
    _sat_lastControlCS = cs;
    printf("T %d %d %.1f %d %lld %lld %d\n", sec, (int)state.sat.bPwmFlameRequested, cs, (int)g_flame,
           rel(_pwm_flameOnMs, base), rel(_pwm_lastUpdateMs, base), @KEPTVAL@);
  }
  return 0;
}
"""


class Timeline:
    def __init__(self, rows):
        self.rows = rows  # dict sec -> (req, flame, flameOnRel, lastUpdateRel, keptValid, keptMsRel)

    def req(self, sec):
        return self.rows[sec][0]

    def ticks(self):
        return sorted(self.rows)

    def first_off_after(self, sec):
        """First tick after `sec` whose ON request is 0, with ON seen before it."""
        seen_on = False
        for s in self.ticks():
            if s < sec:
                continue
            if self.rows[s][0]:
                seen_on = True
            elif seen_on:
                return s
        return None

    def dump(self, upto=None):
        return " ".join(f"{s}:{'ON' if self.rows[s][0] else 'off'}" for s in self.ticks() if upto is None or s <= upto)


class SatPwmOnFreeze(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        chvar = "" if "_sat_chFlameOnMs" in hp.base.extract(SRC) else "static uint32_t _sat_chFlameOnMs = 0;"
        hp.MAIN = (MAIN.replace("@CHVAR@", chvar)
                   .replace("@KEPTCLEAR@", "satPwmKeptClear();" if HAS_FREEZE else "")
                   .replace("@KEPTVAL@", "(int)_pwm_onKeptValid" if HAS_FREEZE else "0")
                   .replace("@DHWSTART@", "satPwmOnDhwStart();").replace("@DHWEND@", "satPwmOnDhwEnd(g_flame);"))
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = hp.build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def sim(self, events, dur, ratio=0.22, init="burning", extra=()):
        lines = [f"cfg 3 {ratio}", f"init {init}", f"run {dur}", *extra]
        lines += [f"ev {s} {k} {v}" for s, k, v in sorted(events, key=lambda e: e[0])]
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
        rows = {}
        for ln in out.splitlines():
            p = ln.split()
            rows[int(p[1])] = (int(p[2]), int(p[4]), int(p[5]), int(p[6]), int(p[7]))
        return Timeline(rows)

    # cycles/hour 3 -> period 1200 s, duty 0.22 -> ON 264 s. The ON phase starts at tick 0 with the flame burning.
    ON264 = 264

    # F10 ---------------------------------------------------------------------------------------------
    def test_f10_first_entry_with_nothing_kept_is_the_same_on_phase_as_before(self):
        tl = self.sim([], 600)
        self.assertEqual(tl.first_off_after(0), 270, tl.dump())
        # unchanged: the minimum-burn anchor of a first entry is the ON start (tick 0), not the older real flame-on (-4000 s)
        self.assertEqual(tl.rows[0][2], 0, "first ON phase: _pwm_flameOnMs is the ON start")
        self.assertEqual(tl.rows[0][4], 0, "nothing kept")

    # F1 ----------------------------------------------------------------------------------------------
    def test_f1_one_flip_ends_when_the_pwm_time_of_both_segments_adds_up_to_the_on_time(self):
        # PWM 0..100 (100 s used), continuous 100..130, fail-safe re-entry at 130: the ON phase resumes at the tick 150
        # with 100 s used, so it ends once 164 s more PWM time ran: tick 330 (old code: 150 + 264 = 414, tick 420).
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 700)
        self.assertEqual(tl.first_off_after(150), 330, tl.dump())
        self.assertTrue(all(s not in tl.rows for s in (120,)), "continuous: no PWM tick at 120")

    # F2 ----------------------------------------------------------------------------------------------
    def test_f2_three_flips_sum_to_the_on_time(self):
        # PWM: 0..100, 150..200, 240..270, 300..: 100 + 50 + 30 = 180 used, 84 s left: tick 390 (old: 300 + 264 -> tick 570)
        ev = [(100, "mode", 1), (130, "mode", 2), (200, "mode", 1), (230, "mode", 2), (270, "mode", 1), (300, "mode", 2)]
        tl = self.sim(ev, 800)
        self.assertEqual(tl.first_off_after(300), 390, tl.dump())

    # F3 ----------------------------------------------------------------------------------------------
    def test_f3_flame_out_between_segments_restarts_the_clock_at_zero(self):
        ev = [(100, "mode", 1), (110, "flame", 0), (120, "flame", 1), (130, "mode", 2)]
        tl = self.sim(ev, 700)
        self.assertEqual(tl.first_off_after(150), 150 + 270, tl.dump())   # the tick 420: a full ON time from the re-entry tick 150
        self.assertEqual(tl.rows[150][4], 0, "nothing kept after the flame-out")

    # F4 ----------------------------------------------------------------------------------------------
    def test_f4_an_on_phase_ended_by_its_timer_starts_the_next_one_at_zero(self):
        # flip as F1 (ends at 330), the OFF phase runs 936 s, the next ON phase starts at the tick after 330 + 936
        ev = [(100, "mode", 1), (130, "mode", 2)]
        tl = self.sim(ev, 2000)
        off1 = tl.first_off_after(150)
        self.assertEqual(off1, 330, tl.dump())
        on2 = next(s for s in tl.ticks() if s > off1 and tl.rows[s][0])
        off2 = tl.first_off_after(on2)
        self.assertEqual(off2 - on2, 270, f"second ON phase {on2}..{off2}: a full ON time, nothing carried over; {tl.dump()}")
        self.assertEqual(tl.rows[on2][4], 0, "nothing kept for the next ON phase")

    # F5 ----------------------------------------------------------------------------------------------
    def test_f5a_hot_water_draw_inside_a_resumed_clock_is_not_counted(self):
        # resume at 150 (100 s kept), draw 170..240 (the tick 180 sees the bit, the tick 240 sees it drop): the draw is
        # excluded, so the ON phase still ends 60 s later than without the draw: tick 390 (without the draw 330)
        tl_plain = self.sim([(100, "mode", 1), (130, "mode", 2)], 700)
        tl = self.sim([(100, "mode", 1), (130, "mode", 2), (170, "dhw", 1), (235, "dhw", 0)], 700)
        self.assertEqual(tl_plain.first_off_after(150), 330)
        self.assertEqual(tl.first_off_after(150), 390, tl.dump())

    def test_f5b_hot_water_draw_while_the_clock_is_frozen_is_not_counted(self):
        # continuous from 100 with a draw in it; the 100 s kept stay 100 s: resume tick 210, end at tick 390
        ev = [(100, "mode", 1), (110, "dhw", 1), (170, "dhw", 0), (200, "mode", 2)]
        tl = self.sim(ev, 700)
        self.assertEqual(tl.first_off_after(210), 390, tl.dump())

    def test_f5c_pwm_left_during_a_draw_keeps_the_time_used_before_the_draw(self):
        # the draw starts at the tick 180 (180 s used), PWM is left at 209 inside the draw: the kept time is 180 s, not 209 s
        ev = [(170, "dhw", 1), (209, "mode", 1), (215, "dhw", 0), (216, "mode", 2)]
        tl = self.sim(ev, 700)
        # resume at the tick 240: anchor 240 - 180 = 60, end at the first tick >= 60 + 264 = 324: tick 330 (209 s kept: tick 300)
        self.assertEqual(tl.first_off_after(240), 330, tl.dump())

    # F6 ----------------------------------------------------------------------------------------------
    def test_f6_frozen_time_above_the_on_time_at_the_resume_ends_the_phase_on_the_first_tick_after_the_resume(self):
        # ratio 0.5: ON time 600 s. PWM runs 300 s, flips, re-entry at 330 with the ON time back at 264 s (ratio 0.22):
        # the resume tick 330 starts the phase with 300 s kept (>= 264), the next tick 360 ends it. The minimum burn counts
        # from the real flame-on (4000 s ago): met. Old code: 330 + 264, tick 600.
        ev = [(300, "mode", 1), (330, "mode", 2), (330, "ratio", 22)]
        tl = self.sim(ev, 800, ratio=0.5)
        self.assertEqual(tl.rows[330][0], 1, tl.dump())
        self.assertEqual(tl.first_off_after(330), 360, tl.dump())

    def test_f6b_the_minimum_burn_anchor_of_a_resumed_phase_is_the_real_flame_on(self):
        # init burning: the real flame-on (_sat_chFlameOnMs) is 4000 s before the start. First phase: its own ON start (tick 0);
        # the resumed phase: the real flame-on, across the flip (entry 479).
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 400)
        self.assertEqual(tl.rows[0][2], 0)
        self.assertEqual(tl.rows[150][2], -4000, f"resumed phase anchor {tl.rows[150][2]} s, expected the real flame-on -4000 s")
        self.assertEqual(tl.rows[150][3], 50, "ON anchor set back by the 100 s kept")

    def test_f6c_heat_pump_resume_counts_the_600_s_minimum_burn_from_the_real_flame_on(self):
        # Heat pump (src 2, entry 197): flame lit at 30 s, flip to continuous at 430 s and back at 460 s, so the real flame-on is
        # 430 s old at the resume. The resumed phase carries the real flame-on as its minimum-burn anchor (entry 479, +30 s here),
        # not the resume tick (460 s), and the phase does not end before that flame has burned 600 s (tick 630).
        ev = [(30, "flame", 1), (430, "mode", 1), (460, "mode", 2)]
        tl = self.sim(ev, 1500, ratio=0.5, init="cold", extra=["src 2"])
        self.assertEqual(tl.rows[480][2], 30, f"resumed phase anchor {tl.rows[480][2]} s, expected the real flame-on +30 s")
        off = tl.first_off_after(460)
        self.assertIsNotNone(off, tl.dump())
        self.assertGreaterEqual(off, 30 + 600, f"ON ended at {off} s, before the 600 s heat pump minimum burn from the real flame-on: {tl.dump()}")

    # F7 ----------------------------------------------------------------------------------------------
    def test_f7_pwm_left_while_waiting_for_ignition_keeps_nothing(self):
        # cold start: the ON phase waits for the flame; PWM is left at 60, the flame lights at 70, re-entry at 100.
        ev = [(60, "mode", 1), (70, "flame", 1), (100, "mode", 2)]
        tl = self.sim(ev, 800, init="cold")
        on = next(s for s in tl.ticks() if s >= 120 and tl.rows[s][0])
        self.assertEqual(on, 120, tl.dump())
        self.assertEqual(tl.rows[on][4], 0, "nothing kept")
        self.assertEqual(tl.first_off_after(on), on + 270, tl.dump())

    # F8 ----------------------------------------------------------------------------------------------
    def test_f8_end_of_cycle_verdict_at_flame_off_keeps_nothing_for_the_next_flame(self):
        # the flame goes out at 100 (hook runs, the ON flag is still set), then the end-of-cycle verdict switches to continuous;
        # the next flame at 200, PWM again at 210: a full ON time from the tick 210 (100 s kept would end it at 390 instead of 480)
        ev = [(100, "flame", 0), (100, "eoc", 1), (200, "flame", 1), (210, "mode", 2)]
        tl = self.sim(ev, 800)
        self.assertEqual(tl.rows[210][0], 1, tl.dump())
        self.assertEqual(tl.rows[210][4], 0, "nothing kept from the flame that ended")
        self.assertEqual(tl.first_off_after(210), 210 + 270, tl.dump())

    # F9 ----------------------------------------------------------------------------------------------
    def test_f9_flame_lit_with_the_hot_water_bit_still_has_a_minimum_burn_anchor(self):
        # _sat_chFlameOnMs stays 0 (flame lit with the DHW bit set / before the first flame-off was seen): the resumed phase keeps
        # a non-zero anchor (the first segment's own, tick 0), so the minimum-burn test of satPwmSchedule() is not skipped
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 400, extra=("nochon 1",))
        self.assertEqual(tl.rows[0][2], 0)
        self.assertNotEqual(tl.rows[150][2], -999999, "the minimum-burn anchor must not be 0 (the != 0 guard would skip the hold)")
        self.assertEqual(tl.rows[150][2], 0, "falls back to the flame-on of the first segment")
        self.assertEqual(tl.first_off_after(150), 330, tl.dump())

    # heater off / SAT disable / restart ---------------------------------------------------------------
    def test_f11a_heater_off_while_pwm_runs_clears_after_the_mode_switch(self):
        # satPwmDisableOnHeaterOff() switches PWM to continuous (a freeze) and then resets: nothing may survive it
        ev = [(100, "heateroff", 1), (130, "mode", 2)]
        tl = self.sim(ev, 800)
        self.assertEqual(tl.first_off_after(150), 150 + 270, tl.dump())
        self.assertEqual(tl.rows[150][4], 0)

    def test_f11b_heater_off_while_frozen_clears(self):
        ev = [(100, "mode", 1), (110, "heateroff", 1), (130, "mode", 2)]
        tl = self.sim(ev, 800)
        self.assertEqual(tl.first_off_after(150), 150 + 270, tl.dump())

    def test_f11c_sat_disable_clears(self):
        ev = [(100, "disable", 1), (130, "mode", 2)]
        tl = self.sim(ev, 800)
        self.assertEqual(tl.first_off_after(150), 150 + 270, tl.dump())

    # a manual control_mode flip uses the same path ----------------------------------------------------
    def test_f12_a_manual_flip_back_and_forth_freezes_like_the_exits(self):
        # the control_mode command calls satSetControlMode() (SATcontrol.ino ~4697-4707): the "mode" event is that call
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 700)
        self.assertEqual(tl.rows[150][3], 50)   # anchor set back by the 100 s kept: the one code path

    # state that must not leak ------------------------------------------------------------------------
    @unittest.skipUnless(HAS_FREEZE, "kept-time state does not exist before TASK-3039")
    def test_the_kept_flag_is_consumed_by_the_resume_and_set_by_the_freeze(self):
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 400)
        self.assertEqual(tl.rows[0][4], 0)
        self.assertEqual(tl.rows[150][4], 0, "consumed at the ON start: the used time lives in the anchor")


class SatPwmOnFreezeSource(unittest.TestCase):
    """Structural pins for the call sites outside the compiled PWM cut."""

    def body(self, name_regex):
        m = re.search(name_regex, SRC.replace("\r\n", "\n"), re.S | re.M)
        self.assertIsNotNone(m, name_regex)
        return m.group(0)

    def test_satpwmreset_does_not_clear_the_kept_time(self):
        body = self.body(r"^void satPwmReset\(\)\n\{.*?^\}")
        self.assertNotIn("Kept", body, "satPwmReset() runs on every mode change: it must not clear the kept ON time")

    def test_the_flame_decision_is_the_live_flame(self):
        body = self.body(r"^static void satPwmKeptFreeze\(\)\n\{.*?^\}")
        self.assertIn("satIsFlameOn()", body)
        self.assertNotIn("_pwm_flameOnMs", body.split("satIsFlameOn()")[0])

    def test_disable_clears_after_reset(self):
        body = self.body(r"satSetControlMode\(SAT_MODE_OFF\);.*?satPwmKeptClear\(\);")
        self.assertLess(body.index("satSetControlMode(SAT_MODE_OFF)"), body.index("satPwmReset()"))
        self.assertLess(body.index("satPwmReset()"), body.index("satPwmKeptClear()"))

    def test_heater_off_clears_after_the_switch_and_the_reset(self):
        body = self.body(r"^static void satPwmDisableOnHeaterOff\(\)\n\{.*?^\}")
        self.assertLess(body.index("satSetControlMode(SAT_MODE_CONTINUOUS)"), body.index("satPwmKeptClear()"))
        self.assertLess(body.index("satPwmReset()"), body.index("satPwmKeptClear()"))

    def test_the_flame_edge_handler_clears_when_the_flame_goes_out(self):
        body = self.body(r"else if \(!currentFlame\)\s*_sat_chFlameOnMs = 0;[^\n]*\n\s*if \(!currentFlame\) satPwmKeptClear\(\);")
        self.assertIn("satPwmKeptClear()", body)

    def test_end_of_on_phase_clears(self):
        body = self.body(r"^static void satPwmEndOn\(.*?^\}")
        self.assertIn("satPwmKeptClear()", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
