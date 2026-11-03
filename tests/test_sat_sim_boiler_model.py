#!/usr/bin/env python3
"""TASK-1532: host test for the SAT simulation boiler model (satUpdateSimulation()).

Before the fix the synthetic boiler short-cycled at every setpoint: its water had almost no heat
capacity (0.4 C/s per kW) and its emitter took 0.02 kW/K, so the 1.2 kW minimum output always
exceeded the load (37-54 starts an hour, 5-12 s burns). No self-tune hour could count in SAT
simulation, and the room warmed at a fixed rate whenever the flame was on, whatever the flow.

The code under test is NOT re-implemented here. The test cuts the real satUpdateSimulation()
and every `static const ... SAT_SIM_*` / SAT_MIN_SETPOINT constant out of
src/OTGW-firmware/SATcontrol.ino and compiles them with a host C++ compiler against small stubs.
Without a compiler the tests are skipped, they do not pass silently.

Old-vs-fix: OTGW_SAT_INO points at another copy, e.g. the output of
`git show HEAD:src/OTGW-firmware/SATcontrol.ino`.

Run: python tests/test_sat_sim_boiler_model.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (shared host compiler lookup: g++, clang++ or MSVC)

REPO = Path(__file__).resolve().parents[1]
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

PRELUDE = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#define F(x) x
#define PSTR(x) x
static void SATTraceTln(const char*) {}
static void SATDebugTln(const char*) {}
static void satNarrate_P(const char*) {}
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static int g_flameChanges = 0;
static void satCycleOnFlameChange(bool) { g_flameChanges++; }
static float g_out = 8.0f;
static float satSimOutdoorTemp() { return g_out; }
#define SAT_QUIRK_MIN_MOD_10 0x01
static uint8_t satGetManufacturerQuirks() { return 0; }
static const int SAT_DHW_SETPOINT_MIN = 30, SAT_DHW_SETPOINT_MAX = 65;
static const uint8_t SAT_MAX_ZONES = 4, SAT_MAX_AREAS = 4;
enum { SAT_HSYS_AUTO = 0, SAT_HSYS_RADIATORS = 1, SAT_HSYS_UNDERFLOOR = 2 };
struct SatSettings {
  bool bSimulation = true; float fDhwSetpoint = 50, fOvershootMargin = 2.0f; uint8_t iMaxRelModulation = 100;
  float fBoilerCapacity = 24.0f, fMaxSetpoint = 65.0f, fTargetTemp = 20.0f, fSimHeatRate = 0.5f, fSimCoolRate = 0.1f;
  uint8_t iZoneCount = 1; bool bMultiArea = false; uint8_t iMultiAreaCount = 0; uint8_t iHeatingSystem = 1;
};
struct Settings { SatSettings sat; } settings;
struct SatState {
  float fSimFlowTemp = 20, fSimReturnTemp = 20, fSimRoomTemp = 20, fSimOutdoorTemp = 8; uint8_t iSimModulation = 0;
  bool bSimFlameOn = false, bSimWarmupDone = false; uint32_t iSimFlameOnSinceMs = 0, iSimFlameOffSinceMs = 0, iSimLastUpdateMs = 0;
  uint32_t iSimDhwExpiryMs = 0, iSimDhwNextSchedMs = 0, iSimWindowExpiryMs = 0, iSimSolarExpiryMs = 0, iSimNoiseExpiryMs = 0, iSimDropoutExpiryMs = 0;
  float fSimWindowLossMult = 1, fSimSolarGainC = 0, fSimNoiseAmplitudeC = 0;
  bool bActive = true; float fFinalSetpoint = 30;
  float fAreaTemp[4] = {0}; bool bAreaValid[4] = {false}; uint32_t iAreaLastMs[4] = {0};
};
struct State { SatState sat; } state;
struct SATZoneState { bool bOff, bSpValid, bRoomValid; float fSetpoint, fRoomTemp; uint32_t iLastUpdateMs; };
static SATZoneState satZones[4];
static uint8_t satGetEffectiveHeatingSystem() { return settings.sat.iHeatingSystem == SAT_HSYS_AUTO ? SAT_HSYS_RADIATORS : settings.sat.iHeatingSystem; }
"""

