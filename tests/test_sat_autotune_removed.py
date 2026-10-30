#!/usr/bin/env python3
"""TASK-1413: the hourly heating-curve coefficient auto-tune is removed (Sergeant D entry 72, Q22).

AC1  The auto-tune rewrote settings.sat.fHeatingCurveCoeff once per hour, after at least 6 cycles.
     - Structural: no auto-tune symbol, setting, topic or REST field remains, and the coefficient
       is written only by the settings loader, the settings POST / REST / MQTT path and the wizard.
     - Behavioural: when a source still contains satAutoTuneUpdate(), the REAL function is cut out
       of SATcontrol.ino, compiled behind small stubs and driven with 1 h and 6 mostly-overshoot
       cycles. It must not rewrite the coefficient. On the pre-fix source this is red:
         git show b3ac0ce67:src/OTGW-firmware/SATcontrol.ino > old.ino
         OTGW_SATCONTROL_INO=old.ino python tests/test_sat_autotune_removed.py
       On the fixed source the function does not exist and the behavioural test is skipped.
AC2  An old settings.ini with SATautotune / SATautotunerate loads without error and keeps SATcoefficient
     (updateSetting() ignores unknown keys; the loader reads the file as keys).

Needs a host C++ compiler for the behavioural test (g++/clang++, or MSVC Build Tools); skipped otherwise.
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
INO = Path(os.environ.get("OTGW_SATCONTROL_INO", FW / "SATcontrol.ino"))


def read(p):
    return Path(p).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def find_compiler():
    for exe in ("g++", "clang++"):
        if shutil.which(exe):
            return ("gnu", exe)
    vc = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if os.name == "nt" and os.path.exists(vc):
        return ("msvc", vc)
    return None


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define PSTR(x) x
#define SATDebugTf(...) do {} while (0)
enum { SAT_CYCLE_OVERSHOOT = 1, SAT_CYCLE_UNDERHEAT = 2, SAT_CYCLE_UNDERHEAT_PWM = 3, SAT_CYCLE_GOOD = 4 };
struct SatSettings { bool bAutoTune; float fAutoTuneRate; float fHeatingCurveCoeff; };
struct SatState { bool bActive; uint32_t iCycleCount; int eLastCycleClass; float fCycleOvershootSec;
                  bool bAutoTuneActive; uint16_t iAutoTuneCycles; float fAutoTuneScore; };
struct { SatSettings sat; } settings;
struct { SatState sat; } state;
static uint32_t g_now = 0;
static int g_writes = 0;
static uint32_t millis() { return g_now; }
static void writeSettings(bool) { g_writes++; }
@@FUNCS@@
int main() {
  settings.sat.bAutoTune = true; settings.sat.fAutoTuneRate = 0.02f; settings.sat.fHeatingCurveCoeff = 2.0f;
  state.sat.bActive = true;
  g_now = 1000;
  satAutoTuneUpdate();                       // arms the 1 h window
  const int cls[6] = { SAT_CYCLE_OVERSHOOT, SAT_CYCLE_OVERSHOOT, SAT_CYCLE_OVERSHOOT,
                       SAT_CYCLE_OVERSHOOT, SAT_CYCLE_OVERSHOOT, SAT_CYCLE_GOOD };
  for (int i = 0; i < 6; i++) {
    g_now += 5 * 60 * 1000UL;
    state.sat.iCycleCount++;
    state.sat.eLastCycleClass = cls[i];
    state.sat.fCycleOvershootSec = 120.0f;
    satAutoTuneUpdate();
  }
  g_now += 61 * 60 * 1000UL;                 // past the 1 h window
  satAutoTuneUpdate();
  printf("%.4f %d\n", settings.sat.fHeatingCurveCoeff, g_writes);
  return 0;
}
"""


