#!/usr/bin/env python3
"""TASK-3151.4 (ADR-305 decision 8, entries 519, 520): host test that every PWM entry starts with a FRESH ON time.

Replaces tests/test_sat_pwm_on_freeze.py (TASK-3039 / ADR-296, entries 475, 476, 478, 479), which pinned the opposite rule: the ON time
used before a switch to continuous was kept and the next ON phase of the same flame resumed it. c96212949 removed the kept time. As in
thermo-nova (pwm.reset() on every pwm.disable(), pwm.py:91), leaving PWM for any reason, inside one flame or not, forgets the ON phase:
the next entry opens a full new ON time (cycles per hour 3, duty 0.22: 1200 s period, ON 264 s, the first OFF at the tick after 264 s).

The code under test is NOT re-implemented: the real PWM block of SATcontrol.ino (satPwmBeginOn, satPwmSchedule, satPwmEndOn, satPwmReset,
satSetControlMode, satPwmDisableOnHeaterOff) is compiled through the shared harness of tests/test_sat_hp_pwm.py with this file's MAIN
(an event script: mode flips, heater off). The flame-edge handler and satDisable() sit outside the PWM cut and are not driven here: the
flame-out and disable cases of the old test cleared the kept time through the driver, so they cannot be red on the old code for the right reason. OTGW_SAT_INO points at another copy: on the parent of c96212949 (kept
ON time) the cases below go red, on the lane they are green.
Run: python tests/test_sat_pwm_fresh_on_time.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_hp_pwm as hp  # noqa: E402  (builds the real PWM cut; its MAIN is replaced below)

TICK = 30

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
      else if (e.kind == "disable") { satSetControlMode(SAT_MODE_OFF); satPwmReset();  }   // satDisable(): SATcontrol.ino ~4395-4398
      else if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) { g_flameOnStart = g_ms; if (!nochon && !state.sat.bDhwActive) _sat_chFlameOnMs = g_ms; }
        else { g_flameOffStart = g_ms; _sat_chFlameOnMs = 0;  }                              // flame-edge handler, SATcontrol.ino ~8118-8125
      }
    }
    if (sec % 30 != 0) continue;
    if (state.sat.eControlMode != SAT_MODE_PWM) continue;       // continuous: the PWM schedule does not run
    if (state.sat.bDhwActive) { if (!prevDhw) {  } prevDhw = true; continue; }
    if (prevDhw) {  }
    prevDhw = false;
    const float cs = satApplyPWM(pid);
    _sat_lastControlCS = cs;
    printf("T %d %d %.1f %d %lld %lld %d\n", sec, (int)state.sat.bPwmFlameRequested, cs, (int)g_flame,
           rel(_pwm_flameOnMs, base), rel(_pwm_lastUpdateMs, base), 0);
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


class SatPwmFreshOnTime(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        chvar = "" if "_sat_chFlameOnMs" in hp.base.extract(hp.SAT_INO.read_text(encoding="utf-8")) else "static uint32_t _sat_chFlameOnMs = 0;"
        hp.MAIN = MAIN.replace("@CHVAR@", chvar)
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

    # cycles/hour 3 -> period 1200 s, duty 0.22 -> ON 264 s. The first ON phase starts at tick 0 with the flame burning: first OFF at 270.
    def test_first_entry_is_a_full_on_time(self):
        self.assertEqual(self.sim([], 600).first_off_after(0), 270)

    def test_one_flip_inside_one_flame_gives_a_full_new_on_time(self):
        # PWM 0..100, continuous 100..130, PWM again at 130: the next ON phase starts at the tick 150 and lasts a full 264 s: first OFF at 420
        # (the kept-time rule ended it at 330 = 150 + 264 - 100 + 16 s of ticks)
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 700)
        self.assertEqual(tl.first_off_after(150), 420, tl.dump())

    def test_three_flips_each_reentry_starts_at_zero(self):
        ev = [(100, "mode", 1), (130, "mode", 2), (200, "mode", 1), (230, "mode", 2), (270, "mode", 1), (300, "mode", 2)]
        tl = self.sim(ev, 900)
        self.assertEqual(tl.first_off_after(300), 570, tl.dump())   # the last re-entry at 300 opens a full ON time (kept-time rule: 390)

    def test_an_on_phase_ended_by_its_timer_starts_the_next_one_at_zero(self):
        tl = self.sim([(100, "mode", 1), (130, "mode", 2)], 2600)
        off = tl.first_off_after(150)                       # 420
        nxt = next(s for s in tl.ticks() if s > off and tl.rows[s][0])
        self.assertEqual(tl.first_off_after(nxt), nxt + 270, tl.dump())

    # Invariant, not red on the parent: the real satPwmDisableOnHeaterOff() also cleared the kept time there.
    def test_heater_off_then_back_on_starts_a_full_on_time(self):
        ev = [(100, "heateroff", 0), (130, "mode", 2)]
        tl = self.sim(ev, 700)
        self.assertEqual(tl.first_off_after(150), 420, tl.dump())


if __name__ == "__main__":
    unittest.main()
