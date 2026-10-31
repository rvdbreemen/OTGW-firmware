#!/usr/bin/env python3
"""TASK-1367 / TASK-1420: host test for the sustained switches of SAT (SATcycles.ino).

Sergeant D entries 41, 86 (Q43 A, Q44 A, Q46 B), 88 (Q47 A), 95 (Q46b) and 108 (docs/sat/sergeant-d-corrections.md):
the sustained overshoot (continuous -> PWM, 60 s), underheat (PWM -> continuous, 180 s) and saturation
(PWM -> continuous, 300 s) are timed as thermo-nova does (heating_control.py:262-372, helpers.py:51-56 at pin
7beaaf5): the first qualifying sample starts the timer at 0 s, a sample that does not qualify clears it, and
the clock is one time per control tick (one millis() read in the control loop), not a per-sample millis().

The code under test is NOT re-implemented here. The test cuts the real sustain state, the SAT_* sustain
constants, the helpers, satCycleResetSustainGuards(), satCycleSkipAutoSwitch(), satCycleNoteDhwTick(),
satCycleCheckOvershootSwitch() and satCycleCheckPwmExits() out of src/OTGW-firmware/SATcycles.ino and compiles
them with a host C++ compiler (compile_cpp from test_sat_pwm_override). Without a compiler the compiled cases
are skipped.

OTGW_SATCYCLES_INO / OTGW_SAT_INO point at other copies (e.g. `git show HEAD:...`) for old-vs-fix runs. The old
code has one function, satCycleCheckAutoSwitch(float), that reads millis(); the harness then calls it with
millis() set to the tick time, so the old code fails the behavioural cases (fire time, equality, DHW guard),
not on a missing symbol.

Tokens "<dt_ms>:<kind>[@<late_ms>]": the tick clock advances by dt_ms plus late_ms (the tick ran late; under
SKIP_MISSED_TICKS the lateness carries into the following ticks, each tick = previous + interval + lateness),
then the tick runs:
  o  continuous, flow 55 (overshoot: 55 >= 50 + 2)           e  continuous, flow 52 exactly requested + margin
  n  continuous, flow 49 (no overshoot)                      p  PWM OFF phase, flow 55
  u  PWM ON phase, flow 40 (underheat: 40 <= 50 - 3)         U  PWM ON phase, flow 47 exactly requested - 3
  z  PWM ON phase, flow 50, PWM off time 0 (saturation)      b  the tick bypasses the switches (fault, ...)
  w  PWM ON phase, flow 50, PWM off time 1 (neither)
  d  a hot-water tick (satCycleNoteDhwTick)
Prints the tick time since the start of the first mode change ("t=<ms>"), or t=-1.

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
SETTINGS_INO = Path(os.environ.get("OTGW_SETTINGS_INO", FW / "settingStuff.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define PSTR(x) x
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
// TASK-1521 log helpers
static void satNarrate_P(const char*) {}
static void satNarratef_P(const char*, ...) {}
static bool _sat_followNarrPending = false;
static bool _sat_narrAtMax = false;
static bool _sat_narrCold = false;
static bool _sat_bootBoilerLogged = false;
static bool _sat_bootOutsideLogged = false;
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

MAIN = r"""
static void run_checks(float req, uint32_t tick) {
#ifdef HEAD_API
  (void)tick;
  satCycleCheckAutoSwitch(req);            // old code: one function, reads millis() (= tick + lateness)
#else
  satCycleCheckOvershootSwitch(req, tick); // the control loop order: overshoot switch, (apply), exits
  satCycleCheckPwmExits(req, tick);
#endif
}

