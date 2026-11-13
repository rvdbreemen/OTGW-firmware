#!/usr/bin/env python3
"""TASK-1869: a SAT simulation run must not change the stored thermal coefficient (satthermalcoeff).

Found on the bench (otgw-firmware-09, .61, beta.216): with satsimulation on, the persisted satthermalcoeff went from
0.0966 to 0.0998. satUpdateThermalLearning() saved its EMA every hour through updateSetting("SATthermalcoeff") with no
simulation guard, and the EMA learned in simulation stayed in RAM, so the next real save would store it as well.

REAL code, cut from src/OTGW-firmware/SATcontrol.ino and compiled on the host: the thermal learning constants, the
_thermal_* statics and satUpdateThermalLearning(). STUBBED: settings/state, millis(), the flame / room / outside inputs,
updateSetting() (records each SATthermalcoeff write and applies it), dtostrf, the debug sink.

Scenarios (a room that cools 0.2 C every 5 min, 15 C above outside, flame off):
  sim     - 3 h in SAT simulation: no SATthermalcoeff write, the stored value unchanged.
  simreal - 3 h in simulation, then 3 h real; real - the same 3 h real without the simulation before it. The real
            writes must be equal: the simulation leaves no trace in the real model.
Old-vs-fix: OTGW_SAT_INO=<older SATcontrol.ino> python tests/test_sat_thermal_sim_persist.py
Run: python tests/test_sat_thermal_sim_persist.py
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
#include <cstdlib>
#include <cstring>
#define PSTR(x) x
#define SATDebugTf(...) do {} while (0)
#define PGM_P const char*
static void satNarratef_P(PGM_P, ...) {}   // TASK-1891 note when the sim cool rate is out of range
struct SatSettings { bool bSimulation = false, bFireplaceMode = false; float fThermalCoeff = 0.0966f; float fSimCoolRate = 0.1f; };
struct Settings { SatSettings sat; };
struct SatState { float fLastKnownRoom = 0.0f; uint32_t iLastKnownRoomMs = 0; float fThermalDropRate = 0.0f;
                  bool bThermalModelValid = false; };
struct State { SatState sat; };
static Settings settings; static State state;
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static bool satIsEnabled() { return true; }
static bool satIsFlameOn() { return false; }
static float g_room = 20.0f;
static float satGetRoomTemp() { return g_room; }
static float satGetOutsideTemp() { return g_room - 15.0f; }   // the delta stays 15 C
static int g_writes = 0; static char g_last[16] = "";
static void updateSetting(const char* field, const char* v) {
  if (strcmp(field, "SATthermalcoeff")) return;
  g_writes++; strncpy(g_last, v, sizeof(g_last) - 1); settings.sat.fThermalCoeff = (float)atof(v);
}
static char* dtostrf(double v, signed char w, unsigned char p, char* s) { sprintf(s, "%*.*f", w, p, v); return s; }
"""

MAIN = r"""
static void hours(int h) {   // h hours of 5-min samples, the room cooling 0.2 C each
  for (int i = 0; i < h * 12; i++) { g_now += 300000UL; g_room -= 0.2f; satUpdateThermalLearning(); }
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  satUpdateThermalLearning();                      // first call: starts a decay period
  int simWrites = 0;
  if (!strcmp(sc, "sim") || !strcmp(sc, "simreal")) {
    settings.sat.bSimulation = true; satUpdateThermalLearning();
    hours(3);
    simWrites = g_writes;
  }
  if (!strcmp(sc, "sim")) { printf("simwrites=%d stored=%.4f\n", simWrites, settings.sat.fThermalCoeff); return 0; }
  if (!strcmp(sc, "real")) g_now += 3UL * 3600000UL;   // the same clock as after the 3 h sim phase (the save is hourly)
  const int before = g_writes;
  // the real house again, same start as the real-only run: room 20 C, a decay period starting at this call
  g_room = 20.0f; settings.sat.bSimulation = false; satUpdateThermalLearning();
  hours(3);
  printf("simwrites=%d realwrites=%d last=%s stored=%.4f\n", simWrites, g_writes - before, g_last, settings.sat.fThermalCoeff);
  return 0;
}
"""


def cut_braced(src: str, head: str) -> str:
    i = src.index(head)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += {"{": 1, "}": -1}.get(src[k], 0)
        if depth == 0:
            return src[i:k + 1]
    raise AssertionError(f"unbalanced braces after {head}")


def build_source() -> str:
    src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    consts = re.search(r"// --- Thermal Drop Learning Constants.*?\n((?:static const [^\n]*\n)+)", src).group(1)
    statics = re.search(r"(static float\s+_thermal_prevRoom[^\n]*\n(?:static [^\n]*_thermal_[^\n]*\n)+)", src).group(1)
    fn = cut_braced(src, "static void satUpdateThermalLearning()")
    return STUBS + consts + statics + fn + "\n" + MAIN


class ThermalSimPersist(unittest.TestCase):
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

    def test_simulation_never_writes_the_coefficient(self):
        r = self.run_case("sim")
        self.assertEqual((r["simwrites"], r["stored"]), ("0", "0.0966"), "a sim run leaves satthermalcoeff as it was")

    def test_simulation_leaves_no_trace_in_the_real_model(self):
        real, simreal = self.run_case("real"), self.run_case("simreal")
        self.assertNotEqual(real["realwrites"], "0", "the real run still saves every hour")
        self.assertEqual((simreal["realwrites"], simreal["last"], simreal["stored"]),
                         (real["realwrites"], real["last"], real["stored"]),
                         "after a sim run the real learning saves what it saves without one")


if __name__ == "__main__":
    unittest.main(verbosity=2)
