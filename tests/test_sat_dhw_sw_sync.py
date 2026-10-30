#!/usr/bin/env python3
"""TASK-1376: SAT sends SW=<int> when the DHW setpoint changes and once after a restart, not at the start of a draw.

Sergeant D entries 33, 53, 69, 121, 150 (docs/sat/sergeant-d-corrections.md); thermo-nova number.py:72-74 is its only SW sender.

Nothing is re-implemented: satDhwSetpointSync() is cut out of SATcontrol.ino and compiled with a host C++ compiler; the
harness runs one control pass per call, with the setpoint, DHW control and SAT state set per scenario. The placement in
satControlLoopRun() (after satRestartStep(), before the control tick, reset while SAT is off) and the removal of the
draw-edge send are checked on the source text. On HEAD the function does not exist and the draw-edge send does.
OTGW_SAT_INO points at another copy of SATcontrol.ino for old-vs-fix.

Run: python tests/test_sat_dhw_sw_sync.py
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

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
SRC = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#define SAT_DHW_SETPOINT_MIN 30
#define SAT_DHW_SETPOINT_MAX 60
#define PSTR(x) x
#define snprintf_P snprintf
#define SATDebugTf(...) ((void)0)
struct SatSettings { bool bDhwEnabled = false; float fDhwSetpoint = 0; };
struct Settings { SatSettings sat; };
static Settings settings;
static bool g_cmdif = true;
static bool hasOTCommandInterface() { return g_cmdif; }
static int g_pass = 0;
static void addCommandToQueue(const char* cmd, int, bool, int) { printf("%d %s\n", g_pass, cmd); }
static int _sat_lastSentSW = 0;
static bool _sat_swSentValid = false;
@FUNC@
int main(int argc, char** argv) {
  // argv: scenario
  const char* sc = argv[1];
  settings.sat.bDhwEnabled = true; settings.sat.fDhwSetpoint = 50.0f;
  if (!strcmp(sc, "dhw_off")) settings.sat.bDhwEnabled = false;
  if (!strcmp(sc, "invalid")) settings.sat.fDhwSetpoint = 70.0f;
  if (!strcmp(sc, "nocmd")) g_cmdif = false;
  for (g_pass = 1; g_pass <= 6; g_pass++) {
    if (!strcmp(sc, "change") && g_pass == 3) settings.sat.fDhwSetpoint = 52.0f;
    if (!strcmp(sc, "change_fraction") && g_pass == 3) settings.sat.fDhwSetpoint = 50.4f;   // same int: no new SW=
    if (!strcmp(sc, "dhw_off") && g_pass == 3) settings.sat.fDhwSetpoint = 52.0f;
    if (!strcmp(sc, "off_on") && g_pass == 3) settings.sat.bDhwEnabled = false;
    if (!strcmp(sc, "off_on") && g_pass == 5) settings.sat.bDhwEnabled = true;
    if (!strcmp(sc, "sat_off_on") && g_pass == 4) _sat_swSentValid = false;   // the disabled branch of satControlLoopRun()
    satDhwSetpointSync();
  }
  return 0;
}
"""


def _function(src, name):
    i = src.index("static void %s()" % name)
    j = src.index("\n}\n", i) + 3
    return src[i:j]


class Harness:
    dir = None
    exe = None

    @classmethod
    def setup(cls):
        if cls.dir is not None:
            return
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(STUBS.replace("@FUNC@", _function(SRC, "satDhwSetpointSync")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")


def run(sc):
    Harness.setup()
    if not Harness.exe:
        raise unittest.SkipTest("no host C++ compiler")
    out = subprocess.run([str(Harness.exe), sc], capture_output=True, text=True, check=True).stdout
    return [(int(a), b) for a, b in (l.split() for l in out.splitlines())]


class SwSync(unittest.TestCase):
    def test_change_sends_one_sw_in_the_same_pass(self):
        # 50 on the first pass, 52 at pass 3: SW=50 once, SW=52 once, in the pass of the change (no control tick needed).
        self.assertEqual(run("change"), [(1, "SW=50"), (3, "SW=52")])

    def test_fresh_state_sends_once_after_start(self):
        cmds = run("fresh")
        self.assertEqual(cmds, [(1, "SW=50")])

    def test_dhw_control_off_sends_nothing_even_on_change(self):
        self.assertEqual(run("dhw_off"), [])

    def test_invalid_setpoint_sends_nothing(self):
        self.assertEqual(run("invalid"), [])

    def test_no_command_interface_sends_nothing(self):
        self.assertEqual(run("nocmd"), [])

    def test_same_integer_is_not_resent(self):
        self.assertEqual(run("change_fraction"), [(1, "SW=50")])

    def test_dhw_control_switched_on_again_resends(self):
        self.assertEqual(run("off_on"), [(1, "SW=50"), (5, "SW=50")])

    def test_sat_switched_off_and_on_resends(self):
        self.assertEqual(run("sat_off_on"), [(1, "SW=50"), (4, "SW=50")])


class Placement(unittest.TestCase):
    def test_sw_has_one_sender_and_it_is_the_sync(self):
        # Entry 150: no SW= at the start of a draw. The only SW= format string lives in satDhwSetpointSync().
        self.assertEqual(SRC.count('PSTR("SW=%d")'), 1)
        self.assertIn('PSTR("SW=%d")', _function(SRC, "satDhwSetpointSync"))

    def test_dhw_active_branch_sends_no_sw(self):
        i = SRC.index("  if (state.sat.bDhwActive) {\n    // DHW has priority")
        block = SRC[i:SRC.index("    return;\n  }\n", i)]
        self.assertNotIn("SW=%d", block)
        self.assertNotIn("addCommandToQueue", block)

    def test_sync_runs_every_pass_after_restart_step_before_the_tick(self):
        run_body = SRC[SRC.index("static void satControlLoopRun()\n{"):]
        a = run_body.index("if (satRestartStep()) return;")
        b = run_body.index("  satDhwSetpointSync();")
        c = run_body.index("if (!DUE(timerSATControl)) return;")
        self.assertTrue(a < b < c)
        self.assertNotIn("DUE(", run_body[b - 200:b])   # not inside a timer gate

    def test_disabled_branch_resets_the_cache(self):
        run_body = SRC[SRC.index("static void satControlLoopRun()\n{"):]
        i = run_body.index("if (!satIsEnabled() || isFlashing()) {")
        self.assertIn("_sat_swSentValid = false;", run_body[i:run_body.index("return;", i)])

    def test_update_setting_does_not_send(self):
        setting = (REPO / "src" / "OTGW-firmware" / "settingStuff.ino").read_text(encoding="utf-8", errors="replace")
        self.assertNotIn("SW=", setting)
        branch = [l for l in setting.splitlines() if 'PSTR("SATdhwsetpoint")' in l and "strcasecmp_P" in l]
        self.assertEqual(len(branch), 1)
        self.assertNotIn("addCommandToQueue", branch[0])


if __name__ == "__main__":
    unittest.main()
