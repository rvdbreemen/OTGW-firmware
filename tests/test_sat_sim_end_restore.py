#!/usr/bin/env python3
"""TASK-1952: when a SAT simulation run ends, the real PID state and heating-curve recommendation come back.

Since TASK-1900 the PID and HCR files are not written during a simulation run, so they hold the real state; but the RAM
state after the run was the simulated one, and the next 5-minute PID save and the next HCR day commit wrote it to flash.
satSimEndRestore() reloads both the way a boot does when the simulation flag goes from on to off: satPidReset() then
satLoadPidState(), satHCRLoadState() and a recomputed recommendation. Entering a run does nothing.

REAL code: satSimEndRestore() (markers) cut from src/OTGW-firmware/SATcontrol.ino and compiled on the host; the reload
functions record their calls. A source check makes sure satControlLoopRun() calls it before its first early return.
SimEndRestoreBehaviour runs on old AND new code: the real PID persistence and PID state code (the build pieces of
tests/test_sat_pid_persist.py, real files in a temp directory) plus the real head of satControlLoopRun() up to
satRetryRelease(): save the real state, a simulation run moves the integral, the run ends; RAM must hold the saved
values. On the code before TASK-1952 it fails on the values (I=7.77, the simulated integral), not on a missing symbol.
Old-vs-fix: OTGW_SAT_CONTROL (unit class) and OTGW_SAT_INO (behaviour class) point at another copy.
Run: python tests/test_sat_sim_end_restore.py
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

CONTROL = Path(os.environ.get("OTGW_SAT_CONTROL", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
SRC = CONTROL.read_text(encoding="utf-8").replace("\r\n", "\n")


def block():
    a = SRC.index("// --- satSimEndRestore begin")
    return SRC[a:SRC.index("// --- satSimEndRestore end", a)]


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#define F(s) (s)
#define SATDebugTln(s) ((void)0)
struct { struct { bool bSimulation = false; } sat; } settings;
static char g_log[256] = "";
static void note(const char* s) { strcat(g_log, s); strcat(g_log, ","); }
static void satPidReset() { note("reset"); }
static uint8_t satLoadPidState() { note("loadpid"); return 1; }
static void satHCRLoadState() { note("loadhcr"); }
static const char* satHeatingCurveRecommendation() { note("rec"); return "ok"; }
"""

MAIN = r"""
// argv[1]: a sequence of the simulation flag per pass, e.g. "0110" = off, on, on, off
int main(int argc, char** argv) {
  for (const char* p = argv[1]; *p; p++) {
    settings.sat.bSimulation = *p == '1';
    satSimEndRestore();
    strcat(g_log, "|");
  }
  printf("%s\n", g_log);
  return 0;
}
"""


class SimEndRestore(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        body = block()
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_seq(self, seq):
        if not self.exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(self.exe), seq], capture_output=True, text=True, check=True).stdout.strip()
        print(seq, out)
        return out.split("|")[:-1]

    def test_the_end_of_a_run_reloads_pid_and_hcr_once_in_boot_order(self):
        passes = self.run_seq("01100")
        self.assertEqual(passes, ["", "", "", "reset,loadpid,loadhcr,rec,", ""])

    def test_entering_a_run_and_never_simulating_reload_nothing(self):
        self.assertEqual(self.run_seq("0000"), ["", "", "", ""])
        self.assertEqual(self.run_seq("011"), ["", "", ""])

    def test_a_unit_that_boots_with_simulation_on_reloads_when_it_ends(self):
        # first pass already in simulation (the flag was persisted): the end still reloads the real files
        self.assertEqual(self.run_seq("110"), ["", "", "reset,loadpid,loadhcr,rec,"])

    def test_two_runs_reload_after_each(self):
        self.assertEqual(self.run_seq("10101").count("reset,loadpid,loadhcr,rec,"), 2)

    def test_called_before_the_first_early_return_of_the_control_loop(self):
        a = SRC.index("static void satControlLoopRun()\n{")   # the definition, not the forward declaration
        body = SRC[a:SRC.index("\n}\n", a)]
        call = body.index("satSimEndRestore();")
        self.assertLess(call, body.index("return;"), "it must run on every pass, also while SAT is off")


import test_sat_pid_persist as pp  # noqa: E402  (its env OTGW_SAT_INO / OTGW_SATPID_INO pick old-vs-fix copies)

