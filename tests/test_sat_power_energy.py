#!/usr/bin/env python3
"""TASK-1258: host test for satUpdatePowerEnergy() (sat/power and the gas estimate).

The function under test is NOT re-implemented here. The test cuts the real
satUpdatePowerEnergy() text out of src/OTGW-firmware/SATcontrol.ino, wraps it in a
small stub header (settings, state, millis, satIsFlameOn) and compiles it with a host
C++ compiler (g++, clang++ or MSVC cl via vswhere). The cases then run against that
compiled code. Without a host compiler the test is skipped, it does not pass silently.

Cases: CH phase, DHW phase with and without a DHW capacity, flame off, the clamps,
the efficiency division and the rated-input switch/cap of the gas estimate.

Run: python tests/test_sat_power_energy.py
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
# OTGW_SAT_INO points at another copy, e.g. `git show HEAD:src/OTGW-firmware/SATcontrol.ino` for old-vs-fix.
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
struct SatSettings { float fBoilerCapacity, fDhwCapacity, fBoilerRatedKW, fBoilerEfficiency; };
struct Settings { SatSettings sat; };
struct SatState { bool bDhwActive; float fCurrentPower, fEnergyTotal, fEnergyEstimatedKWh;
                  uint32_t iEnergyLastMs, iEstEnergyLastMs; };
struct State { SatState sat; };
struct OTSys { float RelModLevel; float DHWFlowRate = 0; };  // DHWFlowRate: only the pre-TASK-1258 code reads it
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static bool g_flame = true; static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return 0; } static float satGetReturnTemp() { return 0; }  // pre-TASK-1258 code only
"""

MAIN = r"""
// argv: capacity dhwCap ratedKW eff dhwActive flame mod
int main(int argc, char** argv) {
  if (argc != 8) return 2;
  settings.sat.fBoilerCapacity = (float)atof(argv[1]);
  settings.sat.fDhwCapacity    = (float)atof(argv[2]);
  settings.sat.fBoilerRatedKW  = (float)atof(argv[3]);
  settings.sat.fBoilerEfficiency = (float)atof(argv[4]);
  state.sat.bDhwActive = atoi(argv[5]) != 0;
  g_flame = atoi(argv[6]) != 0;
  OTcurrentSystemState.RelModLevel = (float)atof(argv[7]);
  g_ms = 1000;  satUpdatePowerEnergy();      // seeds the integrators
  g_ms = 1000 + 36000; satUpdatePowerEnergy();  // 36 s = 0.01 h
  printf("%.4f %.6f %.6f\n", state.sat.fCurrentPower, state.sat.fEnergyTotal, state.sat.fEnergyEstimatedKWh);
  return 0;
}
"""


def extract_function() -> str:
    src = SAT_INO.read_text(encoding="utf-8")
    m = re.search(r"^static void satUpdatePowerEnergy\(\)\s*\{.*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError("satUpdatePowerEnergy() not found in SATcontrol.ino")
    return m.group(0)


def _vcvars():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    out = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                          "-property", "installationPath"],
                         capture_output=True, text=True).stdout.strip()
    bat = Path(out) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat" if out else None
    return bat if bat and bat.exists() else None


def build(workdir: Path):
    cpp = workdir / "t.cpp"
    cpp.write_text(STUBS + extract_function() + MAIN, encoding="utf-8")
    exe = workdir / "t.exe"
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", "-Wall", "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = _vcvars()
    if bat:
        cmd = f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc /Fe:"{exe}" /Fo:"{workdir}\\\\" "{cpp}"'
        subprocess.run(cmd, shell=True, check=True, cwd=workdir, capture_output=True)
        return exe
    return None