MAIN = r"""
// run system out setpoint seconds [room0]: one call per second; prints stats after the first 30 min
int main(int argc, char** argv) {
  settings.sat.iHeatingSystem = (uint8_t)atoi(argv[1]); g_out = (float)atof(argv[2]);
  state.sat.fFinalSetpoint = (float)atof(argv[3]); const int secs = atoi(argv[4]);
  if (argc > 5) state.sat.fSimRoomTemp = state.sat.fSimFlowTemp = state.sat.fSimReturnTemp = (float)atof(argv[5]);
  satUpdateSimulation();                                         // first call only stamps the time
  int starts = 0, onTicks = 0, ticks = 0, minMod = 101, above = 0, maxAbove = 0; bool prev = state.sat.bSimFlameOn;
  const float room0 = state.sat.fSimRoomTemp;
  for (int s = 1; s <= secs; s++) {
    g_ms += 1000; state.sat.iSimDhwNextSchedMs = g_ms + 0x7FFFFFFFUL;   // no scheduled hot-water draws
    satUpdateSimulation();
    if (s > 1800) {
      ticks++;
      if (state.sat.bSimFlameOn) { onTicks++; if (state.sat.iSimModulation < minMod) minMod = state.sat.iSimModulation; }
      if (state.sat.bSimFlameOn && !prev) starts++;
      above = (state.sat.fSimFlowTemp >= state.sat.fFinalSetpoint + 2.0f) ? above + 1 : 0;   // SAT's overshoot test
      if (above > maxAbove) maxAbove = above;
    }
    prev = state.sat.bSimFlameOn;
  }
  printf("starts=%d on=%.3f minmod=%d above=%d room0=%.2f room=%.2f flow=%.2f\n", starts, ticks ? (double)onTicks / ticks : 0.0,
         minMod == 101 ? 0 : minMod, maxAbove, room0, state.sat.fSimRoomTemp, state.sat.fSimFlowTemp);
  return 0;
}
"""


def cut_sources(text):
    m = re.search(r"^static void satUpdateSimulation\(\)\n\{.*?^\}\n", text, re.S | re.M)
    if not m:
        raise AssertionError("satUpdateSimulation() not found in " + str(SAT_INO))
    mm = re.search(r"^static uint8_t satSimMinMod\(\)\n\{.*?^\}\n", text, re.S | re.M)
    if not mm:
        raise AssertionError("satSimMinMod() not found in " + str(SAT_INO))
    consts = [l for l in text.splitlines()
              if re.match(r"\s*static const \w+\s+(SAT_SIM_\w+|SAT_MIN_SETPOINT)\s*=", l)]
    return "\n".join(consts) + "\n" + mm.group(0) + m.group(0)


