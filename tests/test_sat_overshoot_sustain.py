#!/usr/bin/env python3
"""TASK-1367: host test for the timing of the sustained-overshoot switch (continuous -> PWM).

Sergeant D entry 41 (docs/sat/sergeant-d-corrections.md): continuous switches to PWM once the overshoot has
lasted 60 s, counted from the first sample in overshoot as thermo-nova does (heating_control.py:262-305,
helpers.py:51-56 at pin 7beaaf5: the first qualifying sample starts the timer at 0 s, any sample that does
not qualify clears it).

The code under test is NOT re-implemented here. The test cuts the real sustain state, the SAT_* sustain
constants, satCycleResetSustainGuards(), satCycleCheckAutoSwitch() and (when present)
satCycleSkipAutoSwitch() out of src/OTGW-firmware/SATcycles.ino and compiles them with a host C++
compiler (compile_cpp from test_sat_pwm_override). Without a compiler the compiled cases are skipped.

OTGW_SATCYCLES_INO / OTGW_SAT_INO point at other copies (e.g. `git show HEAD:...`) for old-vs-fix runs.
On HEAD the first sample already counts one whole control interval, a gap above 60 s disables the switch,
and a tick that skips the switch leaves the old count in place, so cases 1-7 fail there. Case 8
(underheat) must give the same answer on HEAD and on the fix.

Run: python tests/test_sat_overshoot_sustain.py
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

FW = REPO / "src" / "OTGW-firmware"
CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", FW / "SATcycles.ino"))
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define PSTR(x) x
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
enum SATControlMode { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
struct SatSettings { float fOvershootMargin = 2.0f; };
struct Settings { SatSettings sat; };
struct SatState { SATControlMode eControlMode = SAT_MODE_CONTINUOUS; bool bDhwActive = false, bPwmFlameRequested = false; };
struct State { SatState sat; };
static Settings settings; static State state;
static uint32_t g_ms = 1000000UL; static float g_flow = 49.0f;
static uint32_t millis() { return g_ms; }
static float satGetFlowTemp() { return g_flow; }
static uint32_t g_offTime = 1;
static uint32_t satPwmLastOffTimeMs() { return g_offTime; }
static int g_switches = 0;
static void satSetControlMode(SATControlMode m) { if (m != state.sat.eControlMode) g_switches++; state.sat.eControlMode = m; }
"""

# Tokens "<dt_ms>:<kind>". The clock advances by dt_ms, then:
#   o  continuous, flow 55 (overshoot: 55 > 50 + 2), switch evaluated
#   n  continuous, flow 49 (no overshoot), switch evaluated
#   p  the mode is PWM for this tick (OFF phase), flow 55, switch evaluated
#   b  the tick bypasses the switch (DHW, fault, ...): satCycleSkipAutoSwitch() when the code has it
#   u  PWM ON phase, flow 40 (underheat: 40 < 50 - 3), switch evaluated
#   z  PWM ON phase, flow 50 (no underheat), PWM off time 0 (saturation counts), switch evaluated
# Prints the time since the start of the first mode change ("t=<ms>"), or t=-1, plus the saturation timer.
MAIN = r"""
int main(int argc, char** argv) {
  const uint32_t t0 = g_ms;
  _sustain_lastCheckMs = g_ms;              // the previous control tick ran at t=0
  for (int i = 1; i < argc; i++) {
    const char* tok = argv[i];
    const char* colon = strchr(tok, ':');
    if (!colon) return 2;
    g_ms += (uint32_t)strtoul(tok, 0, 10);
    const char k = colon[1];
    if (k == 'b') {
#ifdef HAS_SKIP
      satCycleSkipAutoSwitch();
#endif
      continue;
    }
    if (k == 'o' || k == 'n') { if (state.sat.eControlMode != SAT_MODE_CONTINUOUS) state.sat.eControlMode = SAT_MODE_CONTINUOUS;
                                state.sat.bPwmFlameRequested = false; g_flow = (k == 'o') ? 55.0f : 49.0f; g_offTime = 1; }
    if (k == 'p') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = false; g_flow = 55.0f; g_offTime = 1; }
    if (k == 'u') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 40.0f; g_offTime = 1; }
    if (k == 'z') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 50.0f; g_offTime = 0; }
    const int before = g_switches;
    satCycleCheckAutoSwitch(50.0f);
    if (g_switches != before) { printf("t=%lu mode=%d sat=%.0f\n", (unsigned long)(g_ms - t0), (int)state.sat.eControlMode, _sustain_saturationSec); return 0; }
  }
  printf("t=-1 mode=%d sat=%.0f\n", (int)state.sat.eControlMode, _sustain_saturationSec);
  return 0;
}
"""


