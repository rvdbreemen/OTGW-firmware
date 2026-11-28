#!/usr/bin/env python3
"""TASK-3151.4 (ADR-305 decision 8, entry 519): the COLD CUTOFF acts at once; the tolerance BAND still waits for the 180 s minimum burn.

History: TASK-1652 (maintainer decision 2026-11-05, entry 275) let a latched cold cutoff wait until the lit PWM ON phase had burned 180 s
(satPwmColdCutWaitsForMinBurn() read by the cutoff). thermo-nova has no such wait: without heating demand PWM is disabled on that tick
(heating_control.py:229-234, pwm.disable()). ADR-305 decision 8 removes the wait for the cutoff ("the cold cutoff acts at once"); the
helper stays for the tolerance band only (ADR-250, ADR-253; entries 275 for the band, 376): a band stop clears CH after the running CH
burn has lasted 180 s, in PWM from the ignition anchor of the ON phase (satBandWaitsForMinBurn()), and leaves CS and PWM alone.

The code under test is NOT re-implemented: the real PWM block (shared cut of tests/test_sat_pwm_override.py), the real cold-cutoff state and
threshold (extract_cold_cut()) and the real control-tick segment of satControlLoopRun() from the solar offset to the CH command (fullTick(),
band included) are compiled with a host C++ compiler. satSendCS/satSendMM/satSendCH record what the tick sends. The DHW early return is
driven as the control loop does since D4 (cbec269e9): the tick segment is skipped while the DHW bit is set, but a PWM mode first runs the
real satPwmSchedule() with the last request. Not cut in: the other upstream early returns of satControlLoopRun() (fault skip, valves,
summer, safety); the cycle tracker's flame edge (the driver sets g_flameOnStart as tests/test_sat_pwm_anchors.py does). The band input
_sat_tolErr (target - room) is set by the driver (event `tol`, in hundredths of a degree). Only the host test touches firmware code.

The mutants edit the SATcontrol.ino text in memory: (1) the cutoff waits for the min burn again (the TASK-1652 rule), (2) the band no
longer waits (its stop acts at once). Each must turn the case that pins the rule red.

OTGW_SAT_INO points at another copy for old-vs-fix runs. Run: python tests/test_sat_cold_cut_min_burn.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402

TICK_STUBS = r"""
static int g_sentCh = -1; static float g_sentCs = -1; static int g_sentMm = -1;
static void satSendCS(float cs) { g_sentCs = cs; }
static void satSendMM(int mm) { g_sentMm = mm; }
static void satSendCH(int ch) { g_sentCh = ch; state.sat.iLastSentCH = (uint8_t)ch; }   // as the real satSendCH() records it
enum { SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2, SAT_SRC_HYBRID = 3 };
static uint8_t satGetEffectiveHeatingSource() { return SAT_SRC_GAS_BOILER; }
static uint8_t satGetManufacturerQuirks() { return 0; }
#ifndef SAT_MM_T
#define SAT_MM_T 255
#endif
static uint8_t satReportedModulation(bool atMax, uint8_t mm) { return atMax ? 100 : mm; }
#define strlcpy otgw_test_strlcpy
static size_t strlcpy(char* d, const char* s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
#define snprintf_P snprintf
"""

MAIN = r"""
#include <string>
#include <vector>
#include <iostream>
struct Ev { int sec; std::string kind; int val; };
int main() {
  std::vector<Ev> evs; std::string cmd; int dur = 0;
  while (std::cin >> cmd) {
    if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
  }
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  const uint32_t base = 100000000UL; g_ms = base;
  g_flameOnStart = base - 1500000UL; g_flameOffStart = base - 1400000UL;   // last burn long ago
  satPwmReset();
  _pwm_effectiveBoilerTemp = 50.0f; _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  state.sat.eControlMode = SAT_MODE_PWM;
  _pwm_entryPending = true; _pwm_lastUpdateMs = base;     // PWM just entered: the first tick starts ON (entry 43)
  _sat_tolErr = 0.5f;                                      // the room is below target: the band wants heat
  float pid = 35.0f;                                       // duty (35-27.2)/(50-27.2) = 0.34: range 3, on 308 s
  bool prevDhw = false;
  for (int sec = 0; sec <= dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;   // what satCycleOnFlameChange() records
      } else if (e.kind == "pid") pid = (float)e.val;
      else if (e.kind == "tol") _sat_tolErr = (float)e.val / 100.0f;   // the band input: target - room
      else if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
    }
    if (sec % 30 != 0) continue;                          // control tick
    if (state.sat.bDhwActive) {                           // hot water: satControlLoopRun() returns before the tick segment, but first
      // runs the PWM schedule with the request of the last tick (TASK-3151.4, D4); nothing is sent from there
      if (state.sat.eControlMode == SAT_MODE_PWM && pid > satColdThreshold()) satPwmSchedule(pid);
      printf("T %d ch=- cs=- mode=%d on=%d flame=%d\n", sec, state.sat.eControlMode, (int)state.sat.bPwmFlameRequested, (int)g_flame);
      continue;
    }
    _sat_tickMs = g_ms; g_sentCh = -1; g_sentCs = -1;
    state.sat.fPidOutput = pid;
    fullTick(pid);
    printf("T %d ch=%d cs=%.1f mode=%d on=%d flame=%d\n", sec, g_sentCh, g_sentCs, state.sat.eControlMode,
           (int)state.sat.bPwmFlameRequested, (int)g_flame);
  }
  return 0;
}
"""


def extract_full_tick(src: str) -> str:
    """The real segment of satControlLoopRun() from the solar offset to the CH command, compiled as fullTick()."""
    src = src.replace("\r\n", "\n")
    loop = src[src.index("void satControlLoopRun"):]
    a = loop.find("  // --- Solar gain compensation on the requested setpoint")
    if a < 0:
        a = loop.index("  // --- Clamp to valid range ---")
    ch = loop.index("satSendCH(satChEnableDecide(", a)
    end = loop.index("\n", ch) + 1
    seg = loop[a:end]
    # TASK-1882: the diagnostic block reads tick locals from before this segment and sends nothing; it has its own
    # test (tests/test_sat_diag_events.py), so it is left out of this compiled segment
    seg = re.sub(r"\n  // TASK-1882: diagnostic events from what this tick decided.*?satDiagEventsStep\(d\);\n  \}\n", "\n", seg,
                 flags=re.S)
    return "\nstatic float fullTick(float pidOutput) {\n" + seg + "  }\n  return state.sat.fFinalSetpoint;\n}\n"


def build(workdir: Path, edits=()):
    src = base.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    for old, new in edits:
        assert src.count(old) == 1, "edit target must occur exactly once: %r (%d)" % (old, src.count(old))
        src = src.replace(old, new)
    body = base.extract(src)
    pre = ""
    for name, stub in (("satCurveTuneNoteNotControlling", "static void satCurveTuneNoteNotControlling(){ }"),):
        if name not in body:
            pre += stub + "\n"
    cycles = "static uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }\n" + base.extract_cycles()
    flag = "#define HAS_REQSP 1\n" if "_sat_requestedSetpoint" in body else ""
    stubs = (base.STUBS.replace("@FORCEFIELD@", "").replace("@AUTOFIELD@", "")
             .replace("@CHENABLE@", (base.REPO / "src" / "OTGW-firmware" / "SATchEnable.h").as_posix()))
    workdir.mkdir(exist_ok=True)
    cpp = workdir / "cc.cpp"
    cpp.write_text(flag + stubs + TICK_STUBS + pre + body + cycles + extract_full_tick(src) + MAIN, encoding="utf-8")
    return base.compile_cpp(cpp, workdir / "cc.exe")


class ColdCutMinBurn(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_sc(self, events, dur=420, exe=None):
        script = "".join("ev %d %s %d\n" % e for e in events) + "run %d\n" % dur
        out = subprocess.run([str(exe or self.exe)], input=script, capture_output=True, text=True, check=True).stdout
        rows = {}
        for line in out.splitlines():
            if line.startswith("T "):
                parts = line.split()
                rows[int(parts[1])] = dict(kv.split("=") for kv in parts[2:])
        return rows

    @staticmethod
    def first_cut(rows):
        """Second of the first tick that sends CH=0 with CS=10 (the cutoff), or None."""
        for sec in sorted(rows):
            if rows[sec]["ch"] == "0" and float(rows[sec]["cs"]) == 10.0:
                return sec
        return None

    @staticmethod
    def first_ch_off(rows):
        """Second of the first tick that sends CH=0, whatever the CS, or None."""
        for sec in sorted(rows):
            if rows[sec]["ch"] == "0":
                return sec
        return None

    # ---- the cold cutoff acts at once (ADR-305 decision 8) ----
    def test_cutoff_of_a_lit_phase_acts_at_once(self):
        # ON starts at tick 0 without a flame, ignition at 40 s (anchor at the 60 s tick), request drops below cold at 85 s. The old rule
        # waited until 240 (180 s after the anchor); thermo-nova disables PWM on the next tick (heating_control.py:229-234): 90.
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20)])
        self.assertEqual(rows[60]["on"], "1", rows[60])
        self.assertEqual(rows[60]["ch"], "1", rows[60])
        self.assertEqual(self.first_cut(rows), 90, rows)
        self.assertEqual(rows[90]["mode"], "1", rows[90])        # PWM left
        self.assertEqual(rows[120]["ch"], "0", rows[120])        # the latched cutoff holds afterwards
        self.assertEqual(rows[240]["ch"], "0", rows[240])

    def test_unlit_phase_is_cut_at_once(self):
        # Still waiting for the first flame when the request drops: cut on the next tick (unchanged).
        rows = self.run_sc([(85, "pid", 20)])
        self.assertEqual(self.first_cut(rows), 90, rows)

    def test_flame_lost_inside_the_first_180s_changes_nothing(self):
        # Lit at 40 s, cold at 85 s, the flame goes out at 125 s: the cutoff was already taken at 90.
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20), (125, "flame", 0)])
        self.assertEqual(self.first_cut(rows), 90, rows)

    def test_cutoff_after_the_180s_is_immediate(self):
        # Request drops after the phase has burnt 180 s (anchor 60 s, cold at 265 s): cut on the next tick, as before.
        rows = self.run_sc([(40, "flame", 1), (265, "pid", 20)])
        self.assertEqual(self.first_cut(rows), 270, rows)

    def test_cutoff_during_a_draw_is_taken_on_the_first_tick_after_it(self):
        # Lit at 40 s, draw 100..155 s with the request dropping below cold at 105 s. The DHW ticks (120, 150) send nothing; the first tick
        # after the draw (180) sees the cold request and cuts, with or without a flame (no wait, no post-draw rule).
        rows = self.run_sc([(40, "flame", 1), (100, "dhw", 1), (105, "pid", 20), (155, "dhw", 0)])
        self.assertEqual(rows[90]["ch"], "1", rows[90])
        self.assertEqual(rows[120]["ch"], "-", rows[120])
        self.assertEqual(self.first_cut(rows), 180, rows)

    # ---- the tolerance band keeps its 180 s minimum burn (ADR-250, entries 275, 376) ----
    def test_band_stop_waits_180s_from_the_ignition_and_keeps_pwm_running(self):
        # ON starts at 0 without a flame, ignition at 40 s (anchor at the 60 s tick), the room rises 0.3 above target at 85 s (target - room
        # = -0.3, past the +0.1 stop). The CH bit stays on until 240 (60 + 180); the stop clears CH only: CS stays the PWM setpoint, the mode stays PWM.
        rows = self.run_sc([(40, "flame", 1), (85, "tol", -30)], 330)
        for sec in (90, 120, 150, 180, 210):
            self.assertEqual(rows[sec]["ch"], "1", (sec, rows[sec]))
        self.assertEqual(self.first_ch_off(rows), 240, rows)
        self.assertEqual(rows[240]["mode"], "2", rows[240])
        self.assertGreater(float(rows[240]["cs"]), 10.0, rows[240])

    def test_band_stop_acts_at_once_when_the_flame_is_dark(self):
        # The same stop with the flame lost at 125 s: nothing left to protect, CH goes off on the next tick (150); PWM stays.
        rows = self.run_sc([(40, "flame", 1), (85, "tol", -30), (125, "flame", 0)], 330)
        self.assertEqual(rows[120]["ch"], "1", rows[120])
        self.assertEqual(self.first_ch_off(rows), 150, rows)
        self.assertEqual(rows[150]["mode"], "2", rows[150])

    def test_band_stop_of_an_unlit_phase_acts_at_once(self):
        rows = self.run_sc([(85, "tol", -30)], 200)
        self.assertEqual(self.first_ch_off(rows), 90, rows)

    # ---- mutants: each rule must be able to fail ----
    def test_mutant_cutoff_waits_for_the_min_burn_again(self):
        # the TASK-1652 rule: a latched cutoff is deferred while the ON phase's flame is younger than 180 s
        exe = build(Path(self.tmp.name) / "m1", edits=[(
            "  _sat_coldCut = satColdCutoffLatched(pidOutput, satGetColdSetpoint(), _sat_coldCut, SAT_COLD_HYSTERESIS_C);",
            "  _sat_coldCut = satColdCutoffLatched(pidOutput, satGetColdSetpoint(), _sat_coldCut, SAT_COLD_HYSTERESIS_C) && !satPwmColdCutWaitsForMinBurn();   // MUTANT")])
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20)], exe=exe)
        self.assertNotEqual(self.first_cut(rows), 90, "the mutant must delay the cutoff, got %r" % self.first_cut(rows))
        print("\nMUTANT cutoff-waits RED: first cut at %r instead of 90" % self.first_cut(rows))

    def test_mutant_band_stop_does_not_wait(self):
        exe = build(Path(self.tmp.name) / "m2", edits=[(
            "    bool bandOff = !_sat_tolDemand && !satBandWaitsForMinBurn();",
            "    bool bandOff = !_sat_tolDemand;   // MUTANT")])
        rows = self.run_sc([(40, "flame", 1), (85, "tol", -30)], 330, exe=exe)
        self.assertNotEqual(self.first_ch_off(rows), 240, "the mutant must stop CH at once, got %r" % self.first_ch_off(rows))
        print("\nMUTANT band-no-wait RED: CH off at %r instead of 240" % self.first_ch_off(rows))


class ColdCutMinBurnSource(unittest.TestCase):
    """The min-burn helper is read by the tolerance band only, never by the cold cutoff (decision 8)."""

    def setUp(self):
        self.src = base.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_cutoff_latch_does_not_read_the_helper(self):
        self.assertNotIn("_sat_coldCutDeferred", self.src)
        line = [ln for ln in self.src.splitlines() if "_sat_coldCut = satColdCutoffLatched(" in ln]
        self.assertEqual(len(line), 1, line)
        self.assertNotIn("WaitsForMinBurn", line[0])

    def test_only_the_band_helper_calls_the_pwm_helper(self):
        users = [ln.strip() for ln in self.src.splitlines() if "satPwmColdCutWaitsForMinBurn()" in ln and not ln.lstrip().startswith("//")]
        self.assertEqual(users, ["static bool satPwmColdCutWaitsForMinBurn()",
                                 "if (state.sat.eControlMode == SAT_MODE_PWM) return satPwmColdCutWaitsForMinBurn();"], users)
        callers = [ln.strip() for ln in self.src.splitlines() if "satBandWaitsForMinBurn()" in ln and not ln.lstrip().startswith("//")]
        self.assertEqual(callers, ["static bool satBandWaitsForMinBurn()",
                                   "bool bandOff = !_sat_tolDemand && !satBandWaitsForMinBurn();"], callers)


if __name__ == "__main__":
    unittest.main(verbosity=2)