class SatPowerEnergy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        try:
            cls.exe = build(Path(cls.tmp.name))
        except subprocess.CalledProcessError as e:
            raise AssertionError(f"host compile failed: {e.stdout or e.stderr or e}")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_case(self, cap, dhw_cap, rated, eff, dhw, flame, mod):
        out = subprocess.run([str(self.exe), str(cap), str(dhw_cap), str(rated), str(eff),
                              str(int(dhw)), str(int(flame)), str(mod)],
                             capture_output=True, text=True, check=True).stdout.split()
        return tuple(float(x) for x in out)  # power kW, energy kWh, estimated gas kWh

    def test_ch_phase_uses_boiler_capacity(self):
        p, e, g = self.run_case(12, 23, 30, 0.9, False, True, 100)
        self.assertAlmostEqual(p, 12.0, places=3)
        self.assertAlmostEqual(e, 12.0 * 0.01, places=4)
        self.assertAlmostEqual(g, 12.0 / 0.9 * 0.01, places=4)

    def test_dhw_phase_uses_dhw_capacity(self):
        p, e, g = self.run_case(12, 23, 30, 0.9, True, True, 100)
        self.assertAlmostEqual(p, 23.0, places=3)           # not capped back to 12 x 1.1
        self.assertAlmostEqual(e, 23.0 * 0.01, places=4)
        self.assertAlmostEqual(g, 23.0 / 0.9 * 0.01, places=4)

    def test_dhw_capacity_zero_falls_back_to_boiler_capacity(self):
        p, _, _ = self.run_case(12, 0, 30, 0.9, True, True, 50)
        self.assertAlmostEqual(p, 6.0, places=3)

    def test_dhw_capacity_ignored_while_dhw_inactive(self):
        p, _, _ = self.run_case(12, 23, 30, 0.9, False, True, 50)
        self.assertAlmostEqual(p, 6.0, places=3)

    def test_flame_off_is_zero(self):
        self.assertEqual(self.run_case(12, 23, 30, 0.9, True, False, 100), (0.0, 0.0, 0.0))

    def test_gas_estimate_off_without_rated_input(self):
        p, _, g = self.run_case(12, 23, 0, 0.9, False, True, 100)
        self.assertAlmostEqual(p, 12.0, places=3)
        self.assertEqual(g, 0.0)

    def test_gas_estimate_capped_at_rated_input(self):
        # 24 kW / 0.5 = 48 kW gas, rated input 30 kW: capped to 30 (48 > 33 hits the clamp)
        _, _, g = self.run_case(24, 0, 30, 0.5, False, True, 100)
        self.assertAlmostEqual(g, 30.0 * 0.01, places=4)

    def test_gas_estimate_capped_just_above_rated_input(self):
        # 15 kW / 0.48 = 31.25 kW gas, just above the 30 kW rated input: capped to exactly 30
        _, _, g = self.run_case(15, 0, 30, 0.48, False, True, 100)
        self.assertAlmostEqual(g, 30.0 * 0.01, places=4)

    def test_zero_efficiency_does_not_divide(self):
        p, _, g = self.run_case(12, 0, 30, 0.0, False, True, 100)
        self.assertAlmostEqual(p, 12.0, places=3)
        self.assertEqual(g, 0.0)

    def test_power_clamp_uses_active_capacity(self):
        # modulation above 110 % of the active capacity falls back to the capacity itself
        p, _, _ = self.run_case(12, 23, 30, 0.9, True, True, 120)
        self.assertAlmostEqual(p, 23.0, places=3)


class DhwReplayFixture(unittest.TestCase):
    """The bench replay fixture for TASK-1258 must hold valid OT frames: the firmware
    drops a frame with odd parity, so a bad status frame would never set DHW active."""

    FIXTURE = Path(__file__).resolve().parent / "webui" / "fixtures" / "dhw-power-replay.txt"

    def frames(self):
        return [l.strip() for l in self.FIXTURE.read_text().splitlines() if l.strip()]

    def test_every_frame_has_even_parity(self):
        odd = [f for f in self.frames() if bin(int(f[1:], 16)).count("1") % 2]
        self.assertEqual(odd, [])

    def test_fixture_has_a_ch_phase_and_a_dhw_phase_at_full_modulation(self):
        status = {int(f[1:], 16) & 0xFF for f in self.frames()
                  if f[0] == "B" and (int(f[1:], 16) >> 16) & 0xFF == 0}
        self.assertIn(0x0A, status)   # CH active + flame
        self.assertIn(0x0C, status)   # DHW active + flame
        mod = {int(f[5:], 16) / 256 for f in self.frames()
               if f[0] == "B" and (int(f[1:], 16) >> 16) & 0xFF == 17}
        self.assertEqual(mod, {100.0})


if __name__ == "__main__":
    unittest.main(verbosity=2)
