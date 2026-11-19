#!/usr/bin/env python3
"""TASK-1887: the hot/cold tolerance band on the SAT heating demand (Sergeant D entries 326, 328, 361, 376, 378, 384; ADR-250).

Entry 328 (Home Assistant generic thermostat): heating stops when the room is at or above target + 0.1 and starts again below
target - 0.1; inside the band the previous state holds. Entry 361: the band decides heating on/off only, the PID keeps running,
the cold cutoff (ADR-199) is unchanged. Entry 384 (Q-377a, "yes, B"): a band stop sends CH=0 and PWM stays active (no CS=10, no
PWM end). Entry 376: a burn first runs at least 180 s (in PWM the entry-275 rule, applied to the band as our reading). Entry 378:
ECO = only the main room decides, COMFORT = any usable room keeps heating on.

Nothing is re-implemented:
- BandHelper compiles the real SATchEnable.h (satToleranceMaxError, satToleranceDemand, satChEnableDecide).
- BandLoop compiles the real control-tick segment of satControlLoopRun() (the cut of tests/test_sat_cold_cut_min_burn.py: the
  PWM block, the cold-cut state, the tick from the solar offset to the CH command). The driver sets the band input _sat_tolErr
  (target - room of the deciding rooms) the way the zone block does before that segment; latch, minimum burn and CH bit are the
  firmware's. Not cut in: the computation of _sat_tolErr itself (source check below and BandHelper).

OTGW_SAT_INO points at another SATcontrol.ino (e.g. origin/dev) for old-vs-fix runs: the code before TASK-1887 has no band, so a
warm room keeps CH=1 and the loop tests fail there.
Run: python tests/test_sat_tolerance_band.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402
import test_sat_cold_cut_min_burn as ccmb  # noqa: E402

CHENABLE = base.REPO / "src" / "OTGW-firmware" / "SATchEnable.h"

HELPER_MAIN = r"""
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "@CHENABLE@"
int main(int argc, char** argv) {
  // demand <wasOn> <err|nan>  |  maxerr <eco> <roomValid> <roomErr> <n> (<zoneErr> <zoneUse>)*  |  ch <cold> <band>
  if (!strcmp(argv[1], "demand")) {
    float e = !strcmp(argv[3], "nan") ? NAN : (float)atof(argv[3]);
    printf("%d\n", (int)satToleranceDemand(atoi(argv[2]) != 0, e));
  } else if (!strcmp(argv[1], "ch")) {   // requested 45 vs cold 28.2 (or 20 = cold cut), PWM not driving, band on/off
    printf("%d\n", (int)satChEnableDecide(atoi(argv[2]) ? 20.0f : 45.0f, 28.2f, false, false, atoi(argv[3]) != 0));
  } else {
    float ze[4] = {0}; bool zu[4] = {false}; int n = atoi(argv[5]);
    for (int i = 0; i < n; i++) { ze[i] = (float)atof(argv[6 + 2 * i]); zu[i] = atoi(argv[7 + 2 * i]) != 0; }
    float m = satToleranceMaxError(atoi(argv[3]) != 0, (float)atof(argv[4]), ze, zu, (uint8_t)n, atoi(argv[2]) != 0);
    if (isnan(m)) printf("nan\n"); else printf("%.3f\n", m);
  }
  return 0;
}
"""

LOOP_MAIN = r"""
#include <string>
#include <vector>
#include <iostream>
struct Ev { int sec; std::string kind; float val; };
int main() {
  std::vector<Ev> evs; std::string cmd; int dur = 0;
  while (std::cin >> cmd) {
    if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
  }
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  const uint32_t base = 100000000UL; g_ms = base;
  g_flameOnStart = base - 1500000UL; g_flameOffStart = base - 1400000UL;
  satPwmReset();
  _pwm_effectiveBoilerTemp = 50.0f; _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  state.sat.eControlMode = SAT_MODE_CONTINUOUS;
  float pid = 45.0f;
  for (int sec = 0; sec <= dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;
        satPwmNoteDhwFlameEdge(g_flame, state.sat.bDhwActive);
      } else if (e.kind == "pid") pid = e.val;
      else if (e.kind == "flow") g_flow = e.val;   // the MsgID 25 flow the switch rules read (satGetFlowTemp())
      else if (e.kind == "pwm") { state.sat.eControlMode = SAT_MODE_PWM; _pwm_entryPending = true; _pwm_lastUpdateMs = g_ms; }
#if HAS_TOL
      else if (e.kind == "err") _sat_tolErr = e.val;
      else if (e.kind == "errnan") _sat_tolErr = NAN;
#endif
    }
    if (sec % 30 != 0) continue;
    _sat_tickMs = g_ms; g_sentCh = -1; g_sentCs = -1;
    state.sat.fPidOutput = pid;
    fullTick(pid);
    printf("T %d ch=%d cs=%.1f mode=%d pid=%.3f on=%d starts=%d\n", sec, g_sentCh, g_sentCs, state.sat.eControlMode,
           state.sat.fPidOutput, (int)state.sat.bPwmFlameRequested, (int)g_pwmStarts);
  }
  return 0;
}
"""


def build_helper(workdir: Path):
    cpp = workdir / "band.cpp"
    cpp.write_text("#include <stdlib.h>\n#include <stdint.h>\n" + HELPER_MAIN.replace("@CHENABLE@", CHENABLE.as_posix()),
                   encoding="utf-8")
    return base.compile_cpp(cpp, workdir / "band.exe")


def build_loop(workdir: Path):
    src = base.SAT_INO.read_text(encoding="utf-8")
    body = base.extract(src)
    pre = ""
    for name, stub in (("satCurveTuneNoteNotControlling", "static void satCurveTuneNoteNotControlling(){ }"),):
        if name not in body:
            pre += stub + "\n"
    cycles = "static uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }\n" + base.extract_cycles()
    flag = "#define HAS_REQSP 1\n" if "_sat_requestedSetpoint" in body else ""
    flag += "#define HAS_TOL %d\n" % (1 if "_sat_tolErr" in body else 0)
    stubs = (base.STUBS.replace("@FORCEFIELD@", "").replace("@AUTOFIELD@", "")
             .replace("@CHENABLE@", CHENABLE.as_posix()))
    cpp = workdir / "loop.cpp"
    cpp.write_text(flag + stubs + ccmb.TICK_STUBS + pre + body + cycles + ccmb.extract_full_tick(src) + LOOP_MAIN,
                   encoding="utf-8")
    return base.compile_cpp(cpp, workdir / "loop.exe")


class BandHelper(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build_helper(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_exe(self, *args):
        return subprocess.run([str(self.exe)] + [str(a) for a in args], capture_output=True, text=True, check=True).stdout.strip()

    def demand(self, was_on, err):
        return self.run_exe("demand", int(was_on), err) == "1"

    def test_starts_as_soon_as_the_room_is_below_target_plus_0_1(self):
        # entry 401: both tolerances mean 0.1 above the target; error = target - room (rounded like satPidError)
        self.assertFalse(self.demand(False, -0.1))    # room exactly target + 0.1: no start
        self.assertTrue(self.demand(False, -0.099))
        self.assertTrue(self.demand(False, 0.0))      # at the target: demand (before entry 401 it waited for target - 0.1)
        self.assertFalse(self.demand(False, -0.5))

    def test_stops_at_or_above_target_plus_0_1(self):
        # entry 328: "stop when the sensor equals or goes above target + tolerance"
        self.assertFalse(self.demand(True, -0.1))     # room exactly target + 0.1: stop
        self.assertTrue(self.demand(True, -0.099))
        self.assertTrue(self.demand(True, 0.0))

    def test_no_hold_zone_the_minimum_off_time_does_the_job(self):
        # entries 401/402: stop and restart share target + 0.1; the 600 s minimum CH-off (entry 403) replaces the gap
        for err in (0.1, 0.05, 0.0, -0.05, -0.099):
            self.assertTrue(self.demand(True, err), err)
            self.assertTrue(self.demand(False, err), err)

    def test_no_usable_room_never_blocks_heating(self):
        self.assertTrue(self.demand(False, "nan"))
        self.assertTrue(self.demand(True, "nan"))

    def test_comfort_takes_the_largest_error_of_the_usable_rooms(self):
        self.assertEqual(self.run_exe("maxerr", 0, 1, -0.3, 0), "-0.300")
        self.assertEqual(self.run_exe("maxerr", 0, 1, -0.3, 2, 0.4, 1, 0.9, 0), "0.400")   # a cold usable zone wins, an unusable one not
        self.assertEqual(self.run_exe("maxerr", 0, 0, 0.0, 1, -0.2, 1), "-0.200")           # no main room: the zones decide
        self.assertEqual(self.run_exe("maxerr", 0, 0, 0.0, 1, 0.5, 0), "nan")               # nothing usable

    def test_eco_lets_only_the_main_room_decide(self):   # entry 378
        self.assertEqual(self.run_exe("maxerr", 1, 1, -0.3, 2, 0.4, 1, 0.9, 1), "-0.300")   # cold zones do not keep heating on
        self.assertEqual(self.run_exe("maxerr", 1, 0, 0.0, 1, 0.5, 1), "nan")               # no main room: the band does not block

    def test_band_off_clears_only_the_ch_bit(self):
        self.assertEqual(self.run_exe("ch", 0, 0), "1")
        self.assertEqual(self.run_exe("ch", 0, 1), "0")
        self.assertEqual(self.run_exe("ch", 1, 0), "0")   # the cold cutoff as before


class BandLoop(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build_loop(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_sc(self, events, dur=330):
        script = "".join("ev %d %s %s\n" % e for e in events) + "run %d\n" % dur
        out = subprocess.run([str(self.exe)], input=script, capture_output=True, text=True, check=True).stdout
        rows = {}
        for line in out.splitlines():
            if line.startswith("T "):
                parts = line.split()
                rows[int(parts[1])] = dict(kv.split("=") for kv in parts[2:])
        return rows

    def test_warm_room_sends_ch0_and_keeps_the_real_setpoint(self):   # entry 384: no CS=10
        rows = self.run_sc([(0, "err", 0.5), (65, "err", -0.2)])
        self.assertEqual(rows[60]["ch"], "1", rows[60])
        for sec in (90, 120):
            self.assertEqual(rows[sec]["ch"], "0", (sec, rows[sec]))
            self.assertGreater(float(rows[sec]["cs"]), 10.0, (sec, rows[sec]))      # the setpoint keeps running
            self.assertEqual(float(rows[sec]["pid"]), 45.0, (sec, rows[sec]))        # the PID request is untouched (entry 361)

    # A real burn first (CH=1 from tick 0), then the band stop at tick 60 turns CH off: the entry-403 minimum runs to 660.
    BURN_THEN_STOP = [(0, "err", 0.5), (35, "err", -0.2)]

    def test_after_a_band_stop_heating_stays_off_for_the_minimum(self):   # entries 402/403
        rows = self.run_sc(self.BURN_THEN_STOP + [(95, "err", 0.05), (155, "err", 0.1)], dur=700)
        self.assertEqual(rows[30]["ch"], "1", rows[30])
        for sec in (60, 90, 150, 630):
            self.assertEqual(rows[sec]["ch"], "0", (sec, rows[sec]))
        self.assertEqual(rows[660]["ch"], "1", rows[660])

    # Entries 410 (Q-1887c B) and 417 (Q-410a A): both continuous -> PWM rules only run while heating is allowed (thermo-nova heating_control.py:276-278).
    # Replay of indigo_light 2026-11-16 16:00:58: continuous, a band pause (CH off), flame off, residual hot-water heat brings the flow
    # to 67.3 C. Old code: the 3.5 C fail-safe (entries 288/289) switches to PWM 30 s later, inside the pause. Mode 1 = continuous, 2 = PWM.
    INDIGO_PAUSE = [(0, "flow", 40.0), (0, "err", 0.5), (35, "err", -0.2), (65, "flow", 67.3)]

    def test_no_switch_to_pwm_during_a_band_pause_on_residual_heat(self):   # entry 410, indigo 16:00:58
        rows = self.run_sc(self.INDIGO_PAUSE, dur=630)
        self.assertEqual(rows[60]["ch"], "0", rows[60])
        for sec in range(60, 631, 30):
            self.assertEqual(rows[sec]["mode"], "1", (sec, rows[sec]))

    def test_margin_rule_is_skipped_during_a_band_pause_too(self):   # entry 410: flow 48 = request + 3 (margin 2, below the 3.5 fail-safe)
        rows = self.run_sc([(0, "flow", 40.0), (0, "err", 0.5), (35, "err", -0.2), (65, "flow", 48.0)], dur=630)
        for sec in range(60, 631, 30):
            self.assertEqual(rows[sec]["mode"], "1", (sec, rows[sec]))

    def test_switch_runs_again_once_heating_is_allowed(self):   # entry 410: only the pause is gated, not the rule
        rows = self.run_sc(self.INDIGO_PAUSE + [(640, "err", 0.2)], dur=750)
        self.assertEqual(rows[660]["ch"], "1", rows[660])
        self.assertEqual(rows[660]["mode"], "1", rows[660])   # the start counts from the first tick with heating allowed
        self.assertEqual(rows[690]["mode"], "2", rows[690])   # fail-safe: 30 s later

    def test_a_stop_while_ch_is_already_off_starts_no_minimum(self):   # entry 403: nothing to turn off (boot, cold cutoff)
        rows = self.run_sc([(0, "err", -0.2), (65, "err", 0.2)])
        self.assertEqual(rows[60]["ch"], "0", rows[60])
        self.assertEqual(rows[90]["ch"], "1", rows[90])

    def narr(self, script):
        err = subprocess.run([str(self.exe)], input=script, capture_output=True, text=True, check=True).stderr
        return [l[2:] for l in err.splitlines() if l.startswith("N ")]

    def test_narration_names_the_value_the_band_decided_on(self):   # 09's bench 2026-11-16: the log alone must show it
        narr = self.narr("ev 0 err 0.5\nev 65 err -0.2\nev 125 err 0.25\nrun 720\n")
        pause = [l for l in narr if l.startswith("Heating paused")]
        hold = [l for l in narr if l.startswith("Heating pause minimum")]
        resume = [l for l in narr if l.startswith("Heating resumes")]
        self.assertTrue(pause and "at least 0.20 above their target (band 0.1)" in pause[0], narr)
        self.assertTrue(hold and hold[0].startswith("Heating pause minimum 600 s: a room is below target + 0.1"), narr)
        self.assertTrue(resume and "target - room 0.25, below target + 0.1; CH was off 600 s" in resume[0], narr)
        self.assertEqual((len(pause), len(hold), len(resume)), (1, 1, 1), narr)

    def test_restart_below_target_plus_0_1_after_600s_off(self):   # entries 401, 403
        rows = self.run_sc(self.BURN_THEN_STOP + [(95, "err", 0.0)], dur=720)   # room back at the target, CH off since tick 60
        for sec in (60, 330, 630):
            self.assertEqual(rows[sec]["ch"], "0", (sec, rows[sec]))
        self.assertEqual(rows[660]["ch"], "1", rows[660])

    def test_room_far_below_target_still_waits_for_the_minimum(self):   # entry 403: the minimum holds whatever the room does
        rows = self.run_sc(self.BURN_THEN_STOP + [(95, "err", 0.5)], dur=720)
        self.assertEqual({rows[s]["ch"] for s in sorted(rows) if 60 <= s < 660}, {"0"})
        self.assertEqual(rows[660]["ch"], "1", rows[660])
        hold = [l for l in self.narr("ev 0 err 0.5\nev 35 err -0.2\nev 95 err 0.5\nrun 720\n") if l.startswith("Heating pause minimum")]
        self.assertTrue(hold and "CH stays off 540 s more" in hold[0], hold)   # demand back at tick 120, CH off since tick 60

    def test_minimum_counts_from_the_ch_off_not_from_the_stop_decision(self):   # entry 403: after any 180 s burn
        # request 55 above the flow 50 keeps the boiler continuous; flame on at 40, stop decided at 65, CH off at 240
        rows = self.run_sc([(0, "pid", 55), (0, "err", 0.5), (40, "flame", 1), (65, "err", -0.2), (270, "err", 0.2)], dur=900)
        self.assertEqual(rows[210]["ch"], "1", rows[210])
        self.assertEqual(rows[240]["ch"], "0", rows[240])
        self.assertEqual(rows[810]["ch"], "0", rows[810])   # 570 s after the CH-off
        self.assertEqual(rows[840]["ch"], "1", rows[840])   # 600 s after the CH-off

    def test_a_room_hovering_at_target_plus_0_1_restarts_at_most_once_per_600s(self):   # entries 401-403
        ev = [(0, "err", 0.5)] + [(s, "err", -0.12 if (s // 30) % 2 else -0.08) for s in range(35, 3600, 30)]
        rows = self.run_sc(ev, dur=3600)
        secs = sorted(rows)
        ons = [s for p, s in zip(secs, secs[1:]) if rows[p]["ch"] == "0" and rows[s]["ch"] == "1"]
        offs = [s for p, s in zip(secs, secs[1:]) if rows[p]["ch"] == "1" and rows[s]["ch"] == "0"]
        self.assertGreaterEqual(len(offs), 2, (offs, ons))   # the band does stop and restart
        for off, on in zip(offs, ons):
            self.assertGreaterEqual(on - off, 600, (off, on))

    def test_no_room_temperature_never_blocks_heating(self):   # entries 38/46: also not during the entry-403 minimum (ours)
        rows = self.run_sc([(0, "err", -0.2), (65, "errnan", 0)])
        self.assertEqual(rows[60]["ch"], "0", rows[60])
        self.assertEqual(rows[90]["ch"], "1", rows[90])

    def test_band_off_keeps_pwm_active_and_its_schedule_running(self):   # entry 384
        rows = self.run_sc([(0, "err", 0.5), (5, "pwm", 1), (35, "err", -0.2)], dur=1500)
        band = [s for s in sorted(rows) if s >= 60]
        self.assertTrue(all(rows[s]["mode"] == "2" for s in band), [rows[s] for s in band][:3])   # PWM never ends
        self.assertTrue(all(rows[s]["ch"] == "0" for s in band), [(s, rows[s]["ch"]) for s in band][:5])
        self.assertEqual({rows[s]["on"] for s in band}, {"0", "1"})                                # the ON/OFF schedule keeps toggling

    # --- entry 398: ON phases that start during a band stop leave the hourly start count ---
    PAUSE = [(0, "err", 0.5), (5, "pwm", 1), (35, "err", -0.2)]

    @staticmethod
    def on_edges(rows, after):
        secs = sorted(rows)
        return [s for p, s in zip(secs, secs[1:]) if s > after and rows[p]["on"] == "0" and rows[s]["on"] == "1"]

    def test_flameless_on_phases_during_a_stop_are_not_counted(self):   # entry 398
        rows = self.run_sc(self.PAUSE, dur=1500)
        edges = self.on_edges(rows, 60)
        self.assertTrue(edges, "no new ON phase during the stop")
        self.assertEqual({rows[s]["starts"] for s in sorted(rows) if s >= 60}, {rows[60]["starts"]}, edges)

    def test_on_phase_from_a_stop_counts_once_when_ch_is_allowed_again(self):   # entry 398 + our reading (TASK-1887 notes)
        s0 = self.on_edges(self.run_sc(self.PAUSE, dur=1500), 690)[0]   # after the entry-403 minimum (CH off at 60)
        rows = self.run_sc(self.PAUSE + [(s0 + 15, "err", 0.2)], dur=s0 + 120)
        self.assertEqual(rows[s0]["ch"], "0", rows[s0])
        self.assertEqual(rows[s0 + 30]["on"], "1", rows[s0 + 30])        # still the same ON phase
        self.assertEqual(rows[s0 + 30]["ch"], "1", rows[s0 + 30])
        n = int(rows[s0]["starts"])
        self.assertEqual(int(rows[s0 + 30]["starts"]), n + 1, (rows[s0], rows[s0 + 30]))
        same = [s for s in sorted(rows) if s > s0 and all(rows[t]["on"] == "1" for t in sorted(rows) if s0 <= t <= s)]
        self.assertEqual({rows[s]["starts"] for s in same}, {str(n + 1)}, same)   # once, not on every tick

    def test_on_phase_that_starts_in_the_resume_tick_counts_once(self):   # the band block runs before satPwmSchedule()
        s0 = self.on_edges(self.run_sc(self.PAUSE, dur=1500), 690)[0]   # after the entry-403 minimum (CH off at 60)
        rows = self.run_sc(self.PAUSE + [(s0, "err", 0.2)], dur=s0 + 30)
        self.assertEqual(rows[s0]["on"], "1", rows[s0])
        self.assertEqual(int(rows[s0]["starts"]), int(rows[s0 - 30]["starts"]) + 1, (rows[s0 - 30], rows[s0]))

    def test_outside_a_stop_every_on_phase_counts(self):   # entries 59, 90, 95 unchanged
        rows = self.run_sc([(0, "err", 0.5), (5, "pwm", 1)], dur=1500)
        edges = self.on_edges(rows, 0)
        self.assertTrue(edges)
        for s in edges:
            self.assertEqual(int(rows[s]["starts"]), int(rows[s - 30]["starts"]) + 1, (s, rows[s - 30], rows[s]))

    def test_lit_pwm_phase_burns_180s_before_the_band_stops_it(self):   # entries 275 (PWM) and 376
        rows = self.run_sc([(0, "err", 0.5), (5, "pwm", 1), (40, "flame", 1), (85, "err", -0.2)], dur=300)
        for sec in (90, 120, 150, 180, 210):
            self.assertEqual(rows[sec]["ch"], "1", (sec, rows[sec]))
        self.assertEqual((rows[240]["ch"], rows[240]["mode"]), ("0", "2"), rows[240])

    def test_continuous_lit_burn_runs_180s_before_the_band_stops_it(self):   # entry 376
        # request 55 above the flow 50, so the overshoot switch cannot move the boiler into PWM
        rows = self.run_sc([(0, "pid", 55), (0, "err", 0.5), (40, "flame", 1), (65, "err", -0.2)], dur=300)
        for sec in (90, 120, 150, 180, 210):
            self.assertEqual((rows[sec]["ch"], rows[sec]["mode"]), ("1", "1"), (sec, rows[sec]))   # flame on at 40 s: until 220 s
        self.assertEqual(rows[240]["ch"], "0", rows[240])

    def test_cold_cutoff_unchanged_while_the_band_wants_heat(self):
        rows = self.run_sc([(0, "err", 0.5), (65, "pid", 20)])
        self.assertEqual((rows[90]["ch"], float(rows[90]["cs"])), ("0", 10.0), rows[90])   # requested 20 <= 28.2: cold cutoff as before


class BandSource(unittest.TestCase):
    """What the loop cut does not run."""

    def setUp(self):
        self.src = base.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        if "_sat_tolErr" not in self.src:
            self.fail("SATcontrol.ino has no tolerance band (code before TASK-1887)")

    def test_band_input_uses_the_pid_room_target_zones_and_the_eco_switch(self):
        loop = self.src[self.src.index("void satControlLoopRun"):]
        i = loop.index("_sat_tolErr = satToleranceMaxError(")
        stmt = loop[i:loop.index(";", i)]
        for token in ("roomValid", "satPidError(effectiveTarget, roomTemp)", "zoneErr", "zoneUse", "zoneCount",
                      "settings.sat.iHeatingMode == 1"):
            self.assertIn(token, stmt)
        self.assertLess(loop.index("roomTemp = satRoomFiltered(roomTemp);"), i)
        self.assertLess(i, loop.index("_sat_tolDemand = satToleranceDemand("))

    def test_only_the_main_ch_command_reads_the_band(self):   # CS, PWM and the cold cutoff are untouched
        self.assertIn("satChEnableDecide(pidOutput, satColdThreshold(), pwmDrivesSetpoint, pwmOn, _sat_tolBandOff)", self.src)
        a = self.src.index("static float satColdThreshold()")
        self.assertNotIn("_sat_tol", self.src[a:self.src.index("\n}\n", a)])

    def test_back_from_satdisable_the_band_starts_with_demand_on(self):
        loop = self.src[self.src.index("void satControlLoopRun"):]
        edge = loop.index("if (state.sat.eControlMode == SAT_MODE_OFF) {")
        block = loop[edge:loop.index("}", edge)]
        self.assertIn("_sat_tolDemand = true;", block)
        self.assertIn("static bool  _sat_tolDemand = true;", self.src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
