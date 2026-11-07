#!/usr/bin/env python3
"""TASK-1652: a lit PWM ON phase burns at least 180 s before the cold cutoff ends it (maintainer decision 2026-11-05,
docs/sat/sergeant-d-corrections.md entry 275; minimum burn of entries 31 and 80; cold cutoff of entries 12 and 199).

Before TASK-1652 the control tick latched the cold cutoff and, on the same tick, sent CS=10 / CH=0 and left PWM
(satPwmDisableOnHeaterOff()), whatever flame time the running ON phase had. With the rule the latched cutoff waits while
the ON phase's flame is lit, without hot water, and its ignition anchor is younger than 180 s; it acts on the first tick
after. A dark flame, hot water or an unlit phase let it act at once, as before.

The code under test is NOT re-implemented: the real PWM block (shared cut of tests/test_sat_pwm_override.py), the real
cold-cutoff state and threshold (extract_cold_cut()) and the real control-tick segment of satControlLoopRun() from the
solar offset to the CH command (fullTick()) are compiled with a host C++ compiler. satSendCS/satSendMM/satSendCH record
what the tick sends. The DHW early return is driven as tests/test_sat_pwm_anchors.py does (the real satPwmOnDhwStart(),
satPwmNoteDhwFlame(), satPwmOnDhwEnd() hooks, the tick segment skipped while the DHW bit is set). Not cut in: the other
upstream early returns of satControlLoopRun() (fault skip, valves, summer, safety), checked by source order below; the
cycle tracker's flame edge (the driver sets g_flameOnStart as tests/test_sat_pwm_anchors.py does). How hot water and the
180 s interact is our reading (entry 275 lists it as open): the hold keeps the part that ran before the draw (entry 132). Only the host test touches firmware code; the TASK-1412.19 simulation tests the model.

OTGW_SAT_INO points at another copy (e.g. `git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for old-vs-fix runs.
Run: python tests/test_sat_cold_cut_min_burn.py
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
static uint32_t _sat_tickMs = 0;
static int g_sentCh = -1; static float g_sentCs = -1; static int g_sentMm = -1;
static void satSendCS(float cs) { g_sentCs = cs; }
static void satSendMM(int mm) { g_sentMm = mm; }
static void satSendCH(int ch) { g_sentCh = ch; }
enum { SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2, SAT_SRC_HYBRID = 3 };
static uint8_t satGetEffectiveHeatingSource() { return SAT_SRC_GAS_BOILER; }
static uint8_t satGetManufacturerQuirks() { return 0; }
#ifndef SAT_MM_T
#define SAT_MM_T 255
#endif
static uint8_t satReportedModulation(bool atMax, uint8_t mm) { return atMax ? 100 : mm; }
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
  float pid = 35.0f;                                       // duty (35-27.2)/(50-27.2) = 0.34: range 3, on 308 s
  bool prevDhw = false;
  for (int sec = 0; sec <= dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;   // what satCycleOnFlameChange() records
        satPwmNoteDhwFlameEdge(g_flame, state.sat.bDhwActive);              // satControlLoopRun(): every-loop flame edge
      } else if (e.kind == "pid") pid = (float)e.val;
      else if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
    }
    if (sec % 30 != 0) continue;                          // control tick
    if (state.sat.bDhwActive) {                           // hot water: satControlLoopRun() returns before the tick segment
      if (!prevDhw) satPwmOnDhwStart();                   // rising edge: the ON clock and the hold freeze (entries 95, 132)
      satPwmNoteDhwFlame(g_flame);
      prevDhw = true;
      printf("T %d ch=- cs=- mode=%d on=%d flame=%d\n", sec, state.sat.eControlMode, (int)state.sat.bPwmFlameRequested, (int)g_flame);
      continue;
    }
    if (prevDhw) satPwmOnDhwEnd(g_flame);                 // falling edge, before the tick segment
    prevDhw = false;
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
    return "\nstatic float fullTick(float pidOutput) {\n" + loop[a:end] + "  }\n  return state.sat.fFinalSetpoint;\n}\n"


def build(workdir: Path):
    src = base.SAT_INO.read_text(encoding="utf-8")
    body = base.extract(src)
    pre = ""
    for name, stub in (("satCurveTuneNoteNotControlling", "static void satCurveTuneNoteNotControlling(){ }"),):
        if name not in body:
            pre += stub + "\n"
    cycles = "static uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }\n" + base.extract_cycles()
    flag = "#define HAS_REQSP 1\n" if "_sat_requestedSetpoint" in body else ""
    stubs = (base.STUBS.replace("@FORCEFIELD@", "").replace("@AUTOFIELD@", "")
             .replace("@CHENABLE@", (base.REPO / "src" / "OTGW-firmware" / "SATchEnable.h").as_posix()))
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

    def run_sc(self, events, dur=420):
        script = "".join("ev %d %s %d\n" % e for e in events) + "run %d\n" % dur
        out = subprocess.run([str(self.exe)], input=script, capture_output=True, text=True, check=True).stdout
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

    def test_lit_phase_burns_180s_before_the_cutoff(self):
        # ON starts at tick 0 without a flame, ignition at 40 s (anchor at the 60 s tick), request drops below cold at 85 s.
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20)])
        self.assertEqual(rows[60]["on"], "1", rows[60])
        for sec in (90, 120, 150, 180, 210):                  # inside the 180 s from the 60 s anchor: still burning, CH on
            self.assertEqual(rows[sec]["ch"], "1", (sec, rows[sec]))
            self.assertGreater(float(rows[sec]["cs"]), 10.0, (sec, rows[sec]))
        self.assertEqual(self.first_cut(rows), 240, rows)       # 180 s after the anchor: CS=10, CH=0, PWM left
        self.assertEqual(rows[240]["mode"], "1", rows[240])
        self.assertEqual(rows[270]["ch"], "0", rows[270])       # the latched cutoff holds afterwards

    def test_unlit_phase_is_cut_at_once(self):
        # Still waiting for the first flame when the request drops: nothing to protect, cut on the next tick (unchanged).
        rows = self.run_sc([(85, "pid", 20)])
        self.assertEqual(self.first_cut(rows), 90, rows)

    def test_flame_lost_inside_the_hold_cuts_at_once(self):
        # Lit at 40 s, cold at 85 s (deferred), the flame goes out at 125 s: the next tick cuts.
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20), (125, "flame", 0)])
        self.assertEqual(rows[120]["ch"], "1", rows[120])
        self.assertEqual(self.first_cut(rows), 150, rows)

    def test_draw_without_a_ch_flame_after_it_cuts_at_once(self):
        # Lit at 40 s, cold at 85 s (deferred), draw 100-155 s; the flame goes out with the draw at 155 s. The tick at 120/150 is a
        # hot-water tick (the loop returns early, nothing sent). The first tick after the draw (180 s) has no CH flame: cut at once.
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20), (100, "dhw", 1), (155, "flame", 0), (155, "dhw", 0)])
        self.assertEqual(rows[90]["ch"], "1", rows[90])
        self.assertEqual(rows[120]["ch"], "-", rows[120])
        self.assertEqual(self.first_cut(rows), 180, rows)

    def test_draw_with_the_ch_flame_after_it_defers_only_the_rest_of_the_hold(self):
        # Lit at 40 s (anchor at the 60 s tick), cold at 85 s, draw 100-145 s, the flame keeps burning. 60 s of the hold had run at
        # the draw (tick 120); the draw time does not count (entry 132 case 2), so the hold ends 120 s after the resume at 150 s.
        rows = self.run_sc([(40, "flame", 1), (85, "pid", 20), (100, "dhw", 1), (145, "dhw", 0)])
        for sec in (150, 180, 210, 240):
            self.assertEqual(rows[sec]["ch"], "1", (sec, rows[sec]))
        self.assertEqual(self.first_cut(rows), 270, rows)

    def test_cutoff_after_the_180s_is_immediate(self):
        # Request drops after the phase has burnt 180 s (anchor 60 s, cold at 265 s): cut on the next tick, as before.
        rows = self.run_sc([(40, "flame", 1), (265, "pid", 20)])
        self.assertEqual(self.first_cut(rows), 270, rows)


class ColdCutMinBurnSource(unittest.TestCase):
    """The other stop paths act before the deferral decision and never read it (entry 275: they still end the phase at once)."""

    def setUp(self):
        self.src = base.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        if "_sat_coldCutDeferred" not in self.src:
            self.skipTest("source without TASK-1652")

    def test_early_stop_paths_come_before_the_deferral(self):
        loop = self.src[self.src.index("void satControlLoopRun"):]
        defer = loop.index("_sat_coldCutDeferred = _sat_coldCut && satPwmColdCutWaitsForMinBurn();")
        for marker in ("OTcurrentSystemState.SlaveStatus & 0x01",        # boiler fault: tick skipped
                       "if (!state.sat.bValvesOpen) {",                   # valves closed: heater off
                       "satCycleCheckLowLoadVeto();",                     # boiler withdrew low-off permission
                       "if (pidOutput >= maxSetpoint && state.sat.eControlMode == SAT_MODE_PWM) {"):
            self.assertLess(loop.index(marker), defer, marker)

    def test_only_the_cold_cut_helpers_read_the_flag(self):
        users = [m.start() for m in re.finditer(r"_sat_coldCutDeferred", self.src)]
        lines = sorted({self.src[self.src.rfind("\n", 0, u) + 1:self.src.find("\n", u)].strip() for u in users})
        allowed = ("static bool _sat_coldCutDeferred", "return _sat_coldCut && !_sat_coldCutDeferred;",
                   "if (_sat_coldCutDeferred) return SAT_MIN_SETPOINT", "_sat_coldCutDeferred = _sat_coldCut &&",
                   "if (_sat_coldCutDeferred && !satPwmColdCutWaitsForMinBurn())", "if (_sat_coldCutDeferred && !narrColdWait)",
                   "narrColdWait = _sat_coldCutDeferred;", "// ")
        for ln in lines:
            self.assertTrue(ln.startswith(allowed), ln)


if __name__ == "__main__":
    unittest.main(verbosity=2)
