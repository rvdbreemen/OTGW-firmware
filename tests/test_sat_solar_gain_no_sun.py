#!/usr/bin/env python3
"""TASK-1408: solar gain needs a valid sun elevation (thermo-nova solar_gain.py:56-58, maintainer override 2026-10-26).

Nothing is re-implemented. The test cuts the real satUpdateSolarGain() (with its static state) out of
SATcontrol.ino, from "_solar_prevRoomTemp" up to the Summer Simmer banner, and compiles it with a host C++ compiler.
The harness calls it once per simulated minute with a room that rises 3 C/h, modulation 10 %, valves open, feature on.

Cases:
  nosun  no sun elevation ever received      -> never active (HEAD: active after 10 min of rise rate alone)
  sun    valid elevation 30 deg, kept fresh  -> active after more than 10 min (both)
  stale  active with a valid elevation, then the elevation stops updating
         -> inactive on the call after it is older than 1 h (HEAD: stays active)
  low    valid elevation 5 deg (< 12 min)    -> never active (both; the existing min-elevation gate)

OTGW_SAT_INO points at another copy (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the old-vs-fix run.

Run: python tests/test_sat_solar_gain_no_sun.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define SATDebugTln(x) ((void)0)
struct SatSettings { bool bSolarGainEnable = true; float fSolarMinRiseRate = 0.6f; float fSolarMinElevation = 12.0f; };
struct Settings { SatSettings sat; };
struct SatState { bool bSolarGainActive = false; float fIndoorRiseRate = 0; bool bSunElevationValid = false;
                  float fSunElevation = 0; uint32_t iSunElevLastMs = 0; bool bValvesOpen = true; };
struct State { SatState sat; };
struct OTSys { float RelModLevel = 10.0f; }; static OTSys OTcurrentSystemState;
static Settings settings; static State state;
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static float g_room = 20.0f;
static float satGetRoomTemp() { return g_room; }
static bool satRoomTempValid(float t) { return !isnan(t) && t >= -10.0f && t <= 50.0f; }
@SPAN@
int main(int argc, char** argv) {
  const char* sc = argv[1];
  int endMin = atoi(argv[2]);
  const int sunUntilMin = !strcmp(sc, "stale") ? 25 : 100000;   // stale: the elevation stops updating after 25 min
  for (int m = 0; m <= endMin; m++) {
    g_ms = 1000 + (uint32_t)m * 60000UL;
    g_room = 20.0f + 3.0f * (float)m / 60.0f;                   // +3 C/h
    if (strcmp(sc, "nosun") && m <= sunUntilMin) {
      state.sat.fSunElevation = !strcmp(sc, "low") ? 5.0f : 30.0f;
      state.sat.bSunElevationValid = true;
      state.sat.iSunElevLastMs = g_ms;
    }
    satUpdateSolarGain();
    printf("%d %d\n", m, (int)state.sat.bSolarGainActive);
  }
  return 0;
}
"""


def build_cpp(src: str) -> str:
    a = src.index("static float    _solar_prevRoomTemp")
    b = src.index("//=== Summer Simmer", a)
    b = src.rindex("//=====", a, b)
    return STUBS.replace("@SPAN@", src[a:b])


class SolarGainNoSun(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc, end_min):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc, str(end_min)], capture_output=True, text=True, check=True).stdout
        return {int(m): int(a) for m, a in (ln.split() for ln in out.strip().split("\n"))}

    def test_a_no_sun_elevation_never_active(self):
        trace = self.run_sc("nosun", 20)
        self.assertEqual([m for m, a in trace.items() if a], [], "no sun elevation: solar gain stays off at +3 C/h")

    def test_b_valid_high_sun_becomes_active_after_ten_minutes(self):
        trace = self.run_sc("sun", 20)
        on = [m for m, a in trace.items() if a]
        self.assertTrue(on, "valid elevation 30 deg: solar gain becomes active")
        self.assertGreater(on[0], 10, "not before the 10 min sustain")

    def test_c_stale_sun_elevation_switches_it_off(self):
        # Elevation last updated at 25 min; it is older than 1 h from minute 86 on.
        trace = self.run_sc("stale", 100)
        self.assertEqual(trace[80], 1, "premise: active while the elevation is still younger than 1 h")
        self.assertEqual(trace[85], 1, "premise: still active at exactly 60 min of age")
        self.assertEqual(trace[86], 0, "inactive on the first call after the elevation is older than 1 h")
        self.assertEqual(trace[100], 0, "stays inactive")

    def test_d_low_sun_never_active(self):
        trace = self.run_sc("low", 20)
        self.assertEqual([m for m, a in trace.items() if a], [], "elevation below the minimum: off")


if __name__ == "__main__":
    unittest.main(verbosity=2)