class SimBoilerModel(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="satsim-")
        src = Path(cls.tmp) / "sim.cpp"
        src.write_text(PRELUDE + cut_sources(SAT_INO.read_text(encoding="utf-8")) + MAIN, encoding="utf-8")
        exe = base.compile_cpp(src, Path(cls.tmp) / "sim.exe")
        cls.exe = str(exe) if exe else None

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_model(self, system, out, sp, secs=5400, room0=None):
        args = [self.exe, str(system), str(out), str(sp), str(secs)] + ([str(room0)] if room0 is not None else [])
        line = subprocess.run(args, capture_output=True, text=True, check=True).stdout.strip()
        return {k: float(v) for k, v in (p.split("=") for p in line.split())}

    def test_radiators_burn_steadily_at_the_curve_setpoint(self):
        # SAT curve at coefficient 2.0, radiators, outside 8 C: 31.8 C (SATcontrol.ino satCalcHeatingCurve)
        r = self.run_model(1, 8, 31.8)
        self.assertEqual(r["starts"], 0, r)          # no new start in the hour after the first 30 min
        self.assertEqual(r["on"], 1.0, r)
        self.assertGreater(r["minmod"], 5, r)        # above the 5 % minimum: real low-load burning

    def test_underfloor_burns_steadily_at_the_curve_setpoint(self):
        # SAT curve at coefficient 2.0, underfloor, outside 8 C: 24.6 C
        r = self.run_model(2, 8, 24.6)
        self.assertEqual(r["starts"], 0, r)
        self.assertEqual(r["on"], 1.0, r)
        self.assertGreater(r["minmod"], 5, r)

    def test_load_below_minimum_output_still_cycles(self):
        # mild outside, a flow setpoint 1 K above the room: up to sp + 2 the emitter takes at most
        # 0.33 x 3 = 1.0 kW, less than the 1.2 kW minimum output, so the flow overshoots and the flame stops
        # (with real water mass one on/off cycle takes tens of minutes, not seconds)
        r = self.run_model(1, 19, 22.0, room0=21.0)
        self.assertGreaterEqual(r["starts"], 1, r)
        self.assertLess(r["on"], 0.9, r)

    def test_room_warms_with_the_emitter_output_not_a_fixed_rate(self):
        # a flow far above what the house needs overheats the room, as a real one would; the old model
        # clamped the room at the target while the flame burned, whatever the flow
        hi = self.run_model(1, 8, 50.0)
        self.assertGreater(hi["room"], 20.5, hi)
        # and a flow too low for the house lets it cool below the target although the boiler burns
        lo = self.run_model(1, 2, 27.0)
        self.assertGreater(lo["on"], 0.5, lo)
        self.assertLess(lo["room"], 19.5, lo)


    # --- TASK-1535: a normal 24 kW combi (minimum 20 %, own switch-off hysteresis) ---
    def test_low_load_flow_stays_above_cs_plus_2_long_enough_for_sats_pwm_switch(self):
        # radiators, outside 14 C, SAT curve 28.9 C: the load is below the boiler's minimum
        r = self.run_model(1, 14, 28.9)
        self.assertGreaterEqual(r["above"], 60, r)
        self.assertGreaterEqual(r["minmod"], 20, r)

    def test_cold_weather_still_burns_continuously_above_the_minimum(self):
        r = self.run_model(1, 2, 35.7)
        self.assertEqual((r["starts"], r["on"]), (0, 1.0), r)
        self.assertGreater(r["minmod"], 20, r)


class SatTickSource(unittest.TestCase):
    def test_sattick_flow_and_return_use_the_simulation_aware_getters(self):
        text = SAT_INO.read_text(encoding="utf-8")
        ticks = [l for l in text.splitlines() if 'PSTR("SATTICK' in l]
        self.assertEqual(len(ticks), 2, ticks)
        tail = text[text.index(ticks[1]):text.index(ticks[1]) + 1500]
        self.assertIn("satGetFlowTemp()", tail)
        self.assertIn("satGetReturnTemp()", tail)
        self.assertNotIn("OTcurrentSystemState.Tboiler", tail)
        dhw = text[text.index(ticks[0]):text.index(ticks[0]) + 400]
        self.assertNotIn("OTcurrentSystemState.Tboiler", dhw)

    def test_selftune_min_modulation_is_the_simulated_boilers_in_simulation(self):
        # a replayed MsgID 15 must not set the self-tune's minimum modulation while SAT simulates the boiler
        text = SAT_INO.read_text(encoding="utf-8")
        line = text[text.index("ti.minMod ="):text.index("ti.minMod =") + 300]
        self.assertIn("settings.sat.bSimulation ? (float)satSimMinMod()", line)


if __name__ == "__main__":
    unittest.main(verbosity=2)
