#!/usr/bin/env python3
"""TASK-1367 / TASK-1420: host test for the sustained switches of SAT (SATcycles.ino).

Sergeant D entries 41, 86 (Q43 A, Q44 A, Q46 B), 88 (Q47 A), 95 (Q46b) and 108 (docs/sat/decisions/sergeant-d-corrections.md):
the sustained overshoot (continuous -> PWM, 60 s), underheat (PWM -> continuous; TASK-1933 entries 342/350: flame on and request
above the flow for 60 s, was flow <= request - 3 for 180 s) and saturation
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
  o  continuous, flow 53.5 (overshoot: 3.5 over the request)   e  continuous, flow 53 exactly requested + switch margin (3.0)
  h  continuous, flow 52.5 (2.5 over: below the 3.0 margin, no overshoot)
  n  continuous, flow 49 (no overshoot)                      p  PWM OFF phase, flow 55
  u  PWM ON phase, flow 40 (underheat: request 50 above flow) U  PWM ON phase, flow 47
  v  PWM ON phase, flow 49.5 (just below the request)        f  PWM ON phase, flow 40, flame OFF (TASK-1933)
  z  PWM ON phase, flow 50, PWM off time 0 (saturation)      b  the tick bypasses the switches (fault, ...)
  w  PWM ON phase, flow 50, PWM off time 1 (neither)
  m  PWM ON phase, flow missing (satGetFlowTemp() = 0.0, TASK-1428)   y  continuous, flow missing (TASK-1428)
  I  PWM ON phase, flow 40, the flame LIGHTS at this tick (flame-on edge: the flame age is 0 here; ADR-305: the underheat exit has no flame-age gate)
  d  a hot-water tick (satCycleNoteDhwTick), the DHW bit is set from here on (satCycleNoteDhwBit(true))
  x  the loop pass at this time sees the DHW bit drop (satCycleNoteDhwBit(false)); no control tick runs
  s  a draw that began 5 s earlier and ends at this time, between two control ticks: no tick sees it, only the
     loop pass does (satCycleNoteDhwBit true then false). TASK-1429
  W<margin>  set settings.sat.fPwmSwitchMargin (no tick), the one margin (ADR-306)
  g<req>,<flow>  continuous, any requested setpoint and flow, e.g. "g45,62" (TASK-1412.7: setpoint fall, valve step, start-up,
     maximum limiting; the request is what the control loop passes in, after the caller's clamp to the effective maximum)
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
static bool satStartGateActive() { return false; }   // TASK-1942: no start gate here; tests/test_sat_pwm_first_start.py runs the real one
static bool _sat_bootOutsideLogged = false;
enum SATControlMode { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
struct SatSettings { float fPwmSwitchMargin = 3.0f; bool bSummerSimmer = true; };   // ADR-305 / TASK-3151.3: SATtypes.h default, thermo-nova's 3.0 (checked by test_default_switch_margin_is_3_in_sattypes)
struct Settings { SatSettings sat; };
struct SatState { SATControlMode eControlMode = SAT_MODE_CONTINUOUS; bool bDhwActive = false, bPwmFlameRequested = false, bValvesOpen = true, bSummerActive = false; };
struct State { SatState sat; };
static Settings settings; static State state;
static uint32_t g_ms = 1000000UL; static float g_flow = 49.0f;
static uint32_t millis() { return g_ms; }
static float satGetFlowTemp() { return g_flow; }
// ADR-305 / TASK-3151.1: the guards inside the cut-out (satHeatingDemandOn) read the valves, summer mode and the cold setpoint. They
// are open here (valves open, no summer, cold 22 as thermo-nova); each guard is pinned with mutants by tests/test_sat_mode_guards_3151.py.
#ifdef GATE_REAL_COLD   // the gate harness defines the real satColdThreshold() (cut out of SATcontrol.ino) after the cycles section
static float satColdThreshold();
#else
static float satColdThreshold() { return 22.0f; }
#endif
static bool g_flame = true;            // TASK-1933: the entry-342 exit counts only with the flame on (token f: flame off)
static bool satIsFlameOn() { return g_flame; }
// TASK-3008 (entry 456): the flame edge state satCycleFlameEstablished() reads (satCycleOnFlameChange() keeps it on the device).
static bool _cycle_flameOn = true;
static uint32_t _cycle_flameOnStartMs = 0;
static uint32_t satCycleGetFlameOnStartMs() { return _cycle_flameOnStartMs; }
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
  // "all" (entries 428/429): keep running after a mode change and print every change, for scenarios with more than one switch
  bool all = false;
  if (argc > 1 && !strcmp(argv[1], "all")) { all = true; argv++; argc--; }
  if (argc > 1 && !strncmp(argv[1], "start=", 6)) { g_ms = (uint32_t)strtoul(argv[1] + 6, 0, 10); argv++; argc--; }
  uint32_t tick = g_ms;
  _cycle_flameOn = true; _cycle_flameOnStartMs = g_ms - 600000UL;   // burning for 10 min at the start
  const uint32_t t0 = tick;
  float g_req = 50.0f;
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
    g_req = 50.0f;
    if (k == 'g') {                        // continuous, the request and the flow from the token
      float rq = 50.0f, fl = 49.0f;
      sscanf(colon + 2, "%f,%f", &rq, &fl);
      g_req = rq; g_flow = fl; g_offTime = 1;
      state.sat.eControlMode = SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = false;
    }
    if (k == 'W') { settings.sat.fPwmSwitchMargin = (float)atof(colon + 2); continue; }   // TASK-1888: switch margin
    if (k == 'F') {                        // entries 428/429: the flame goes out (satCycleOnFlameChange calls this on the device)
#ifdef HAS_BURN
      satCycleNoteBurnEnd();
#endif
      continue;
    }
    if (k == 'b') {
#ifdef HAS_SKIP
      satCycleSkipAutoSwitch();
#endif
      continue;
    }
    if (k == 'x') {                        // the loop pass sees the DHW bit drop
#ifdef HAS_BIT
      satCycleNoteDhwBit(false, tick);
#endif
      continue;
    }
    if (k == 's') {                        // a draw between two ticks: rises 5 s before, drops now
#ifdef HAS_BIT
      satCycleNoteDhwBit(true, tick - 5000);
      satCycleNoteDhwBit(false, tick);
#endif
      continue;
    }
    if (k == 'd') {
      state.sat.bDhwActive = true;
#ifdef HAS_BIT
      satCycleNoteDhwBit(true, tick);
#endif
#if defined(HAS_NOTE)
      satCycleNoteDhwTick(tick);
#elif defined(HAS_SKIP)
      satCycleSkipAutoSwitch();            // old control loop: the DHW branch only skipped
#endif
      state.sat.bDhwActive = false;
      continue;
    }
    if (k == 'o' || k == 'n' || k == 'e' || k == 'h') { state.sat.eControlMode = SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = false;
                                            g_flow = (k == 'o') ? 53.5f : (k == 'e') ? 53.0f : (k == 'h') ? 52.5f : 49.0f; g_offTime = 1; }
    if (k == 'p') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = false; g_flow = 55.0f; g_offTime = 1; }
    g_flame = (k != 'f');
    if (k == 'I') _cycle_flameOn = false;   // the previous tick had the flame out: this tick is the flame-on edge
    if (g_flame && !_cycle_flameOn) _cycle_flameOnStartMs = g_ms;   // TASK-3008: a flame-on edge at this tick
    _cycle_flameOn = g_flame;
    if (k == 'u' || k == 'I') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 40.0f; g_offTime = 1; }
    if (k == 'f') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 40.0f; g_offTime = 1; }
    if (k == 'v') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 49.5f; g_offTime = 1; }
    if (k == 'U') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 47.0f; g_offTime = 1; }
    if (k == 'q') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 47.1f; g_offTime = 1; }   // TASK-3008: request - 2.9
    if (k == 'm') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 0.0f; g_offTime = 1; }
    if (k == 'y') { state.sat.eControlMode = SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = false; g_flow = 0.0f; g_offTime = 1; }
    if (k == 'w') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 50.0f; g_offTime = 1; }
    if (k == 'z') { state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; g_flow = 50.0f; g_offTime = 0; }
    const int before = g_switches;
    run_checks(g_req, tick);
    if (g_switches != before) {
#ifdef HEAD_API
      const int satrun = _sustain_saturationSec > 0.0f;
#else
      const int satrun = _sustain_saturationRunning;
#endif
      printf("t=%lu mode=%d satrun=%d\n", (unsigned long)(tick - t0), (int)state.sat.eControlMode, satrun);
      if (!all) return 0;
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
        has_bit = "#define HAS_BIT 1\n" if "void satCycleNoteDhwBit(bool dhwBit, uint32_t nowMs)" in src else ""
        has_bit += "#define HAS_BURN 1\n" if "void satCycleNoteBurnEnd()" in src else ""   # entries 428/429
        return "#define HAS_SKIP 1\n#define HAS_NOTE 1\n" + has_bit,"\n".join(consts) + "\n" + src[a:b] + "\n" + ovs + "\n" + ext + "\n"
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


SWITCH_MS = 60000   # TASK-2005, entry 407: continuous -> PWM after 60 s (was 30 s, entry 327)


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

    # --- overshoot (continuous -> PWM), 30 s since TASK-1888 (entry 327; was 60 s, entry 22) ---

    # Case 1: 30 s control tick, overshoot from T0 = 30 s. T0 starts the timer, switch once 30 s passed (thermo-nova: 60 s).
    def test_1_thirty_second_tick_switches_after_30s_of_overshoot(self):
        t0 = 30000
        self.assertEqual(self.switch_at(ticks(30000, "oooooo")), t0 + SWITCH_MS)

    # Case 2: 10 s tick.
    def test_2_ten_second_tick_switches_after_30s(self):
        t0 = 10000
        self.assertEqual(self.switch_at(ticks(10000, "o" * 12)), t0 + SWITCH_MS)

    # Case 3: a dip below the threshold clears the start (heating_control.py:288-291).
    def test_3_dip_restarts_the_timer(self):
        restart = 90000                         # o@30 n@60 o@90 ...
        self.assertEqual(self.switch_at(ticks(30000, "onoooo")), restart + SWITCH_MS)

    # Case 4: 120 s control interval (settingStuff allows 61-300 s): the second qualifying tick is 120 s in.
    def test_4_long_interval_still_switches(self):
        t0 = 120000
        self.assertEqual(self.switch_at(ticks(120000, "ooo")), t0 + 120000)

    # Case 5: one tick in PWM clears the start (thermo-nova :264-266); counting restarts on the return.
    def test_5_pwm_tick_clears_the_start(self):
        back = 90000                            # o@30 p@60 o@90 ...
        self.assertEqual(self.switch_at(ticks(30000, "opoooo")), back + SWITCH_MS)

    # Case 6: ticks that bypass the switch (DHW, fault, thermal-safe, summer, cold setpoint) clear the start.
    def test_6_bypassed_ticks_clear_the_start(self):
        resume = 150000                         # o@30 b@60 b@90 b@120 o@150 ...
        self.assertEqual(self.switch_at(ticks(30000, "obbboooo")), resume + SWITCH_MS)

    # Case 7: boundary, 59 999 ms after the start does not switch, 60 000 ms does (TASK-2005, entry 407; was 30 000).
    def test_7_boundary_60000_ms(self):
        start = 30000
        no = self.run_ticks(["30000:o", "30000:o", "29999:o"])
        self.assertEqual(no["t"], "-1", "59 999 ms of overshoot must not switch")
        self.assertEqual(self.switch_at(["30000:o", "30000:o", "29999:o", "1:o"]), start + SWITCH_MS)

    # Q43 A (entry 86): flow exactly on requested + switch margin (52, exact in binary) counts (>=).
    def test_equality_counts_and_switches_at_60s(self):
        self.assertEqual(self.switch_at(ticks(30000, "eeee")), 30000 + SWITCH_MS)

    # Q43 A: a flow just below the threshold does not count.
    # ADR-305 (Robert 2026-11-28, TASK-3151.3): the switch is flow - requested >= 3.0 C (fPwmSwitchMargin default, thermo-nova's
    # value) for 60 s. 2.5 C and 2.9 C over the request never switch (2.0 C did until ADR-305); 3.0 C does after 60 s.
    def test_flow_2_5_over_the_request_never_switches(self):
        self.assertEqual(self.run_ticks(["30000:g50,52.5"] * 12)["t"], "-1")   # 360 s held
        self.assertEqual(self.run_ticks(["10000:g50,52.5"] * 12)["t"], "-1")   # 120 s held, 10 s ticks

    def test_flow_2_9_over_the_request_never_switches(self):
        self.assertEqual(self.run_ticks(["30000:g50,52.9"] * 12)["t"], "-1")

    def test_flow_3_0_over_the_request_switches_after_60s_not_30s(self):
        self.assertEqual(self.run_ticks(["10000:g50,53"] * 5)["t"], "-1", "no switch 40 s after the first tick (was 30 s: the fail-safe)")
        self.assertEqual(self.switch_at(["10000:g50,53"] * 12), 10000 + SWITCH_MS)
        self.assertEqual(self.switch_at(["30000:g50,53"] * 6), 30000 + SWITCH_MS)

    # ADR-305 verification table, control row: flow 3.0 C over for 60 s from a clean continuous state switches. Green on the code
    # before ADR-305 as well (it switched at 30 s there), so it shows the rule still works, not that the code changed.
    def test_control_flow_3_0_over_from_clean_continuous_switches_within_70s(self):
        t = self.switch_at(["10000:g50,53"] * 12)
        self.assertTrue(0 < t <= 70000, t)

    # TASK-2005 / ADR-305: the stub's 3.0 must be the real default of the setting.
    def test_default_switch_margin_is_3_in_sattypes(self):
        src = Path(os.environ.get("OTGW_SATTYPES_H", FW / "SATtypes.h")).read_text(encoding="utf-8")
        m = re.search(r"^\s*float\s+fPwmSwitchMargin\s*=\s*([0-9.]+)f\s*;", src, re.M)
        self.assertIsNotNone(m, "fPwmSwitchMargin default not found in SATtypes.h")
        self.assertEqual(float(m.group(1)), 3.0)

    # The switch margin is its own setting (0.5-5.0): raised to 2.5, 52.4 no longer counts and 52.5 does.
    def test_switch_margin_setting_moves_the_threshold(self):
        self.assertEqual(self.run_ticks(["0:W2.5"] + ["30000:g50,52.4"] * 8)["t"], "-1")
        self.assertEqual(self.switch_at(["0:W2.5"] + ["30000:g50,52.5"] * 4), 30000 + SWITCH_MS)

    def test_flow_below_threshold_does_not_count(self):
        self.assertEqual(self.run_ticks(ticks(30000, "n" * 12))["t"], "-1")

    # Q46 B (entry 86), entry 109: the timer skips missed ticks, so a late tick pushes the following ticks later
    # (each tick = previous + interval + lateness). The switch comes on the first tick at least 60 s after the
    # first qualifying tick (TASK-2005): the 3rd qualifying tick at a 30 s interval, never earlier.
    def test_switch_at_first_tick_at_least_60s_after_the_start_when_ticks_run_late(self):
        # first tick on time (30000), then 8 and 3 ms late: ticks at 30000, 60008, 90011 -> 60011 ms after the start
        self.assertEqual(self.switch_at(["30000:o@0", "30000:o@8", "30000:o@3", "30000:o@0"]), 90011)
        # first tick 8 ms late (30008), the rest on time (drift carries): 90008 -> exactly 60000 ms
        self.assertEqual(self.switch_at(["30000:o@8", "30000:o@0", "30000:o@0", "30000:o@0"]), 90008)

    # --- underheat (PWM ON phase), 180 s, saturation, 300 s ---

    # TASK-1933 (Sergeant D entries 339, 342, 350): the exit is "flame on and request above the flow for 60 s", 60 s after the
    # first qualifying tick (Q44 A timing), at a 30 s and a 60 s tick. Rewritten on purpose: thermo-nova's flow <= request - 3
    # for 180 s (:337-347, const.py:22) fired at dt + 180000.
    # TASK-3008 (entry 448): thermo-nova's underheat exit, flow <= request - 3.0 C for 180 s (heating_control.py:330-344, const.py:22).
    def test_underheat_fires_180s_after_the_first_tick_at_30s_and_60s_ticks(self):
        for dt in (30000, 60000):
            with self.subTest(tick_s=dt // 1000):
                r = self.run_ticks(ticks(dt, "u" * 12))
                self.assertEqual((int(r["t"]), r["mode"]), (dt + 180000, "1"))

    # TASK-3008: the edge is request - 3.0 (U, flow 47 = 50 - 3 counts); a flow just below the request (v, 49.5) or equal to it
    # (w, 50) never fires. The entry-342 rule (TASK-1933) fired on 49.5 after 60 s.
    def test_underheat_counts_only_3c_below_the_request(self):
        for dt in (30000, 60000):
            with self.subTest(tick_s=dt // 1000):
                self.assertEqual(self.switch_at(ticks(dt, "U" * 12)), dt + 180000)
                self.assertEqual(self.run_ticks(ticks(dt, "q" * 12))["t"], "-1")   # flow 47.1 = request - 2.9
                self.assertEqual(self.run_ticks(ticks(dt, "v" * 12))["t"], "-1")
                self.assertEqual(self.run_ticks(ticks(dt, "w" * 12))["t"], "-1")

    # ADR-305 (entry 456 replaced): no flame-age gate before the underheat exit. The exit is thermo-nova's, 3.0 C below the request
    # for 180 s from the first qualifying tick (heating_control.py:330-344). A flame that lights at the first tick (token I) gives the
    # exit at 30 + 180 s; before ADR-305 the flame had to burn 180 s first (exit at about 390 s). The tests
    # test_underheat_waits_for_180s_of_flame, test_underheat_never_counts_with_the_flame_off and test_flame_off_restarts_the_flame_wait
    # (the old gate) are deleted with it.
    def test_underheat_counts_from_the_ignition_tick(self):
        self.assertEqual(self.switch_at(ticks(30000, "I" + "u" * 12)), 30000 + 180000)

    def test_underheat_run_survives_a_flame_out_and_relight(self):
        # u@30 u@60 (flame burning), f@90 (flame out), I@120 relights: the qualifying run started at 30 is not cleared by the flame.
        self.assertEqual(self.switch_at(ticks(30000, "uu" + "f" + "I" + "u" * 8)), 30000 + 180000)

    # Same as thermo-nova (heating_control.py:321-323 and :331-356): the underheat count runs in the PWM ON phase, which includes the
    # wait for ignition; the flame is not read (otgw-poc-dev, ADR-305). Token f: ON phase, flame out, flow 40 against 50.
    def test_underheat_counts_in_the_on_phase_with_the_flame_out_as_thermo_nova_does(self):
        self.assertEqual(self.switch_at(ticks(30000, "f" * 12)), 30000 + 180000)

    # A flow above request - 3 resets the underheat start: u@30 w@60 u@90 ... fires at 90 + 180.
    def test_underheat_dip_restarts_the_timer(self):
        self.assertEqual(self.switch_at(ticks(30000, "uw" + "u" * 8)), 30000 * 3 + 180000)

    # Q44 A: a 61 s tick counts and fires on the first tick at least 180 s after the first one (61 + 183).
    def test_underheat_61s_tick_still_fires(self):
        self.assertEqual(self.switch_at(ticks(61000, "uuuuu")), 61000 + 183000)

    # TASK-3008: 170 s of underheat does not fire (10 s ticks: u@10 .. u@180 is 170 s), the next tick (180 s) does.
    def test_underheat_needs_the_full_180s(self):
        self.assertEqual(self.run_ticks(ticks(10000, "u" * 18))["t"], "-1")
        self.assertEqual(self.switch_at(ticks(10000, "u" * 19)), 10000 + 180000)

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
        r = self.run_ticks(ticks(30000, "zzzz") + ticks(30000, "oooo"))   # o@150 starts, o@210 switches (60 s, TASK-2005)
        self.assertEqual((int(r["t"]), r["mode"], r["satrun"]), (210000, "2", "0"))

    # Exit timers reset in the PWM OFF phase and when the mode is not PWM (thermo-nova :308-311, :321-323).
    def test_exit_timers_reset_in_the_off_phase(self):
        # u@30 p@60 (OFF phase: reset) u@90 starts again, fires 180 s later (TASK-3008, entry 448)
        self.assertEqual(self.switch_at(ticks(30000, "up" + "u" * 8)), 30000 * 3 + 180000)

    # --- hot-water guard (entry 108, thermo-nova :272-274, :313-315) ---

    # The 300 s guard after a draw arms: overshoot right after the draw does not switch; counting starts once
    # tick - end >= 300 s. d@30 (end = 30 s) ... o@330 starts the timer, o@390 fires (60 s, TASK-2005).
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

    # TASK-1429 (entry 108, thermo-nova device/__init__.py:223-224): a draw that began and ended between two ticks
    # (no tick sees the bit) arms the guard from the moment the bit dropped. s@30: o@60..o@330-30: guarded (tick - 30 <
    # 300 until o@300); o@330 is the first unguarded tick and starts the timer; o@390 fires (60 s, TASK-2005).
    def test_sub_tick_draw_arms_the_guard(self):
        r = self.run_ticks(["30000:s"] + ticks(30000, "o" * 9))                     # o@60 .. o@300: guarded
        self.assertEqual(r["t"], "-1", "a draw between two ticks must arm the 300 s guard: %r" % r)
        self.assertEqual(self.switch_at(["30000:s"] + ticks(30000, "o" * 13)), 390000)

    # The guard counts 300 s from the moment the DHW bit dropped, not from the last tick that saw it set: d@30 (bit
    # set), the bit drops at 70 (no tick), ticks at 100, 130 ...: the guard ends at 370, o@370 starts, o@430 fires.
    # Counting from the tick (30) the guard would end at 330 and the switch would come at 400 (o@340 starts).
    def test_guard_counts_from_the_real_end_of_the_draw(self):
        toks = ["30000:d", "40000:x"] + ticks(30000, "o" * 13)
        self.assertEqual(self.switch_at(toks), 430000)


    # A draw also resets running exit timers, and they count only 300 s after it.
    def test_dhw_resets_underheat_and_blocks_it_for_300s(self):
        # u@30 u@60 d@90 u@120..u@360 guarded (tick - 90 < 300), u@390 starts, fires at 570 (TASK-3008: 180 s)
        toks = ticks(30000, "uu") + ["30000:d"] + ticks(30000, "u" * 20)
        self.assertEqual(self.switch_at(toks), 390000 + 180000)

    def test_dhw_resets_saturation_and_blocks_it_for_300s(self):
        toks = ticks(30000, "zzz") + ["30000:d"] + ticks(30000, "z" * 24)
        self.assertEqual(self.switch_at(toks), 420000 + 300000)

    # --- TASK-1428 (entries 218, 241; thermo-nova heating_control.py:284-286, :333-335): a missing flow resets the starts ---

    # u@30 u@60 m@90 (flow missing: the underheat start is cleared) u@120 restarts, fires at 120 + 180. Old code: the
    # missing flow (0.0) counts as underheat, fires at 30 + 180.
    def test_missing_flow_resets_the_underheat_start(self):
        self.assertEqual(self.switch_at(ticks(30000, "um" + "u" * 8)), 30000 * 3 + 180000)   # TASK-3008: 180 s

    # Missing flow for the whole stint never fires underheat. Old code: fires 180 s after the first tick.
    def test_missing_flow_alone_never_counts_as_underheat(self):
        self.assertEqual(self.run_ticks(ticks(30000, "m" * 20))["t"], "-1")

    # The missing flow also clears the start on a long (> 60 s) tick.
    def test_missing_flow_resets_the_underheat_start_at_a_90s_tick(self):
        self.assertEqual(self.switch_at(ticks(90000, "umuuu")), 90000 * 5)   # u@90 m@180 u@270 .. u@450: 180 s (TASK-3008)

    # Overshoot: o@30 o@60 y@90 (flow missing) o@120 restarts, switch at 120 + 30 (TASK-1888). Without the missing-flow
    # reset the o@30 start would have switched at o@60 already.
    def test_missing_flow_resets_the_overshoot_start(self):
        self.assertEqual(self.switch_at(ticks(30000, "oyoooo")), 30000 * 3 + SWITCH_MS)
        self.assertEqual(self.run_ticks(ticks(30000, "y" * 12))["t"], "-1")

    # --- bypass and skip (AC#4) ---

    # Case 10: underheat running, one bypassed tick, ~200 s gap: the timer restarts, there is no immediate switch.
    def test_bypass_clears_the_underheat_start(self):
        toks = ["30000:u", "30000:u", "30000:b", "200000:u"]
        self.assertEqual(self.run_ticks(toks)["t"], "-1", "a bypassed tick must clear the underheat start")
        # the next ticks count from the restart (u@290): fires 180 s later (TASK-3008)
        self.assertEqual(self.switch_at(toks + ticks(30000, "u" * 8)), 30000 + 30000 + 30000 + 200000 + 180000)

    def test_bypass_clears_the_saturation_start(self):
        toks = ["30000:z", "30000:z", "30000:b", "400000:z"]
        self.assertEqual(self.run_ticks(toks)["t"], "-1", "a bypassed tick must clear the saturation start")



# ADR-305 (entries 517, 519, 520; TASK-3151.1): the classes OnePwmEntryPerFlame (the flame that left PWM stays in continuous,
# entries 428/429/430) and FlameOutFailsafe (flow - request >= 3.0 for 30 s switches to PWM, entries 288/289/417/429) are
# deleted. They pinned two rules thermo-nova does not have (heating_control.py:262-305: one overshoot rule, no fail-safe, no lock) and
# ADR-305 removes both. The rule that replaces them is pinned by SameFlameReentry below.
class SameFlameReentry(unittest.TestCase):
    """ADR-305: after a PWM exit with the flame lit, the same flame goes back to PWM by the one overshoot rule: flow >= request + 3.0
    held for 60 s from the first qualifying tick. No lock holds the margin rule, no 30 s fail-safe takes the flame back earlier."""
    setUpClass = classmethod(SustainedSwitches.setUpClass.__func__)
    tearDownClass = classmethod(SustainedSwitches.tearDownClass.__func__)

    def switches(self, toks):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), "all", *toks], capture_output=True, text=True, check=True).stdout
        rows = [dict(kv.split("=") for kv in line.split()) for line in out.splitlines() if not line.startswith("t=-1")]
        return [(int(r["t"]), int(r["mode"])) for r in rows]

    # Token u from a burning flame (flame lit 10 min before the first tick): PWM ON, request 50, flow 40, underheat exit.
    # The exit timing itself is pinned by test_underheat_*; the cases below only need the exit as a starting point.
    EXIT = ["0:u"] + ["30000:u"] * 6   # exit to continuous at t=180000, the flame stays on

    def test_flow_3_0_over_takes_the_same_flame_back_to_pwm_60s_after_the_first_tick(self):
        sw = self.switches(self.EXIT + ["30000:g50,53"] * 4)   # first qualifying tick t=210000
        self.assertEqual(sw[0], (180000, 1), sw)
        self.assertEqual(sw[1], (270000, 2), "re-entry 60 s after 210000, no lock, no 30 s fail-safe (240000): %r" % (sw,))

    def test_flow_3_5_over_is_no_faster_than_3_0_over(self):
        sw = self.switches(self.EXIT + ["30000:g50,54"] * 4)   # was the 30 s fail-safe at 240000
        self.assertEqual(sw[1], (270000, 2), sw)

    def test_flow_2_5_over_never_takes_the_same_flame_back(self):
        sw = self.switches(self.EXIT + ["30000:g50,52.5"] * 12)
        self.assertEqual(len(sw), 1, "only the exit: %r" % (sw,))

    def test_a_burn_that_ended_is_no_different(self):
        sw = self.switches(self.EXIT + ["0:F"] + ["30000:g50,53"] * 4)
        self.assertEqual(sw[1], (270000, 2), sw)

    def test_the_overshoot_switch_has_no_failsafe_and_no_burn_lock(self):
        src = CYCLES_INO.read_text(encoding="utf-8").replace(chr(13), "")
        body = _fn(src, "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)")
        self.assertTrue(body)
        for gone in ("FAILSAFE", "fail-safe", "_cycle_burnLeftPwm", "_cycle_burnHoldLogged"):
            self.assertNotIn(gone, body)
        for gone in ("SAT_OVERSHOOT_FAILSAFE", "_sustain_overshootFailsafe", "_cycle_burnLeftPwm", "_cycle_burnHoldLogged"):
            self.assertNotIn(gone, src, "%s is gone with ADR-305" % gone)


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
  g_flow = pwm ? 10.0f : 53.0f;         // continuous: exactly 50 + the 3.0 K margin when pid = 50 (ADR-305); pwm: far below the request
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
    # TASK-1652: satColdThreshold() reads SAT_MIN_SETPOINT while the cutoff waits for the minimum burn
    consts += re.findall(r"^static const float SAT_MIN_SETPOINT\s*=[^;]*;", sat_src, re.M)
    always = _fn(sat_src, "static bool satAlwaysMaxModulation()")   # empty when the source does not define it
    # TASK-1547: the gate compares against the latched threshold; take the real latch, band and helper when the source has them
    latch = "\n".join(re.findall(r"^static (?:bool _sat_coldCut(?:Deferred)?|const float SAT_COLD_HYSTERESIS_C)\s*=[^;]*;", sat_src, re.M))
    # Entries 410/417 (TASK-1887): the gate also reads the band-pause flag; take its real declaration (false = no pause) when present
    latch += "\n" + "\n".join(re.findall(r"^static bool\s+_sat_tolBandOff\s*=[^;]*;", sat_src, re.M))
    # TASK-1652: satColdCutActive() sits next to the latch (empty before); then the threshold (empty before TASK-1547)
    thr = _fn(sat_src, "static bool satColdCutActive()") + "\n" + _fn(sat_src, "static float satColdThreshold()")
    return ("\n".join(consts) + "\n" + cold + "\n" + latch + "\n" + thr + "\n" + always
            + "\nstatic void gate(float pidOutput) {\n" + blocks + "\n}\n")


class LowLoadEntryCells(unittest.TestCase):
    """TASK-1412.7, LL-012 / R06: the continuous -> PWM entry is a TEMPERATURE PROXY, not proof of minimum-power saturation.

    The real satCycleCheckOvershootSwitch() decides from three numbers only: the requested setpoint, the flow temperature and the
    MsgID 3 permission. Whatever moved the flow above request + 1.5 K, the verdict is the same: 30 s from the first qualifying tick
    (entries 327 and 360 since TASK-1888, before them 22; 41, 86, 109; thermo-nova heating_control.py:262-305). The cells below pin that for the situations the OT application
    note (AFS 2.1.1.2.1) says are NOT real low load: start-up, a setpoint fall, a pump or valve step, the maximum-temperature
    limit and the heat left after a hot-water draw. They are characterization tests of the recorded rule, not fixes: the
    conditions of OT-R1 that SAT does not use (flame off with demand, ID17 <= 5 %, shortly after start-up, a fast setpoint
    change) are differences to the OpenTherm recommendation, listed in docs/sat/sat-low-load-verification-plan-2026-10-28.md.
    Tick 30 s. A token "g<req>,<flow>" is one control tick in continuous mode.
    """

    @classmethod
    def setUpClass(cls):
        src = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.src = src
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

    @staticmethod
    def g(*pairs, dt=30000):
        return ["%d:g%g,%g" % (dt, rq, fl) for rq, fl in pairs]

    def test_setpoint_fall_is_a_proxy_hit(self):
        # Request 60 -> 50 with the flow at 53: no overshoot at 60 (53 < 62), overshoot at 50 (53 >= 52) although the boiler is
        # modulating normally and only cools towards the new request. First qualifying tick 90 s, switch 60 s later (TASK-2005).
        # (53 stays below the fail-safe 50 + 3.0 of entry 429, so this tests the switch rule.)
        toks = self.g((60, 53), (60, 53), (50, 53), (50, 53), (50, 53), (50, 53))
        self.assertEqual(self.switch_at(toks), 150000)

    def test_setpoint_fall_that_is_reversed_inside_60s_restarts_the_timer(self):
        toks = self.g((60, 53), (50, 53), (60, 53), (50, 53), (50, 53), (50, 53))
        self.assertEqual(self.switch_at(toks), 180000)

    def test_startup_ramp_through_the_margin_counts_from_the_first_tick_above_it(self):
        # Cold start at high fire: the flow runs 20, 35, 50, 53, 53.2, 53.4 at request 50. 53 >= 52 is the first qualifying tick (120 s).
        # (All below the fail-safe 50 + 3.0 of entry 429; a steeper ramp is test_failsafe_startup_ramp_switches_after_30s.)
        # The burner is at high power and the flow is still rising, yet the proxy fires 60 s later (TASK-2005). OT-R1 excludes
        # "shortly after boiler start-up"; SAT does not (entry 22 only runs the cold start in continuous first).
        toks = self.g((50, 20), (50, 35), (50, 50), (50, 53), (50, 53.2), (50, 53.4))
        self.assertEqual(self.switch_at(toks), 180000)

    def test_startup_59999ms_does_not_switch_60000ms_does(self):
        self.assertEqual(self.switch_at(self.g((50, 53), (50, 53)) + self.g((50, 53), dt=29999)), -1)
        self.assertEqual(self.switch_at(self.g((50, 53), (50, 53)) + self.g((50, 53), dt=30000)), 90000)

    def test_valve_or_pump_step_shorter_than_60s_never_switches(self):
        # A flow step of +8 K that lasts one tick (zone valve, pump start) and returns: no switch.
        toks = self.g((50, 45), (50, 53), (50, 45), (50, 45), (50, 53), (50, 45))
        self.assertEqual(self.switch_at(toks), -1)

    def test_valve_or_pump_step_held_for_60s_switches_although_nothing_is_saturated(self):
        toks = self.g((50, 45), (50, 53), (50, 53), (50, 53))
        self.assertEqual(self.switch_at(toks), 120000)

    def test_a_flow_dip_inside_the_window_restarts_the_timer(self):
        # 53 @30 starts, 48 @60 (inside the 60 s window) clears it, 53 @90 starts again, fires @150 (TASK-2005)
        toks = self.g((50, 53), (50, 48), (50, 53), (50, 53), (50, 53))
        self.assertEqual(self.switch_at(toks), 150000)

    def test_maximum_temperature_limit_counts_like_any_other_request(self):
        # The caller clamps the request to the effective maximum (here 55) before the check. A boiler held at its limit with the
        # flow 2.0 K above it is an overshoot verdict (57, exact in binary), one 0.1 K lower is not (TASK-2005; was 1.5 K).
        self.assertEqual(self.switch_at(self.g((55, 58), (55, 58), (55, 58))), 90000)
        self.assertEqual(self.switch_at(self.g(*[(55, 57.9)] * 8)), -1)

    def test_heat_left_after_a_hot_water_draw_is_blocked_for_300s_then_counts_60s(self):
        # AFS 2.2.3 situation: at the end of a draw the supply stays above the request (here 44 against 41, inside the switch
        # rule's band, below the fail-safe 41 + 3.0 of entry 429). The 300 s guard keeps the entry closed (the guard is not the AFS
        # recovery: nothing is held, it only forbids the switch), and the proxy then needs 60 s of qualifying ticks: the switch
        # comes 360 s after the draw tick.
        toks = ["30000:d"] + self.g(*[(41, 44)] * 14)
        self.assertEqual(self.switch_at(toks), 30000 + 360000)

    def test_heat_left_far_above_the_request_after_a_draw_is_blocked_for_300s_then_counts_60s(self):
        # ADR-305: no fail-safe. Supply 62 against 41 (21 K over): nothing counts inside the 300 s guard; the first tick after it
        # starts the one timer and it fires 60 s later: 360 s after the draw tick (330 s with the old 30 s fail-safe).
        toks = ["30000:d"] + self.g(*[(41, 62)] * 14)
        self.assertEqual(self.switch_at(toks), 30000 + 360000)

    def test_true_minimum_capacity_saturation_is_a_different_path_and_takes_300s(self):
        # PWM -> continuous after 300 s of "the PWM off time is 0" (the duty asks for no OFF phase at all): it is the one
        # place where the controller itself knows the boiler cannot be throttled further. 12 ticks of 30 s after the first.
        self.assertEqual(self.switch_at(ticks(30000, "z" * 14)), 30000 + 300000)

    def test_entry_reads_no_flame_modulation_or_status_signal(self):
        body = _fn(self.src, "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)")
        self.assertTrue(body, "satCycleCheckOvershootSwitch not found")
        for token in ("Modulation", "RelMod", "satIsFlameOn", "Statusflags", "MasterStatus", "SlaveStatus", "OTcurrentSystemState"):
            self.assertNotIn(token, body, "the entry now reads %s: it is no longer a pure temperature proxy; update LL-012 and the "
                                          "evidence section of docs/sat/sat-low-load-verification-plan-2026-10-28.md" % token)

    def test_request_at_the_system_maximum_ends_pwm_before_the_entry_check(self):
        # The loop cannot be sliced (static, hardware-bound), so this is a source assertion. thermo-nova update():
        # `if requested >= maximum_setpoint: pwm.disable()` precedes `_maybe_enable_pwm_on_sustained_overshoot` (heating_control.py:203-210).
        s = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        # prefix: TASK-1942 (entries 385/389) adds '&& !satStartGateActive()'; the order this test pins is unchanged
        a = s.index("if (pidOutput >= maxSetpoint && state.sat.eControlMode == SAT_MODE_PWM")
        b = s.index("satCycleCheckOvershootSwitch(pidOutput, _sat_tickMs);")
        self.assertLess(a, b)


class PwmExitsWithoutGates(unittest.TestCase):
    """ADR-305 decision 5 (entries 463 and 456 replaced): the PWM exits carry no start gate and no flame-age gate. STRUCTURAL: the
    loop cannot be sliced on the host, so the branch and the function are read. A request at the clamped maximum ends PWM on that
    tick, also within 60 s of the first flame; the underheat exit has neither gate."""

    def test_the_at_max_branch_has_no_start_gate_and_no_flame_gate(self):
        s = SAT_INO.read_text(encoding="utf-8").replace(chr(13), "")
        a = s.index("if (pidOutput >= maxSetpoint && state.sat.eControlMode == SAT_MODE_PWM")
        branch = s[a:s.index("}", a)]
        self.assertNotIn("satStartGateActive", branch)
        self.assertNotRegex(branch, r"flameEstablished|SAT_UNDERHEAT_FLAME_MIN_MS|satCycleGetFlameOnStartMs")
        self.assertIn("SAT_MODE_CONTINUOUS", branch)

    def test_the_underheat_exit_has_no_flame_gate_and_no_start_gate(self):
        c = CYCLES_INO.read_text(encoding="utf-8").replace(chr(13), "")
        a = c.index("bool satCycleCheckPwmExits(")
        body = c[a:c.index(chr(10) + "}" + chr(10), a)]
        self.assertNotRegex(body, r"flameEstablished|SAT_UNDERHEAT_FLAME_MIN_MS|satCycleGetFlameOnStartMs")
        self.assertNotIn("satStartGateActive", body)


class GateHeatPumpSwitch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cyc = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        flag, body = extract(cyc)
        gate = extract_gate(SAT_INO.read_text(encoding="utf-8"))
        cpp = d / "g.cpp"
        cpp.write_text(flag + "#define GATE_REAL_COLD 1" + chr(10) + STUBS + GATE_STUBS + body + gate + GATE_MAIN, encoding="utf-8")
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
        self.assertEqual((r["t"], r["mode"]), ("61000", "2"), r)   # 60 s since TASK-2005 (30 s under TASK-1888)

    def test_hybrid_overshoot_switches_to_pwm(self):
        r = self.run_gate(3, 50)                      # SAT_SRC_HYBRID
        self.assertEqual((r["t"], r["mode"]), ("61000", "2"), r)   # 60 s since TASK-2005 (30 s under TASK-1888)

    def test_requested_at_or_below_cold_setpoint_never_switches(self):
        for src in (1, 2):
            with self.subTest(source=src):
                r = self.run_gate(src, 20)            # below the radiator cold setpoint 28.2
                self.assertEqual((r["t"], r["mode"]), ("-1", "1"), r)

    # The exits gate: above the cold setpoint the underheat exit fires 180 s after the first tick (TASK-3008); at or below it
    # the exit timers are reset every tick (thermo-nova :317-319) and nothing fires.
    def test_pwm_exit_gate_above_and_below_cold_setpoint(self):
        r = self.run_gate(1, 50, "pwm")
        self.assertEqual((r["t"], r["mode"]), ("181000", "1"), r)   # TASK-3008: 180 s (entry 448)
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

    # STRUCTURAL (TASK-1429): every loop pass hands the live DHW bit to satCycleNoteDhwBit(), before the timer gate.
    def test_loop_pass_notes_the_dhw_bit_before_the_timer_gate(self):
        self.assertEqual(len(re.findall(r"satCycleNoteDhwBit\(dhwBitNow, millis\(\)\);", self.loop)), 1)
        self.assertLess(self.loop.index("satCycleNoteDhwBit("), self.loop.index("if (!DUE(timerSATControl)) return;"))

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
        i_max = loop.index("satSetControlMode(SAT_MODE_CONTINUOUS);\n    SATDebugTln(F(\"SAT: PID at the effective max")
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
