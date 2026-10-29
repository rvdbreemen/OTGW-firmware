#!/usr/bin/env python3
"""TASK-1353: the published SAT heating curve is the main curve, also with zones on.

`state.sat.fHeatingCurveValue` feeds MQTT `sat/heating_curve`, the REST/WS status field
`heating_curve` (SATcontrol.ino satStatus JSON, restAPI.ino `state.sat.heating_curve`) and the
telnet debug dump. Before the fix satCalcHeatingCurve() wrote it on every call, and each zone
step (satZonePidOutput, satZonePidAdvance) calls it again with the zone's own target, so with zones on the published
value was the last zone's curve.

The code under test is NOT re-implemented: the test cuts the real satCalcHeatingCurve(),
satZonePidExclude(), satZoneUsable(), satZoneKp(), satZonePidOutput() and the real control-loop segment from
"// --- Calculate heating curve ---" up to the deadband restore (main curve, main PID call and
the whole multi-zone block) out of src/OTGW-firmware/SATcontrol.ino, wraps them in stubs and
compiles them with a host C++ compiler (g++, clang++ or MSVC cl via vswhere). Without a compiler
the tests are skipped, they do not pass silently.

OTGW_SAT_INO points at another copy, e.g. `git show HEAD:src/OTGW-firmware/SATcontrol.ino`,
for old-vs-fix runs.

Not reachable from here: the publishers themselves (publishIfChangedF, the JSON writers); they
read the state field directly, which a source-level check below confirms.

Run: python tests/test_sat_published_curve.py
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
#define SAT_MIN_SETPOINT 10.0f
#define SAT_MAX_ZONES 4
#define SAT_HSYS_UNDERFLOOR 2
static const float SAT_HC_REF_TEMP = 20.0f;
struct SatSettings { float fHeatingCurveCoeff = 1.6f; int iZoneCount = 1; int iZoneTimeoutS = 900;
                     float fDeadband = 0.1f; float fZoneAggregationHeadroom = 0.0f; };
struct Settings { SatSettings sat; };
struct SatState { float fHeatingCurveValue = 0.0f; };
struct State { SatState sat; };
struct SATZoneState { float fRoomTemp = 0, fSetpoint = 0, fPidOutput = 0, fPidIntegral = 0, fPrevError = 0;
                      uint32_t iLastUpdateMs = 0; bool bRoomValid = false, bSpValid = false, bOff = false; };
static Settings settings; static State state; static SATZoneState satZones[SAT_MAX_ZONES];
static uint32_t millis() { return 1000; }
static uint8_t satGetEffectiveHeatingSystem() { return 1; }   // radiators
static float satGetBaseOffset() { return 27.2f; }
static float satGetMaxSetpoint() { return 62.0f; }
static float satGetFlowTemp() { return 40.0f; }
static float satPidError(float t, float r) { return roundf((t - r) * 100.0f) / 100.0f; }
static float satPidOutput(float curve) { return curve; }   // TASK-1368: the tick only reads the PID output
static const bool roomValid = true;                       // TASK-1364: room temperature valid in these scenarios
static float satPidCurveOnly(float curve) { return curve; }
static void satPublishZoneDiagnostics() { }
"""

MAIN = r"""
int main(int argc, char** argv) {   // zoneCount mainTarget outside zone1Target zone2Target ...
  if (argc < 4) return 2;
  settings.sat.iZoneCount = atoi(argv[1]);
  float effectiveTarget = (float)atof(argv[2]);
  float outsideTemp = (float)atof(argv[3]);
  for (int i = 4; i < argc && i - 4 < SAT_MAX_ZONES; i++) {
    SATZoneState& z = satZones[i - 4];
    z.fSetpoint = (float)atof(argv[i]); z.fRoomTemp = z.fSetpoint - 0.5f;
    z.bRoomValid = z.bSpValid = true; z.iLastUpdateMs = 1000;
  }
  float pidOut = tick(effectiveTarget, outsideTemp);
  const float published = state.sat.fHeatingCurveValue;   // read BEFORE the reference call: on HEAD that call writes the field
  const float mainCurve = satCalcHeatingCurve(effectiveTarget, outsideTemp);
  printf("published=%.2f main=%.2f pid=%.2f\n", published, mainCurve, pidOut);
  return 0;
}
"""


def _fn(src: str, sig: str) -> str:
    m = re.search(r"^" + sig + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(f"{sig} not found in {SAT_INO}")
    return m.group(0)


def extract(src: str) -> str:
    curve = _fn(src, r"static float satCalcHeatingCurve\(")
    excl = _fn(src, r"static inline float satZonePidExclude\(")
    usable = _fn(src, r"static bool satZoneUsable\(")
    kp = _fn(src, r"static float satZoneKp\(")
    step = _fn(src, r"static float satZonePidOutput\(")
    a = src.index("  // --- Calculate heating curve ---")
    b = src.index("  // Task #21: Restore original deadband", a)
    loop = src[a:b]
    return (curve + "\n" + excl + "\n" + usable + "\n" + kp + "\n" + step + "\n"
            "static float tick(float effectiveTarget, float outsideTemp) {\n"
            "  float roomTemp = effectiveTarget - 0.5f;\n" + loop + "\n  return pidOutput;\n}\n")


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


def compile_cpp(cpp: Path, exe: Path):
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = _vcvars()
    if bat:
        cmd = f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc /Fe:"{exe}" /Fo:"{cpp.parent}\\\\" "{cpp}"'
        r = subprocess.run(cmd, shell=True, cwd=cpp.parent, capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("host compile failed:\n" + r.stdout[-3000:] + r.stderr[-1000:])
        return exe
    return None


class SatPublishedCurve(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + extract(cls.src) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_tick(self, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        m = re.match(r"published=(\S+) main=(\S+) pid=(\S+)", out)
        self.assertIsNotNone(m, out)
        return float(m.group(1)), float(m.group(2)), float(m.group(3))

    def test_two_zones_publish_the_main_curve(self):
        # main target 20, zones 22 and 18: their curves differ from the main curve by about +/-3.2
        published, main, _ = self.run_tick(2, 20.0, 5.0, 22.0, 18.0)
        print(f"\n  2 zones: published={published:.2f} main={main:.2f}")
        self.assertAlmostEqual(published, main, places=2)

    def test_zone_order_does_not_change_the_published_curve(self):
        published, main, _ = self.run_tick(2, 20.0, 5.0, 18.0, 22.0)
        self.assertAlmostEqual(published, main, places=2)

    def test_single_zone_publishes_the_main_curve(self):
        published, main, _ = self.run_tick(1, 21.0, 0.0)
        self.assertAlmostEqual(published, main, places=2)

    def test_zones_still_drive_the_requested_setpoint(self):
        # Display-only fix: the zone aggregation still sets pidOutput (zone curve 22 > main curve 20).
        _, main, pid = self.run_tick(2, 20.0, 5.0, 22.0, 18.0)
        self.assertGreater(pid, main + 1.0)

    def test_publishers_read_the_state_field(self):
        self.assertIn('F("sat/heating_curve"),  state.sat.fHeatingCurveValue', self.src)
        self.assertIn('F("heating_curve"),        snap->st.sat.fHeatingCurveValue', self.src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
