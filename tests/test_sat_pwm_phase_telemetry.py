#!/usr/bin/env python3
"""TASK-1880: per PWM ON phase, record what SAT asked for and what the boiler did (ignition delay, CH flame seconds,
ignited yes/no). Telemetry only. Evidence for Q-P2: on gruijter's 12 Nov capture a 300 s saturation ran while the boiler
never lit; the cycle classifier records per flame cycle, so a phase that never lit left no record at all.

REAL code, cut from src/OTGW-firmware/SATcontrol.ino and compiled on the host: the TASK-1880 block (the record, the ring,
the counters, satPwmPhaseSample/Open/Close). STUBBED: millis-free (every call takes nowMs), the debug sink.
The call sites (satPwmBeginOn, satPwmEndOn, satPwmReset, the loop-pass sample on the flame bit with the DHW bit clear) are
pinned by regex on the real source, and a mutant that judges ignition on the control setpoint instead of the flame bit must
fail that pin.

Scenarios:
  noign  - ON phase in duty range 5 (2400 s asked), the burner never lights, 300 s later the saturation exit resets PWM:
           ignited=0, ignition delay none, cmd_on 2400, flame_on 0, end=cut, not_ignited counter 1.
  lit    - 600 s asked, CH flame from 45 s to 245 s, a hot-water flame 300-360 s (not counted), ON->OFF at 600 s:
           ignited=1, delay 45, flame_on 200, end=off.
  burning- the burner is already lit at the ON start: delay 0.
  ring   - 10 phases: the ring keeps the last 8, the counters count all 10.
Old-vs-fix: on the code before TASK-1880 the block does not exist (build_source fails: red).
Run: python tests/test_sat_pwm_phase_telemetry.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_curve_selftune import compile_cpp  # noqa: E402  (shared g++/clang++/MSVC helper)

REPO = TESTS.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#define PSTR(x) x
#define SATDebugTf(...) do {} while (0)
"""

MAIN = r"""
static void run(uint32_t& t, uint32_t untilMs, bool lit) { for (; t < untilMs; t += 1000) satPwmPhaseSample(lit, t); }
static void dump() {
  const SatPwmPhaseRec& p = _pwmPhHist[(uint8_t)((_pwmPhHead + SAT_PWM_PHASE_HIST - 1) % SAT_PWM_PHASE_HIST)];
  printf("cmd=%u final=%u len=%u ignited=%u delay=%d flame=%u end=%u count=%u phases=%lu notign=%lu ign=%lu max=%u\n",
         p.cmdOnS, p.finalOnS, p.lenS, p.ignited, p.ignitionDelayS == SAT_PWM_NO_IGNITION ? -1 : (int)p.ignitionDelayS,
         p.flameOnS, p.endReason, _pwmPhCount, (unsigned long)_pwmPhCnt.phases, (unsigned long)_pwmPhCnt.notIgnited,
         (unsigned long)_pwmPhCnt.ignited, _pwmPhCnt.ignitionDelayMaxS);
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  uint32_t t = 1000000;
  if (!strcmp(sc, "noign")) {
    satPwmPhaseOpen(2400000UL, false, t);
    run(t, t + 300000, false);
    satPwmPhaseClose(SAT_PWM_PHASE_END_CUT, 2400000UL, t);   // satPwmReset() on the saturation exit
  } else if (!strcmp(sc, "lit")) {
    const uint32_t t0 = t;
    satPwmPhaseOpen(600000UL, false, t);
    run(t, t0 + 45000, false); run(t, t0 + 245000, true); run(t, t0 + 300000, false);
    run(t, t0 + 360000, false);   // hot-water flame: flame bit with the DHW bit set reads lit=false at the call site
    run(t, t0 + 600000, false);
    satPwmPhaseClose(SAT_PWM_PHASE_END_OFF, 560000UL, t);   // ON->OFF with the ON time re-evaluated to 560 s
  } else if (!strcmp(sc, "burning")) {
    satPwmPhaseOpen(300000UL, true, t);
    run(t, t + 300000, true);
    satPwmPhaseClose(SAT_PWM_PHASE_END_OFF, 300000UL, t);
  } else if (!strcmp(sc, "ring")) {
    for (int k = 0; k < 10; k++) {
      satPwmPhaseOpen(200000UL + 1000UL * k, k % 2 == 0, t);   // a new open also closes an open phase (end=cut)
      run(t, t + 200000, k % 2 == 0);
    }
    satPwmPhaseClose(SAT_PWM_PHASE_END_OFF, 209000UL, t);
  }
  dump();
  return 0;
}
"""


def build_source() -> str:
    src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    a = src.index("// --- TASK-1880: what each PWM ON phase asked for")
    b = src.index("// --- TASK-1880 end")
    return STUBS + src[a:b] + "\n" + MAIN


CALL_PINS = {
    "loop pass samples the CH flame (flame bit, DHW bit clear)": r"satPwmPhaseSample\(currentFlame && !dhwBitNow, millis\(\)\);",
    "ON start opens the record": r"satPwmPhaseOpen\(onTimeMs, flame && !state\.sat\.bDhwActive, _pwm_lastUpdateMs\);",
    "ON->OFF closes it as off": r"satPwmPhaseClose\(SAT_PWM_PHASE_END_OFF, onTimeMs, millis\(\)\);",
    "a reset closes it as cut": r"void satPwmReset\(\)\n\{\n  satPwmPhaseClose\(SAT_PWM_PHASE_END_CUT, _pwm_lastOnTimeMs, millis\(\)\);",
}


def pins_failing(src: str):
    return [name for name, rx in CALL_PINS.items() if not re.search(rx, src)]


class PwmPhaseTelemetry(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_phase_without_ignition_is_recorded_as_not_ignited(self):
        r = self.run_case("noign")
        self.assertEqual((r["ignited"], r["delay"], r["cmd"], r["flame"], r["len"], r["end"], r["notign"]),
                         ("0", "-1", "2400", "0", "300", "1", "1"))

    def test_lit_phase_records_delay_and_ch_flame_seconds(self):
        r = self.run_case("lit")
        self.assertEqual((r["ignited"], r["delay"], r["flame"], r["cmd"], r["final"], r["len"], r["end"]),
                         ("1", "45", "200", "600", "560", "600", "0"))

    def test_burner_already_lit_at_on_start(self):
        r = self.run_case("burning")
        self.assertEqual((r["ignited"], r["delay"], r["flame"]), ("1", "0", "300"))

    def test_ring_keeps_eight_counters_count_all(self):
        r = self.run_case("ring")
        self.assertEqual((r["count"], r["phases"], r["ign"], r["notign"], r["cmd"]), ("8", "10", "5", "5", "209"))

    def test_call_sites_sample_the_flame_bit(self):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertEqual(pins_failing(src), [])

    def test_mutant_ignition_on_setpoint_fails_the_pin(self):
        # judging "lit" from the control setpoint (CS > 10) is what made the Q-P2 phase look like heat; the pin must catch it
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        mutant = src.replace("satPwmPhaseSample(currentFlame && !dhwBitNow, millis());",
                             "satPwmPhaseSample(state.sat.fFinalSetpoint > 10.0f, millis());")
        self.assertNotEqual(mutant, src)
        self.assertIn("loop pass samples the CH flame (flame bit, DHW bit clear)", pins_failing(mutant))


if __name__ == "__main__":
    unittest.main(verbosity=2)