class NoAutoTuneLeft(unittest.TestCase):
    def test_no_auto_tune_symbol_in_firmware_sources(self):
        # "Smart Autotune Thermostat" is the product name and stays; match only the removed identifiers
        pat = re.compile(r"auto_tune|satautotune|SATautotune|[bfi]AutoTune|AutoTune(Update|Active|Cycles|Score|Rate)|AT_TUNE")
        offenders = []
        for p in sorted(FW.rglob("*")):
            if p.suffix not in (".ino", ".h", ".cpp", ".js", ".html", ".css") or "node_modules" in p.parts:
                continue
            for n, line in enumerate(read(p).split("\n"), 1):
                if pat.search(line) and "RETIRED-SAT-ENTITY" not in line:
                    offenders.append("%s:%d: %s" % (p.name, n, line.strip()[:100]))
        self.assertEqual(offenders, [])

    def test_retained_topics_are_cleared(self):
        ha = read(FW / "MQTTHaDiscovery.cpp")
        rows = [l for l in ha.splitlines() if "RETIRED-SAT-ENTITY" in l and "TASK-1413" in l]
        joined = "\n".join(rows)
        self.assertIn('"switch", "sat_auto_tune_enable", "/sat/auto_tune_enable"', joined)
        self.assertIn('"/sat/auto_tune"', joined)
        self.assertIn('"/sat/auto_tune_rate"', joined)

    def test_coefficient_has_only_the_allowed_writers(self):
        # The coefficient is assigned in the settings loader (updateSetting), the wizard and the
        # REST/MQTT paths that call updateSetting(); no control-loop file assigns it.
        allowed = {"settingStuff.ino", "OTGW-firmware.ino"}
        writers = []
        for p in sorted(FW.glob("*.ino")) + sorted(FW.glob("*.cpp")):
            for n, line in enumerate(read(p).split("\n"), 1):
                if re.search(r"settings\.sat\.fHeatingCurveCoeff\s*=[^=]", line):
                    writers.append((p.name, n))
        self.assertTrue(writers, "expected the settings loader to write the coefficient")
        self.assertEqual([w for w in writers if w[0] not in allowed], [], writers)

    def test_old_keys_are_ignored_by_the_settings_loader(self):
        src = read(FW / "settingStuff.ino")
        body = src[src.index("void updateSetting("):src.index("} // updateSetting()")]
        self.assertNotIn("SATautotune", body)
        self.assertIn('"SATcoefficient"', body)
        self.assertNotRegex(body, r"(?i)unknown (setting|key|field)")
        self.assertNotIn("SATautotune", read(FW / "settingStuff.ino"))


class RealFunctionNeverRewritesCoefficient(unittest.TestCase):
    def test_one_hour_and_six_overshoot_cycles(self):
        src = read(INO)
        m = re.search(r"^// Tracking state \(static", src, re.M)
        if not m:
            self.skipTest("satAutoTuneUpdate() no longer exists in " + str(INO))
        cc = find_compiler()
        if not cc:
            self.skipTest("no host C++ compiler found")
        end = src.index("\n}\n", src.index("static void satAutoTuneUpdate()")) + 3
        consts = re.search(r"^// Gain clamp ranges.*?(?=^// Tracking state)", src, re.M | re.S)
        funcs = (consts.group(0) if consts else "") + src[m.start():end]
        code = HARNESS.replace("@@FUNCS@@", funcs)
        tmp = Path(tempfile.mkdtemp(prefix="otgw_at_"))
        try:
            cpp = tmp / "h.cpp"
            cpp.write_text(code)
            exe = tmp / "h.exe"
            if cc[0] == "gnu":
                r = subprocess.run([cc[1], "-std=c++17", str(cpp), "-o", str(exe)], capture_output=True, text=True)
            else:
                bat = tmp / "b.bat"
                bat.write_text('@call "%s" >nul 2>&1\r\ncl /nologo /std:c++17 /EHsc /Fe:h.exe h.cpp\r\n' % cc[1])
                r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True, text=True, cwd=tmp)
            self.assertTrue(exe.exists(), "harness build failed:\n" + r.stdout + r.stderr)
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.split()
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        coeff, writes = float(out[0]), int(out[1])
        print("\n  coefficient after 1 h + 6 mostly-overshoot cycles: %.4f (settings writes: %d)" % (coeff, writes))
        self.assertEqual(coeff, 2.0)
        self.assertEqual(writes, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
