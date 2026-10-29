#!/usr/bin/env python3
"""TASK-1381: continuous mode clamps against the previous CS (thermo-nova heating_control.py:485-489, entry 112).

Nothing is re-implemented. The test cuts the real satApplyContinuous() out of SATcontrol.ino and compiles it
with a host C++ compiler. The harness feeds flame, flow, offset, previous CS and request and prints the CS.

Rule: flame on, valid flow above the request -> minAllowed = flow - offset;
      request > previous -> max(request, minAllowed); else max(request, min(minAllowed, previous)).
      Bypasses (flame off, no/invalid flow, flow <= request) -> request.

Where the previous CS comes from (0 after enable/boot, 10 after heater-off, cold cutoff, valves closed; the CS
sent by thermal-safe and summer simmer) lives in satControlLoop()/satDisable(); those are asserted on the source
because the control loop cannot be sliced.

OTGW_SAT_INO points at another copy (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the old-vs-fix run.

Run: python tests/test_sat_previous_cs_clamp.py
"""
import os
import re
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
struct SatSettings { float fFlowOffset = 2.0f; };
struct Settings { SatSettings sat; };
static Settings settings;
static bool g_flame = true; static float g_flow = 55.0f;
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
@SPAN@
int main(int argc, char** argv) {
  g_flame = atoi(argv[1]) != 0;
  g_flow = (float)atof(argv[2]);
  settings.sat.fFlowOffset = (float)atof(argv[3]);
  float prev = (float)atof(argv[4]);
  float req = (float)atof(argv[5]);
  (void)prev;
  printf("%.1f\n", @CALL@);
  return 0;
}
"""


def build_cpp(src: str) -> str:
    a = src.index("static float satApplyContinuous(")
    b = src.index("//=== Get Room Temperature", a)
    b = src.rindex("//=====", a, b)
    span = src[a:b]
    call = "satApplyContinuous(req, prev)" if "float previousCS" in span else "satApplyContinuous(req)"
    return STUBS.replace("@SPAN@", span).replace("@CALL@", call)


def func_body(src: str, header: str) -> str:
    a = src.index(header)
    nxt = re.search(r"\n(?:static |void |bool |float |uint)[^\n;]*\)\s*\n\{", src[a + len(header):])
    return src[a:a + len(header) + (nxt.start() if nxt else 6000)]


class PreviousCsClamp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(cls.src), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def cs(self, flame, flow, offset, prev, req):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(flame), str(flow), str(offset), str(prev), str(req)],
                             capture_output=True, text=True, check=True).stdout
        return float(out.strip())

    def test_a_falling_request_holds_previous_cs(self):
        # flow 55, offset 2 -> minAllowed 53; request 40 < previous 50 -> max(40, min(53, 50)) = 50 (HEAD: 53)
        self.assertEqual(self.cs(1, 55, 2, 50, 40), 50.0)

    def test_b_rising_request_follows_min_allowed(self):
        # request 45 > previous 40 -> max(45, 53) = 53
        self.assertEqual(self.cs(1, 55, 2, 40, 45), 53.0)

    def test_c_previous_above_min_allowed(self):
        # request 40 < previous 60 -> max(40, min(53, 60)) = 53
        self.assertEqual(self.cs(1, 55, 2, 60, 40), 53.0)

    def test_c2_equal_request_takes_the_falling_branch(self):
        # request == previous is not "request > previous": max(50, min(53, 50)) = 50
        self.assertEqual(self.cs(1, 55, 2, 50, 50), 50.0)

    def test_c3_previous_zero_after_enable(self):
        # previous 0 (after enable/boot): any request > 0 is rising -> max(40, 53) = 53
        self.assertEqual(self.cs(1, 55, 2, 0, 40), 53.0)

    def test_c4_previous_ten_after_cold_cutoff(self):
        self.assertEqual(self.cs(1, 55, 2, 10, 40), 53.0)

    def test_d_bypasses(self):
        self.assertEqual(self.cs(0, 55, 2, 50, 40), 40.0, "flame off -> request")
        self.assertEqual(self.cs(1, 0, 2, 50, 40), 40.0, "flow unavailable -> request")
        self.assertEqual(self.cs(1, 120, 2, 50, 40), 40.0, "flow out of range -> request")
        self.assertEqual(self.cs(1, 38, 2, 50, 40), 40.0, "flow <= request -> request")

    # (e) Source of the previous CS: written by every CS-sending path (control loop cannot be sliced).
    def test_e_previous_cs_sources(self):
        s = self.src
        self.assertRegex(func_body(s, "void satDisable("), r"_sat_lastControlCS = 0\.0f;",
                         "satDisable (enable/boot/OT loss) resets the previous CS to 0")
        self.assertRegex(func_body(s, "static void satPwmDisableOnHeaterOff()"),
                         r"_sat_lastControlCS = SAT_MIN_SETPOINT;", "heater-off sets the previous CS to 10")
        loop = s[s.index("void satControlLoop("):]
        cold = loop[loop.index("if (coldCutoff) {"):]
        cold = cold[:cold.index("\n  }")]
        self.assertIn("satPwmDisableOnHeaterOff();", cold, "cold cutoff goes through heater-off (previous CS 10)")
        simmer = loop[loop.index("if (state.sat.bSummerActive && settings.sat.bSummerSimmer) {"):]
        simmer = simmer[:simmer.index("return; // Skip rest")]
        self.assertIn("_sat_lastControlCS = SAT_MIN_SETPOINT;", simmer, "summer simmer sends CS=10")
        safe = loop[loop.index("_sat_thermalSafeActive = true;"):]
        safe = safe[:safe.index("return;")]
        self.assertIn("_sat_lastControlCS = (float)(int)state.sat.fFinalSetpoint;", safe,
                      "thermal-safe writes the CS it sends")
        valves = loop[loop.index("if (!state.sat.bValvesOpen) {"):]
        valves = valves[:valves.index("return;")]
        self.assertIn("satPwmDisableOnHeaterOff();", valves, "valves closed -> heater-off (previous CS 10)")

    def test_f_continuous_reads_previous_before_the_tick_overwrites_it(self):
        loop = self.src[self.src.index("void satControlLoop("):]
        call = loop.index("finalSetpoint = satApplyContinuous(pidOutput, _sat_lastControlCS);")
        write = loop.index("_sat_lastControlCS = finalSetpoint;")
        self.assertLess(call, write)


if __name__ == "__main__":
    unittest.main()