LOOP_MAIN = r"""
static int g_hcrLoads = 0, g_recs = 0;
static void satApplyControlStateReset() {}
static void satCurveTuneService(uint32_t) {}
static void satRetryRelease() {}
static void satHCRLoadState() { g_hcrLoads++; }
static const char* satHeatingCurveRecommendation() { g_recs++; return "ok"; }
@SIMEND@
// the head of the REAL satControlLoopRun(), up to and including satRetryRelease() (present in old and new code)
static void loopHead() {
@HEAD@
}
int main() {
  const long long wall0 = 1800000000LL;
  // 1. the real state, saved before the run (simulation off)
  g_ms = 1000000; g_now = wall0;
  _pid_integral = 1.25f; _pid_rawDerivative = 0.02f; _pid_lastRoomTemp = 20.7f; _pid_lastError = 0.3f; _pid_initialized = true;
  satSavePidState();
  loopHead();
  // 2. a simulation run: the PID learns on the simulated room (no save in between: TASK-1900 is tested elsewhere)
  settings.sat.bSimulation = true;
  g_ms += 1000; loopHead();
  _pid_integral = 7.77f; _pid_rawDerivative = 0.5f;
  g_ms += 1000; loopHead();
  // 3. the run ends
  settings.sat.bSimulation = false;
  g_ms += 1000; g_now = wall0 + 600; loopHead();
  printf("I=%.4f RD=%.4f HCR=%d REC=%d\n", _pid_integral, _pid_rawDerivative, g_hcrLoads, g_recs);
  return 0;
}
"""


def loop_cpp():
    sat = pp.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    pid = pp.PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    persist = pp._between(sat, "//=== PID State Persistence", "// --- satPwmFlag begin")
    pidvars = pp._between(pid, "static float  _pid_lastError", "// The PID moves on its own 60 s timer")
    pidfuncs = pp._between(pid, "//=== PID rounding (entry 95, Q28a)", "//=== Reset of the integral and the raw derivative")
    r0 = pid.index("//=== PID Reset ===")
    reset = pid[r0:pid.index("\n}\n", r0) + 3]
    simend = ""
    if "// --- satSimEndRestore begin" in sat:
        a = sat.index("// --- satSimEndRestore begin")
        simend = sat[a:sat.index("// --- satSimEndRestore end", a)]
    h = sat.index("static void satControlLoopRun()\n{")
    head = sat[sat.index("{", h) + 1:sat.index("satRetryRelease();", h) + len("satRetryRelease();")]
    stubs = pp.STUBS.replace(
        "struct SatState { float fKd = 0,",
        "struct SatState { float fPidP = 0, fPidOutput = 0, fKp = 0, fKi = 0, fKd = 0,")
    if "_pid_startupPending" not in pidvars:   # declared after the cut range in SATpid.ino; satPidReset() sets it
        pidvars += "\nstatic bool _pid_startupPending = true;\n"
    stubs = stubs.replace("@PIDVARS@", pidvars).replace("@PIDFUNCS@", pidfuncs + "\n" + reset).replace("@PERSIST@", persist)
    return stubs + LOOP_MAIN.replace("@SIMEND@", simend).replace("@HEAD@", head)


class SimEndRestoreBehaviour(unittest.TestCase):
    """Old AND new code: a simulation run that moved the PID integral must leave the real one behind when it ends.
    On the code before TASK-1952 the loop head has no restore, so the simulated 7.77 stays (fails on the value)."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "loop.cpp"
        cpp.write_text(loop_cpp(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("loop.exe" if os.name == "nt" else "loop"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_after_a_run_ram_holds_the_saved_real_pid_state_and_the_hcr_is_reloaded(self):
        if not self.exe:
            self.skipTest("no host compiler")
        work = tempfile.mkdtemp(dir=self.tmp.name)
        out = subprocess.run([str(self.exe)], capture_output=True, text=True, check=True, cwd=work).stdout.strip()
        print(out)
        r = dict(kv.split("=") for kv in out.split())
        self.assertEqual((r["I"], r["RD"]), ("1.2500", "0.0200"), "the real integral and raw derivative from the file")
        self.assertEqual((r["HCR"], r["REC"]), ("1", "1"), "the heating-curve file is reloaded and the text recomputed")


if __name__ == "__main__":
    unittest.main(verbosity=2)