def _fn(src: str, head: str) -> str:
    m = re.search(r"^" + re.escape(head) + r"\s*\{.*?^\}[ \t]*$", src, re.S | re.M)
    return m.group(0) if m else ""


def extract(src: str):
    """Returns (flag line, C++ text)."""
    consts = re.findall(r"^static const \w+\s+SAT_(?:OVERSHOOT|UNDERSHOOT|UNDERHEAT|SATURATION|DHW)\w*\s*=[^;]*;", src, re.M)
    a = src.index("// --- Sustained State Detection ---")
    b = src.index("// --- EMA Fractions", a)
    check = _fn(src, "bool satCycleCheckAutoSwitch(float requestedSetpoint)")
    if not check:
        raise AssertionError("satCycleCheckAutoSwitch(float) not found in %s" % CYCLES_INO)
    skip = _fn(src, "void satCycleSkipAutoSwitch()")
    out = "\n".join(consts) + "\n" + src[a:b] + "\n" + _fn(src, "void satCycleResetSustainGuards()") + "\n"
    flag = "#define HAS_SKIP 1\n" if skip else ""   # HEAD has no bypass helper
    return flag, out + skip + "\n" + check + "\n"


def ticks(dt, kinds):
    return ["%d:%s" % (dt, k) for k in kinds]


class OvershootSustain(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        flag, body = extract(src)
        cpp.write_text(flag + STUBS + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_ticks(self, toks):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), *toks], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def switch_at(self, toks):
        return int(self.run_ticks(toks)["t"])

    # Case 1: 30 s control tick, overshoot from T0 = 30 s. thermo-nova: T0 starts the timer, switch once 60 s passed.
    def test_1_thirty_second_tick_switches_after_60s_of_overshoot(self):
        t0 = 30000
        self.assertEqual(self.switch_at(ticks(30000, "oooooo")), t0 + 60000)

    # Case 2: 10 s tick.
    def test_2_ten_second_tick_switches_after_60s(self):
        t0 = 10000
        self.assertEqual(self.switch_at(ticks(10000, "o" * 12)), t0 + 60000)

    # Case 3: a dip below the threshold clears the start (heating_control.py:288-291).
    def test_3_dip_restarts_the_timer(self):
        restart = 90000                         # o@30 n@60 o@90 ...
        self.assertEqual(self.switch_at(ticks(30000, "onoooo")), restart + 60000)

    # Case 4: 120 s control interval (settingStuff allows 61-300 s): the second qualifying tick is 120 s in.
    # Behaviour change: HEAD never switches above a 60 s interval.
    def test_4_long_interval_still_switches(self):
        t0 = 120000
        self.assertEqual(self.switch_at(ticks(120000, "ooo")), t0 + 120000)

    # Case 5: one tick in PWM clears the start (thermo-nova :264-266); counting restarts on the return.
    def test_5_pwm_tick_clears_the_start(self):
        back = 90000                            # o@30 p@60 o@90 ...
        self.assertEqual(self.switch_at(ticks(30000, "opoooo")), back + 60000)

    # Case 6: ticks that bypass the switch (DHW, fault, thermal-safe, summer, cold setpoint) clear the start.
    def test_6_bypassed_ticks_clear_the_start(self):
        resume = 150000                         # o@30 b@60 b@90 b@120 o@150 ...
        self.assertEqual(self.switch_at(ticks(30000, "obbboooo")), resume + 60000)

    # Case 7: boundary, 59 999 ms after the start does not switch, 60 000 ms does.
    def test_7_boundary_60000_ms(self):
        start = 30000
        no = self.run_ticks(["30000:o", "29999:o", "30000:o"])
        self.assertEqual(no["t"], "-1", "59 999 ms of overshoot must not switch")
        self.assertEqual(self.switch_at(["30000:o", "29999:o", "30000:o", "1:o"]), start + 60000)

    # Case 8: regression guard, underheat (PWM -> continuous) keeps its dt counting: identical on HEAD and fix.
    def test_8_underheat_counting_unchanged(self):
        r = self.run_ticks(ticks(30000, "u" * 10))
        self.assertEqual((int(r["t"]), r["mode"]), (180000, "1"))

    # Case 9: the switch to PWM resets the underheat and saturation timers, as thermo-nova
    # _reset_pwm_disable_guards() after pwm.enable() (heating_control.py:304-305). 120 s of saturation
    # from an earlier PWM stint (the underheat exit zeroes only underheat) must not carry over.
    def test_9_switch_resets_the_saturation_timer(self):
        r = self.run_ticks(ticks(30000, "zzzz") + ticks(30000, "ooo"))
        self.assertEqual((int(r["t"]), r["mode"], r["sat"]), (210000, "2", "0"))


