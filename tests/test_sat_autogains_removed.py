#!/usr/bin/env python3
"""TASK-1357: the dead SAT settings satautogains (fAutoGainsValue) and the manual-gain switch
(bAutoGains, fKpManual/fKiManual/fKdManual) are gone, and the PID gains did not change.

Two parts:
  1. Source check: no firmware source (src/OTGW-firmware, every file type, the web UI included) still names
     satautogains / fAutoGainsValue / bAutoGains / fKpManual / fKiManual / fKdManual / auto_gains / kp_manual /
     ki_manual / kd_manual / auto_gains_value. This part fails on HEAD before the removal.
  2. Behaviour: the REAL _pidCalculateGains() is cut out of SATpid.ino and compiled with a host C++ compiler; the Kp/Ki/Kd it
     computes for a grid of heating-curve inputs are compared with the automatic formula as it stood on HEAD
     (pid.py automatic_gains: kp = coeff * curve / divisor, ki = kp / 8400, kd = 0.07 * 8400 * kp; divisor 4 underfloor,
     3 radiators). The stub settings carry the old fields at their old defaults (bAutoGains true), so the same harness
     also compiles the pre-removal SATpid.ino: gains are identical before and after.

OTGW_FW_DIR points at another copy of src/OTGW-firmware (e.g. a `git show HEAD:` export) for the old-vs-fix run.

Run: python tests/test_sat_autogains_removed.py
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
FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))

NAMES = re.compile(
    r"satautogains|fAutoGainsValue|bAutoGains|fKpManual|fKiManual|fKdManual|"
    r"auto_gains|kp_manual|ki_manual|kd_manual",
    re.I,
)

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#define SAT_HSYS_UNDERFLOOR 2
// Superset of the settings: the pre-removal fields are present at their old defaults so the HEAD SATpid.ino compiles too.
struct SatSettings { float fHeatingCurveCoeff = 2.0f; bool bAutoGains = true;
                     float fKpManual = 5.0f, fKiManual = 0.0005f, fKdManual = 0.0f; };
struct Settings { SatSettings sat; };
struct SatState { float fKp = 0, fKi = 0, fKd = 0; };
struct State { SatState sat; };
static Settings settings; static State state;
static int g_system = 1;
static int satGetEffectiveHeatingSystem() { return g_system; }
@CODE@
int main(int argc, char** argv) {
  // argv: coefficient curveValue heatingSystem
  settings.sat.fHeatingCurveCoeff = (float)atof(argv[1]);
  g_system = atoi(argv[3]);
  _pidCalculateGains((float)atof(argv[2]));
  printf("%.9g %.9g %.9g\n", state.sat.fKp, state.sat.fKi, state.sat.fKd);
  return 0;
}
"""


def extract_gain_code(src: str) -> str:
    """The gain constants and the real _pidCalculateGains() body, verbatim."""
    consts = [m.group(0) for m in re.finditer(r"^static const float SAT_PID_(?:AGGRESSION_V3|KP_DIVISOR_FLOOR|KP_DIVISOR_RAD)\s*=.*;", src, re.M)]
    assert len(consts) == 3, consts
    a = src.index("static void _pidCalculateGains(float curveValue)")
    b = src.index("\n}\n", a) + 3
    return "\n".join(consts) + "\n" + src[a:b]


def expected_gains(coeff: float, curve: float, system: int):
    divisor = 4.0 if system == 2 else 3.0
    kp = coeff * curve / divisor
    return kp, kp / 8400.0, 0.07 * 8400.0 * kp


class AutoGainsNamesGone(unittest.TestCase):
    def test_no_autogains_name_left_in_firmware_sources(self):
        left = []
        for f in FW.rglob("*"):
            if not f.is_file() or f.suffix not in (".ino", ".cpp", ".h", ".js", ".html", ".css", ".ini", ".json"):
                continue
            for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                # The retired-topic row that empties the old retained value is the one allowed mention.
                if NAMES.search(line) and "RETIRED-SAT-ENTITY TASK-1357" not in line:
                    left.append(f"{f.relative_to(FW).as_posix()}:{n}")
        self.assertEqual(left, [])

    def test_pid_has_no_manual_gain_branch(self):
        pid = (FW / "SATpid.ino").read_text(encoding="utf-8")
        body = extract_gain_code(pid)
        self.assertNotIn("Manual", body)
        self.assertIn("(coeff * curveValue) / divisor", body)


class GainsUnchanged(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from test_sat_pwm_override import compile_cpp
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        code = extract_gain_code((FW / "SATpid.ino").read_text(encoding="utf-8").replace("\r\n", "\n"))
        cpp = d / "gains.cpp"
        cpp.write_text(STUBS.replace("@CODE@", code), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "gains.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def test_gains_match_the_automatic_formula(self):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        n = 0
        for system in (1, 2):
            for coeff in (0.5, 1.0, 2.0, 3.5):
                for curve in (25.0, 40.0, 55.0):
                    out = subprocess.run([str(self.exe), str(coeff), str(curve), str(system)],
                                         capture_output=True, text=True, check=True).stdout.split()
                    got = [float(x) for x in out]
                    for g, e, what in zip(got, expected_gains(coeff, curve, system), ("kp", "ki", "kd")):
                        self.assertAlmostEqual(g, e, delta=max(1e-6, abs(e) * 2e-6),
                                               msg=f"{what} coeff={coeff} curve={curve} system={system}")
                    n += 1
        self.assertEqual(n, 24)


if __name__ == "__main__":
    unittest.main(verbosity=2)
