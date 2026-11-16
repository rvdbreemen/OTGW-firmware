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
    printf("T %d ch=%d cs=%.1f mode=%d pid=%.3f on=%d\n", sec, g_sentCh, g_sentCs, state.sat.eControlMode, state.sat.fPidOutput,
           (int)state.sat.bPwmFlameRequested);
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

    def test_starts_only_strictly_below_target_minus_0_1(self):
        # entry 328: "start when the sensor goes below target - tolerance"; error = target - room (rounded like satPidError)
        self.assertFalse(self.demand(False, 0.1))     # room exactly target - 0.1: no start
        self.assertTrue(self.demand(False, 0.101))
        self.assertFalse(self.demand(False, 0.0))
        self.assertFalse(self.demand(False, -0.5))

    def test_stops_at_or_above_target_plus_0_1(self):
        # entry 328: "stop when the sensor equals or goes above target + tolerance"
        self.assertFalse(self.demand(True, -0.1))     # room exactly target + 0.1: stop
        self.assertTrue(self.demand(True, -0.099))
        self.assertTrue(self.demand(True, 0.0))

    def test_inside_the_band_the_previous_state_holds(self):
        for err in (0.1, 0.05, 0.0, -0.05, -0.099):
            self.assertTrue(self.demand(True, err), err)
            self.assertFalse(self.demand(False, err), err)

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

    def test_inside_the_band_after_a_stop_heating_stays_off(self):
        rows = self.run_sc([(0, "err", -0.2), (65, "err", 0.05), (125, "err", 0.1)])
        for sec in (30, 90, 150):
            self.assertEqual(rows[sec]["ch"], "0", (sec, rows[sec]))

    def test_room_below_target_minus_0_1_resumes(self):
        rows = self.run_sc([(0, "err", -0.2), (65, "err", 0.2)])
        self.assertEqual(rows[60]["ch"], "0", rows[60])
        self.assertEqual(rows[90]["ch"], "1", rows[90])

    def test_no_room_temperature_never_blocks_heating(self):
        rows = self.run_sc([(0, "err", -0.2), (65, "errnan", 0)])
        self.assertEqual(rows[60]["ch"], "0", rows[60])
        self.assertEqual(rows[90]["ch"], "1", rows[90])

    def test_band_off_keeps_pwm_active_and_its_schedule_running(self):   # entry 384
        rows = self.run_sc([(0, "err", 0.5), (5, "pwm", 1), (35, "err", -0.2)], dur=1500)
        band = [s for s in sorted(rows) if s >= 60]
        self.assertTrue(all(rows[s]["mode"] == "2" for s in band), [rows[s] for s in band][:3])   # PWM never ends
        self.assertTrue(all(rows[s]["ch"] == "0" for s in band), [(s, rows[s]["ch"]) for s in band][:5])
        self.assertEqual({rows[s]["on"] for s in band}, {"0", "1"})                                # the ON/OFF schedule keeps toggling

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
