#!/usr/bin/env python3
"""TASK-1891: the SAT simulation and the thermal learning.

In the simulation the room cools by satsimcoolrate x (room - outside) / SAT_SIM_COOL_REF_K per minute
(SATcontrol.ino, the fSimRoomTemp update), and satUpdateThermalLearning() takes a sample as the drop per hour divided by
(room - outside). With the flame off every sample is therefore 60 / SAT_SIM_COOL_REF_K x satsimcoolrate = 6 x coolrate,
whatever the delta. At the default 0.1 that is 0.6, above SAT_THERMAL_COEFF_MAX 0.3, so every sample is rejected and a sim
run never exercises the thermal learning (found by 09 on the TASK-1869 bench of beta.220).

REAL code, cut from src/OTGW-firmware/SATcontrol.ino and compiled on the host: the thermal-learning constants, the
_thermal_* statics, satUpdateThermalLearning(), SAT_SIM_COOL_REF_K / SAT_SIM_EMITTER_REF_KW and the room-temperature
statement of the simulation model (pasted verbatim into a stub step). STUBBED: settings/state, millis(), the flame (off),
the room and outside getters (they read the sim model), updateSetting(), dtostrf, the debug sink.

Scenarios: hours of 1-minute sim steps with the flame off, counting accepted samples (state.sat.fThermalDropRate set).
Run: python tests/test_sat_sim_thermal_learning.py
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
from test_sat_curve_selftune import compile_cpp  # noqa: E402
from test_sat_thermal_sim_persist import cut_braced  # noqa: E402

REPO = TESTS.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
TYPES_H = Path(os.environ.get("OTGW_SATTYPES_H", REPO / "src" / "OTGW-firmware" / "SATtypes.h"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define PSTR(x) x
#define SATDebugTf(...) do {} while (0)
#define PGM_P const char*
static int g_narr = 0;                                    // TASK-1891: satNarratef_P calls
static void satNarratef_P(PGM_P, ...) { g_narr++; }
struct SatSettings { bool bSimulation = true, bFireplaceMode = false; float fThermalCoeff = 0.0966f;
                     float fSimCoolRate = SIM_COOL_DEFAULT, fSimHeatRate = SIM_HEAT_DEFAULT; };
struct Settings { SatSettings sat; };
struct SatState { float fLastKnownRoom = 0.0f; uint32_t iLastKnownRoomMs = 0; float fThermalDropRate = 0.0f;
                  bool bThermalModelValid = false; float fSimRoomTemp = 20.0f, fSimOutdoorTemp = 5.0f;
                  uint32_t iSimWindowExpiryMs = 0; float fSimWindowLossMult = 1.0f; };
struct State { SatState sat; };
static Settings settings; static State state;
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static bool satIsEnabled() { return true; }
static bool satIsFlameOn() { return false; }
static float satGetRoomTemp() { return state.sat.fSimRoomTemp; }
static float satGetOutsideTemp() { return state.sat.fSimOutdoorTemp; }
static void updateSetting(const char*, const char*) {}
static char* dtostrf(double v, signed char w, unsigned char p, char* s) { sprintf(s, "%*.*f", w, p, v); return s; }
"""

MAIN = r"""
static void simStep(float dtSec, float emitKw) {   // the real room-temperature statement of the sim model
  const float dtMin = dtSec / 60.0f;
  float coolRate = settings.sat.fSimCoolRate;
  if (state.sat.iSimWindowExpiryMs != 0) coolRate *= state.sat.fSimWindowLossMult;
ROOM_STATEMENT
}
int main(int argc, char** argv) {
  if (argc > 2) { settings.sat.fSimCoolRate = (float)atof(argv[1]); settings.sat.fSimHeatRate = (float)atof(argv[2]); }
  int accepted = 0; float last = state.sat.fThermalDropRate;
  satUpdateThermalLearning();
  for (int m = 0; m < 6 * 60; m++) {          // 6 h, flame off, 1-minute steps
    g_now += 60000UL; simStep(60.0f, 0.0f);
    satUpdateThermalLearning();
    if (state.sat.fThermalDropRate != last) { accepted++; last = state.sat.fThermalDropRate; }
  }
  printf("accepted=%d sample=%.4f room=%.2f narr=%d\n", accepted, state.sat.fThermalDropRate, state.sat.fSimRoomTemp, g_narr);
  return 0;
}
"""


def default_of(name):
    m = re.search(r"float\s+" + name + r"\s*=\s*([0-9.]+)f;", TYPES_H.read_text(encoding="utf-8"))
    assert m, name + " default not found in SATtypes.h"
    return m.group(1)


def build_source():
    src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    consts = re.search(r"// --- Thermal Drop Learning Constants.*?\n((?:static const [^\n]*\n)+)", src).group(1)
    sim_consts = "".join(re.findall(r"static const float\s+SAT_SIM_(?:COOL_REF_K|EMITTER_REF_KW)\s*=[^\n]*\n", src))
    assert sim_consts.count("SAT_SIM_") == 2, "sim reference constants not found"
    statics = re.search(r"(static float\s+_thermal_prevRoom[^\n]*\n(?:static [^\n]*_thermal_[^\n]*\n)+)", src).group(1)
    fn = cut_braced(src, "static void satUpdateThermalLearning()")
    room = re.search(r"\n(  state\.sat\.fSimRoomTemp \+= \(settings\.sat\.fSimHeatRate \* emitKw / SAT_SIM_EMITTER_REF_KW\n"
                     r"[^\n]*\* dtMin;)\n", src)
    assert room, "the sim room-temperature statement moved"
    stubs = STUBS.replace("SIM_COOL_DEFAULT", default_of("fSimCoolRate") + "f").replace(
        "SIM_HEAT_DEFAULT", default_of("fSimHeatRate") + "f")
    return stubs + consts + sim_consts + statics + fn + "\n" + MAIN.replace("ROOM_STATEMENT", room.group(1))


class SimThermalLearning(unittest.TestCase):
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

    def run_case(self, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_default_sim_rejects_every_sample(self):
        # documents the finding: 6 x 0.1 = 0.6 > SAT_THERMAL_COEFF_MAX 0.3
        r = self.run_case()
        self.assertEqual(r["accepted"], "0", r)

    def test_default_sim_says_so_once_per_run(self):
        # TASK-1891, Robert's choice B: keep the fast sim, narrate once that thermal learning is out of range
        self.assertEqual(self.run_case()["narr"], "1")                  # 6 h of rejected samples, one note
        self.assertEqual(self.run_case(0.016, 0.08)["narr"], "0")       # in range: no note

    def test_realistic_rates_feed_the_learner(self):
        # coolrate 0.016 / heatrate 0.08: sample = 6 x 0.016 = 0.096, the order of a real house (0.0966 on .61)
        r = self.run_case(0.016, 0.08)
        self.assertGreater(int(r["accepted"]), 0, r)
        self.assertAlmostEqual(float(r["sample"]), 0.096, delta=0.01)

    def test_boundary_is_six_times_the_cool_rate(self):
        self.assertGreater(int(self.run_case(0.045, 0.08)["accepted"]), 0)    # 0.27 <= 0.3
        self.assertEqual(self.run_case(0.06, 0.08)["accepted"], "0")          # 0.36 > 0.3


if __name__ == "__main__":
    unittest.main(verbosity=2)