int main(int argc, char** argv) {
  if (argc > 1 && !strncmp(argv[1], "start=", 6)) { g_ms = (uint32_t)strtoul(argv[1] + 6, 0, 10); argv++; argc--; }
  uint32_t tick = g_ms;
  const uint32_t t0 = tick;
#ifdef HEAD_API
  _sustain_lastCheckMs = g_ms;              // old code: the previous control tick ran at t=0
#endif
  for (int i = 1; i < argc; i++) {
    const char* tok = argv[i];
    const char* colon = strchr(tok, ':');
    if (!colon) return 2;
    const char* at = strchr(colon, '@');
    tick += (uint32_t)strtoul(tok, 0, 10) + (at ? (uint32_t)strtoul(at + 1, 0, 10) : 0UL);
    g_ms = tick;
    const char k = colon[1];
    if (k == 'b') {
#ifdef HAS_SKIP
      satCycleSkipAutoSwitch();
#endif
      continue;
    }
    if (k == 'd') {
      state.sat.bDhwActive = true;
#if defined(HAS_NOTE)
      satCycleNoteDhwTick(tick);
#elif defined(HAS_SKIP)
      satCycleSkipAutoSwitch();            // old control loop: the DHW branch only skipped
#endif
      state.sat.bDhwActive = false;
      continue;
    }
    if (k == 'o' || k == 'n' || k == 'e') { state.sat.eControlMode = SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = false;
                                            g_flow = (k == 'o') ? 55.0f : (k == 'e') ? 52.0f : 49.0f; g_offTime = 1; }
    if (k == 'p') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = false; g_flow = 55.0f; g_offTime = 1; }
    if (k == 'u') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 40.0f; g_offTime = 1; }
    if (k == 'U') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 47.0f; g_offTime = 1; }
    if (k == 'w') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 50.0f; g_offTime = 1; }
    if (k == 'z') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 50.0f; g_offTime = 0; }
    const int before = g_switches;
    run_checks(50.0f, tick);
    if (g_switches != before) {
#ifdef HEAD_API
      const int satrun = _sustain_saturationSec > 0.0f;
#else
      const int satrun = _sustain_saturationRunning;
#endif
      printf("t=%lu mode=%d satrun=%d\n", (unsigned long)(tick - t0), (int)state.sat.eControlMode, satrun);
      return 0;
    }
  }
  printf("t=-1 mode=%d satrun=0\n", (int)state.sat.eControlMode);
  return 0;
}
"""


def _fn(src: str, head: str) -> str:
    m = re.search(r"^" + re.escape(head) + r"\s*\{.*?^\}[ \t]*$", src, re.S | re.M)
    return m.group(0) if m else ""


def extract(src: str):
    """Returns (flag lines, C++ text). Raises when the expected functions are missing."""
    consts = re.findall(r"^static const \w+\s+SAT_(?:OVERSHOOT|UNDERSHOOT|UNDERHEAT|SATURATION|DHW)\w*\s*=[^;]*;", src, re.M)
    if not consts:
        raise AssertionError("no SAT_* sustain constants found in %s" % CYCLES_INO)
    new_api = "bool satCycleCheckOvershootSwitch(" in src
    a = src.index("// --- Sustained State Detection ---")
    if new_api:
        b = src.index("// --- Cycle History ---", a)
        ovs = _fn(src, "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)")
        ext = _fn(src, "bool satCycleCheckPwmExits(float requestedSetpoint, uint32_t tickMs)")
        if not ovs or not ext:
            raise AssertionError("satCycleCheckOvershootSwitch / satCycleCheckPwmExits not found in %s" % CYCLES_INO)
        if "void satCycleNoteDhwTick(uint32_t tickMs)" not in src or "void satCycleSkipAutoSwitch()" not in src:
            raise AssertionError("satCycleNoteDhwTick / satCycleSkipAutoSwitch not found in %s" % CYCLES_INO)
        return "#define HAS_SKIP 1\n#define HAS_NOTE 1\n", "\n".join(consts) + "\n" + src[a:b] + "\n" + ovs + "\n" + ext + "\n"
    b = src.index("// --- EMA Fractions", a)
    check = _fn(src, "bool satCycleCheckAutoSwitch(float requestedSetpoint)")
    if not check:
        raise AssertionError("neither satCycleCheckOvershootSwitch nor satCycleCheckAutoSwitch(float) found in %s" % CYCLES_INO)
    skip = _fn(src, "void satCycleSkipAutoSwitch()")
    out = "\n".join(consts) + "\n" + src[a:b] + "\n" + _fn(src, "void satCycleResetSustainGuards()") + "\n"
    flag = "#define HEAD_API 1\n" + ("#define HAS_SKIP 1\n" if skip else "")
    return flag, out + skip + "\n" + check + "\n"


def ticks(dt, kinds):
    return ["%d:%s" % (dt, k) for k in kinds]


class SustainedSwitches(unittest.TestCase):
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

    # --- overshoot (continuous -> PWM), 60 s ---

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

    # Q43 A (entry 86): flow exactly on requested + margin counts as overshoot (>=).
    def test_equality_counts_and_switches_at_60s(self):
        self.assertEqual(self.switch_at(ticks(30000, "eeee")), 30000 + 60000)

    # Q43 A: a flow just below the threshold does not count.
    def test_flow_below_threshold_does_not_count(self):
        self.assertEqual(self.run_ticks(ticks(30000, "n" * 12))["t"], "-1")

    # Q46 B (entry 86), entry 109: the timer skips missed ticks, so a late tick pushes the following ticks later
    # (each tick = previous + interval + lateness). The switch comes on the first tick at least 60 s after the
    # first qualifying tick: the 3rd qualifying tick at a 30 s interval, never earlier.
    def test_switch_at_first_tick_at_least_60s_after_the_start_when_ticks_run_late(self):
        # first tick on time (30000), then 8 and 3 ms late: ticks at 30000, 60008, 90011 -> 60011 ms after the start
        self.assertEqual(self.switch_at(["30000:o@0", "30000:o@8", "30000:o@3", "30000:o@0"]), 90011)
        # first tick 8 ms late (30008), the rest on time (drift carries): 60008, 90008 -> exactly 60000 ms
        self.assertEqual(self.switch_at(["30000:o@8", "30000:o@0", "30000:o@0", "30000:o@0"]), 90008)

    # --- underheat (PWM ON phase), 180 s, saturation, 300 s ---

    # Q44 A / Q46b: 180 s after the first qualifying tick, at a 30 s and a 60 s tick (thermo-nova :342-345).
    def test_underheat_fires_180s_after_the_first_tick_at_30s_and_60s_ticks(self):
        for dt in (30000, 60000):
            with self.subTest(tick_s=dt // 1000):
                r = self.run_ticks(ticks(dt, "u" * 12))
                self.assertEqual((int(r["t"]), r["mode"]), (dt + 180000, "1"))

    # Q44 A: the comparison is <= (thermo-nova resets on flow > requested - 3, :337-340): exactly requested - 3 counts.
    def test_underheat_counts_at_exactly_requested_minus_3(self):
        for dt in (30000, 60000):
            with self.subTest(tick_s=dt // 1000):
                self.assertEqual(self.switch_at(ticks(dt, "U" * 12)), dt + 180000)

    # A flow above requested - 3 resets the underheat start (thermo-nova :337-340): u@30 u@60 w@90 u@120 ... fires at 120 + 180.
    def test_underheat_dip_restarts_the_timer(self):
        self.assertEqual(self.switch_at(ticks(30000, "uuwuuuuuuuu")), 30000 * 4 + 180000)

    # Q44 A: no 60 s bail-out. A 61 s tick still counts and fires 180 s after the first tick (4 ticks later: 183 s).
    def test_underheat_61s_tick_still_fires(self):
        self.assertEqual(self.switch_at(ticks(61000, "uuuuu")), 61000 + 183000)

    def test_saturation_fires_300s_after_the_first_tick_at_30s_and_60s_ticks(self):
        for dt in (30000, 60000):
            with self.subTest(tick_s=dt // 1000):
                r = self.run_ticks(ticks(dt, "z" * 14))
                self.assertEqual((int(r["t"]), r["mode"]), (dt + 300000, "1"))

    def test_saturation_61s_tick_still_fires(self):
        self.assertEqual(self.switch_at(ticks(61000, "zzzzzzz")), 61000 + 305000)

    # Case 9: the switch to PWM resets the underheat and saturation timers, as thermo-nova
    # _reset_pwm_disable_guards() after pwm.enable() (heating_control.py:304-305).
    def test_9_switch_resets_the_saturation_timer(self):
        r = self.run_ticks(ticks(30000, "zzzz") + ticks(30000, "ooo"))
        self.assertEqual((int(r["t"]), r["mode"], r["satrun"]), (210000, "2", "0"))

    # Exit timers reset in the PWM OFF phase and when the mode is not PWM (thermo-nova :308-311, :321-323).
    def test_exit_timers_reset_in_the_off_phase(self):
        # u@30 u@60 p@90 (OFF phase: reset) u@120 starts again, fires 180 s later
        self.assertEqual(self.switch_at(ticks(30000, "uupuuuuuuu")), 30000 * 4 + 180000)

    # --- hot-water guard (entry 108, thermo-nova :272-274, :313-315) ---

    # The 300 s guard after a draw arms: overshoot right after the draw does not switch; counting starts once
    # tick - end >= 300 s. d@30 (end = 30 s) ... o@330 starts the timer, o@390 fires.
    def test_dhw_guard_blocks_overshoot_for_300s_after_the_draw(self):
        r = self.run_ticks(["30000:d"] + ticks(30000, "o" * 9))                     # o@60 .. o@300: guarded
        self.assertEqual(r["t"], "-1", "overshoot within 300 s after the draw must not switch")

    def test_dhw_guard_overshoot_exact_window(self):
        # d@30: o@330 is the first unguarded tick (330 - 30 = 300 s), it starts the timer; o@390 fires: t = 390 - 0
        self.assertEqual(self.switch_at(["30000:d"] + ticks(30000, "o" * 14)), 390000 - 0)

    # No draw ever: the guard is inactive from boot, also in the 300 s before the millis() wrap (no boot sentinel
    # that would alias a false "draw end" near 0xFFFFFFFF). Start the tick clock 100 s before the wrap.
    def test_dhw_guard_inactive_without_a_draw_near_the_millis_wrap(self):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), "start=%d" % (0xFFFFFFFF - 100000)] + ticks(30000, "o" * 6),
                             capture_output=True, text=True, check=True).stdout
        r = dict(kv.split("=") for kv in out.split())
        self.assertEqual((int(r["t"]), r["mode"]), (90000, "2"), "overshoot near the wrap switched late or never: %r" % r)

    # A draw also resets running exit timers, and they count only 300 s after it.
    def test_dhw_resets_underheat_and_blocks_it_for_300s(self):
        # u@30 u@60 u@90 d@120 u@150..u@390 guarded (tick - 120 < 300), u@420 starts, fires at 600
        toks = ticks(30000, "uuu") + ["30000:d"] + ticks(30000, "u" * 20)
        self.assertEqual(self.switch_at(toks), 420000 + 180000)

    def test_dhw_resets_saturation_and_blocks_it_for_300s(self):
        toks = ticks(30000, "zzz") + ["30000:d"] + ticks(30000, "z" * 24)
        self.assertEqual(self.switch_at(toks), 420000 + 300000)

    # --- bypass and skip (AC#4) ---

    # Case 10: underheat running, one bypassed tick, ~200 s gap: the timer restarts, there is no immediate switch.
    def test_bypass_clears_the_underheat_start(self):
        toks = ["30000:u", "30000:u", "30000:b", "200000:u"]
        self.assertEqual(self.run_ticks(toks)["t"], "-1", "a bypassed tick must clear the underheat start")
        # the next ticks count from the restart (u@290): fires 180 s later
        self.assertEqual(self.switch_at(toks + ticks(30000, "u" * 8)), 30000 + 30000 + 30000 + 200000 + 180000)

    def test_bypass_clears_the_saturation_start(self):
        toks = ["30000:z", "30000:z", "30000:b", "400000:z"]
        self.assertEqual(self.run_ticks(toks)["t"], "-1", "a bypassed tick must clear the saturation start")


# TASK-1419 (Sergeant D entry 87, Q45 A): heat pumps enter PWM through the sustained-overshoot switch.
# The gates that call the switches live in satControlLoopRun() (SATcontrol.ino). Their real if/else blocks are
# cut out by regex and wrapped in a host function; the real satGetColdSetpoint() (and satAlwaysMaxModulation()
# when the source defines it) come from the same file, the real switch functions from SATcycles.ino. Only the
# heating source and system getters are stubbed.
GATE_STUBS = r"""
enum { SAT_HSYS_AUTO = 0, SAT_HSYS_RADIATORS = 1, SAT_HSYS_UNDERFLOOR = 2 };
enum { SAT_SRC_AUTO = 0, SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2, SAT_SRC_HYBRID = 3 };
static int g_source = SAT_SRC_GAS_BOILER;
static uint8_t satGetEffectiveHeatingSource() { return (uint8_t)g_source; }
static uint8_t satGetEffectiveHeatingSystem() { return SAT_HSYS_RADIATORS; }
static uint32_t _sat_tickMs = 0;
"""

GATE_MAIN = r"""
int main(int argc, char** argv) {      // argv[1] = heating source, argv[2] = requested setpoint (pidOutput), argv[3] = start mode (cont|pwm)
  if (argc < 4) return 2;
  g_source = atoi(argv[1]);
  const float pid = (float)atof(argv[2]);
  const bool pwm = !strcmp(argv[3], "pwm");
  state.sat.eControlMode = pwm ? SAT_MODE_PWM : SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = pwm;
  g_flow = pwm ? 10.0f : 55.0f;         // continuous: 5 K above 50 + 2 K margin when pid = 50; pwm: far below the request
  const SATControlMode start = state.sat.eControlMode;
  const uint32_t t0 = g_ms;
  for (int i = 0; i < 400; i++) {       // 1 s control ticks through the real gates
    g_ms += 1000; _sat_tickMs = g_ms;
    gate(pid);
    if (state.sat.eControlMode != start) { printf("t=%lu mode=%d\n", (unsigned long)(g_ms - t0), (int)state.sat.eControlMode); return 0; }
  }
  printf("t=-1 mode=%d\n", (int)state.sat.eControlMode);
  return 0;
}
"""

OVERSHOOT_GATE_RE = (r"^[ \t]*if \([^\n]*\) \{\n[ \t]*satCycleCheckOvershootSwitch\(pidOutput, _sat_tickMs\);\n[ \t]*\} else \{\n"
                     r"[ \t]*satCycleSkipAutoSwitch\(\);[^\n]*\n[ \t]*\}")
EXITS_GATE_RE = (r"^[ \t]*if \([^\n]*\) \{\n[ \t]*satCycleCheckPwmExits\(pidOutput, _sat_tickMs\);\n[ \t]*\} else \{\n"
                 r"[ \t]*satCycleResetSustainGuards\(\);[^\n]*\n[ \t]*\}")
OLD_GATE_RE = (r"^[ \t]*if \([^\n]*\) \{\n[ \t]*satCycleCheckAutoSwitch\(pidOutput\);\n[ \t]*\} else \{\n"
               r"[ \t]*satCycleSkipAutoSwitch\(\);[^\n]*\n[ \t]*\}")


def extract_gate(sat_src: str) -> str:
    """The real gate blocks, the real cold-setpoint getter and constants (and satAlwaysMaxModulation() when the source defines it)."""
    sat_src = sat_src.replace("\r\n", "\n")
    m = re.search(OVERSHOOT_GATE_RE, sat_src, re.M)
    if m:
        e = re.search(EXITS_GATE_RE, sat_src, re.M)
        if not e:
            raise AssertionError("the satCycleCheckPwmExits(pidOutput, _sat_tickMs) if/else gate was not found in %s" % SAT_INO)
        blocks = m.group(0) + "\n" + e.group(0)
    else:
        o = re.search(OLD_GATE_RE, sat_src, re.M)
        if not o:
            raise AssertionError("neither the satCycleCheckOvershootSwitch nor the satCycleCheckAutoSwitch if/else gate was found in %s" % SAT_INO)
        blocks = o.group(0)
    consts = re.findall(r"^static const float SAT_COLD_SETPOINT_(?:RAD|FLOOR)\s*=[^;]*;", sat_src, re.M)
    cold = _fn(sat_src, "static float satGetColdSetpoint()")
    if len(consts) != 2 or not cold:
        raise AssertionError("satGetColdSetpoint() or its constants not found in %s" % SAT_INO)
    always = _fn(sat_src, "static bool satAlwaysMaxModulation()")   # empty when the source does not define it
    return "\n".join(consts) + "\n" + cold + "\n" + always + "\nstatic void gate(float pidOutput) {\n" + blocks + "\n}\n"


class GateHeatPumpSwitch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cyc = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        flag, body = extract(cyc)
        gate = extract_gate(SAT_INO.read_text(encoding="utf-8"))
        cpp = d / "g.cpp"
        cpp.write_text(flag + STUBS + GATE_STUBS + body + gate + GATE_MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "g.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_gate(self, source, pid, mode="cont"):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(source), str(pid), mode], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_heat_pump_overshoot_switches_to_pwm(self):
        r = self.run_gate(2, 50)                      # SAT_SRC_HEAT_PUMP
        self.assertEqual((r["t"], r["mode"]), ("61000", "2"), "heat pump never entered PWM on a sustained overshoot: %r" % r)

    def test_gas_boiler_overshoot_switches_to_pwm(self):
        r = self.run_gate(1, 50)                      # SAT_SRC_GAS_BOILER
        self.assertEqual((r["t"], r["mode"]), ("61000", "2"), r)

    def test_hybrid_overshoot_switches_to_pwm(self):
        r = self.run_gate(3, 50)                      # SAT_SRC_HYBRID
        self.assertEqual((r["t"], r["mode"]), ("61000", "2"), r)

    def test_requested_at_or_below_cold_setpoint_never_switches(self):
        for src in (1, 2):
            with self.subTest(source=src):
                r = self.run_gate(src, 20)            # below the radiator cold setpoint 28.2
                self.assertEqual((r["t"], r["mode"]), ("-1", "1"), r)

    # The exits gate: above the cold setpoint the underheat exit fires 180 s after the first tick; at or below it
    # the exit timers are reset every tick (thermo-nova :317-319) and nothing fires.
    def test_pwm_exit_gate_above_and_below_cold_setpoint(self):
        r = self.run_gate(1, 50, "pwm")
        self.assertEqual((r["t"], r["mode"]), ("181000", "1"), r)
        r = self.run_gate(1, 20, "pwm")
        self.assertEqual((r["t"], r["mode"]), ("-1", "2"), r)


def _loop_text():
    src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    a = src.index("static void satControlLoopRun()\n{")
    return src, src[a:src.index("\n}\n", a)]


class BypassSource(unittest.TestCase):
    """The bypass returns of satControlLoopRun() clear the sustained starts (anchored on text, not lines)."""

    @classmethod
    def setUpClass(cls):
        cls.src, cls.loop = _loop_text()

    def _skip_before_return(self, anchor, call="satCycleSkipAutoSwitch();"):
        i = self.loop.index(anchor)
        r = self.loop.index("return", i)
        self.assertIn(call, self.loop[i:r], "no %s between %r and its return" % (call, anchor))

    def test_dhw_return_notes_the_dhw_tick(self):
        self._skip_before_return("    _sat_prevDhwActive = true;", "satCycleNoteDhwTick(_sat_tickMs);")

    def test_fault_return(self):
        self._skip_before_return('boiler fault flag detected')

    def test_thermal_safe_return(self):
        self._skip_before_return("_sat_thermalSafeActive = true;")

    def test_summer_simmer_return(self):
        self._skip_before_return("if (state.sat.bSummerActive && settings.sat.bSummerSimmer) {")

    def test_overshoot_gate_else_branch(self):
        self.assertRegex(self.loop, "(?m)" + OVERSHOOT_GATE_RE, "the satCycleCheckOvershootSwitch gate has no else branch calling satCycleSkipAutoSwitch()")

    def test_exits_gate_else_branch(self):
        self.assertRegex(self.loop, "(?m)" + EXITS_GATE_RE, "the satCycleCheckPwmExits gate has no else branch resetting the sustain guards")

    def test_satdisable_clears_the_start(self):
        a = self.src.index("void satDisable(bool persistPid)\n{")
        self.assertIn("satCycleSkipAutoSwitch();", self.src[a:self.src.index("\n}\n", a)])

    def test_mode_change_clears_the_start(self):
        # Source-text check only: the host harness stubs satSetControlMode(). Covers the control_mode command.
        a = self.src.index("static void satSetControlMode(SATControlMode m)\n{")
        body = self.src[a:self.src.index("\n}\n", a)]
        self.assertRegex(body, r"if \(m != state\.sat\.eControlMode\) \{[^}]*satCycleSkipAutoSwitch\(\);")

    def test_reenable_from_off(self):
        i = self.loop.index("if (state.sat.eControlMode == SAT_MODE_OFF) {")
        self.assertIn("satCycleSkipAutoSwitch();", self.loop[i:self.loop.index("}", i)])


class TickClockAndLoopOrder(unittest.TestCase):
    """STRUCTURAL checks on the text of satControlLoopRun() (no host build of the loop): tick clock capture and call order."""

    @classmethod
    def setUpClass(cls):
        cls.src, cls.loop = _loop_text()

    # STRUCTURAL (text, not behaviour): one millis() read per tick, right after the timer gate.
    def test_tick_time_is_read_once_right_after_due(self):
        self.assertRegex(self.loop, r"if \(!DUE\(timerSATControl\)\) return;\n(?:[ \t]*//[^\n]*\n)*"
                                    r"[ \t]*_sat_tickMs = millis\(\);")
        self.assertEqual(len(re.findall(r"_sat_tickMs = ", self.loop)), 1, "_sat_tickMs must be assigned once per tick")
        self.assertRegex(self.src, r"static uint32_t _sat_tickMs = 0;")

    # STRUCTURAL: both places that set the control interval pass no CATCH_UP type, so the timer keeps the
    # SKIP_MISSED_TICKS type (safeTimers.h) and two ticks are never closer than one interval (entry 109).
    # Only the two-argument call form is accepted.
    def test_control_interval_is_set_without_a_catch_up_type(self):
        settings = SETTINGS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        for path, text in ((SAT_INO, self.src), (SETTINGS_INO, settings)):
            calls = re.findall(r"CHANGE_INTERVAL_SEC\(\s*timerSATControl\s*,([^;]*)\);", text)
            self.assertTrue(calls, "no CHANGE_INTERVAL_SEC(timerSATControl, ...) in %s" % path)
            for args in calls:
                self.assertNotIn("CATCH_UP", args, path)
                self.assertNotIn(",", args, "a third (type) argument in %s: %r" % (path, args))

    def test_switch_code_does_not_read_millis(self):
        cyc = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        for head in ("bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)",
                     "bool satCycleCheckPwmExits(float requestedSetpoint, uint32_t tickMs)",
                     "void satCycleNoteDhwTick(uint32_t tickMs)",
                     "static bool satSustainedFor(bool &running, uint32_t &startMs, uint32_t tickMs, uint32_t limitMs)",
                     "static bool satDhwGuardActive(uint32_t tickMs)"):
            body = _fn(cyc, head)
            self.assertTrue(body, "%s not found" % head)
            code = re.sub(r"//[^\n]*", "", body)
            self.assertNotIn("millis()", code, head)

    # Q47 A (entry 88): the overshoot switch runs after the max-setpoint check and before the control mode is
    # applied, so the tick sends the first PWM CS on the switch tick. Entry 134 (TASK-1468): the order is schedule, exits,
    # then the control setpoint, so the continuous CS goes out on the tick an exit fires.
    def test_overshoot_switch_precedes_apply_control_mode(self):
        loop = self.loop
        i_max = loop.index("satSetControlMode(SAT_MODE_CONTINUOUS);\n    SATDebugTln(F(\"SAT: PID at system max")
        i_os = loop.index("satCycleCheckOvershootSwitch(pidOutput, _sat_tickMs);")
        i_apply = loop.index("// --- Apply control mode ---")
        i_mode = loop.index("const bool pwmMode = (state.sat.eControlMode == SAT_MODE_PWM);")
        i_pwm = loop.index("finalSetpoint = satApplyPWM(pidOutput, true);")
        i_sched = loop.index("satPwmSchedule(pidOutput);")
        i_exit = loop.index("satCycleCheckPwmExits(pidOutput, _sat_tickMs);")
        self.assertLess(i_max, i_os)
        self.assertLess(i_os, i_apply)
        self.assertLess(i_sched, i_exit, "the exits must run after the schedule: saturation reads this tick's off time")
        self.assertLess(i_exit, i_apply, "the exits must run before the control setpoint: the exit tick sends the continuous CS (entry 134)")
        self.assertLess(i_apply, i_mode, "pwmMode must be read after the overshoot switch and the exits")
        self.assertLess(i_mode, i_pwm)
        self.assertNotIn("return", loop[i_os:i_apply], "a return between the overshoot switch and the apply step would skip the first PWM CS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