class BypassSource(unittest.TestCase):
    """The bypass returns of satControlLoopRun() call satCycleSkipAutoSwitch() (anchored on text, not lines)."""

    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.index("static void satControlLoopRun()\n{")
        cls.loop = src[a:src.index("\n}\n", a)]

    def _skip_before_return(self, anchor):
        i = self.loop.index(anchor)
        r = self.loop.index("return", i)
        self.assertIn("satCycleSkipAutoSwitch();", self.loop[i:r], "no satCycleSkipAutoSwitch() between %r and its return" % anchor)

    def test_dhw_return(self):
        self._skip_before_return("    _sat_prevDhwActive = true;")

    def test_fault_return(self):
        self._skip_before_return('boiler fault flag detected')

    def test_thermal_safe_return(self):
        self._skip_before_return("_sat_thermalSafeActive = true;")

    def test_summer_simmer_return(self):
        self._skip_before_return("if (state.sat.bSummerActive && settings.sat.bSummerSimmer) {")

    def test_gate_else_branch(self):
        m = re.search(r"satCycleCheckAutoSwitch\(pidOutput\);\s*\}\s*else\s*\{\s*satCycleSkipAutoSwitch\(\);", self.loop)
        self.assertTrue(m, "the satCycleCheckAutoSwitch(pidOutput) gate has no else branch calling satCycleSkipAutoSwitch()")

    def test_satdisable_clears_the_start(self):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.index("void satDisable()\n{")
        self.assertIn("satCycleSkipAutoSwitch();", src[a:src.index("\n}\n", a)])

    def test_mode_change_clears_the_start(self):
        # Source-text check only: the host harness stubs satSetControlMode(). Covers the control_mode command.
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.index("static void satSetControlMode(SATControlMode m)\n{")
        body = src[a:src.index("\n}\n", a)]
        self.assertRegex(body, r"if \(m != state\.sat\.eControlMode\) \{[^}]*satCycleSkipAutoSwitch\(\);")

    def test_reenable_from_off(self):
        i = self.loop.index("if (state.sat.eControlMode == SAT_MODE_OFF) {")
        self.assertIn("satCycleSkipAutoSwitch();", self.loop[i:self.loop.index("}", i)])


if __name__ == "__main__":
    unittest.main(verbosity=2)
