#!/usr/bin/env python3
"""TASK-1401 + TASK-1369: host test for the PWM hourly start limit and the first PWM phase.

TASK-1401 (Sergeant D entry 59, Q12 A): the hourly start limit counts only PWM's own OFF->ON phase starts
(thermo-nova pwm.py:159-160), never hot-water ignitions or relights. The limit stays cycles per hour
(entry 52). Before the fix every flame-on edge went into the rolling-hour ring (SATcycles.ino
satCycleOnFlameChange), so a shower blocked heating (gruijter, Q12 evidence).

TASK-1369 (entry 43, Q11): when PWM is entered the first phase is ON at once (thermo-nova starts ON from IDLE
without the off wait, pwm.py:154-155), when the ON time is at least HEATER_STARTUP_TIMEFRAME 180 s
(pwm.py:155, const.py:18). Maintainer decision (Robert 2026-10-26, provisional until Q48): the ON phase that
starts when PWM is entered with the burner already running counts as a PWM ON start but the hourly limit
never blocks or cuts it. That phase lasts its ON time from the entry (pwm.py:162-165, :205-209), so the
burner is not cut on the next tick.

Review scenarios (2026-10-26): an entry in duty range 5 anchors its ON phase like any ON start (pwm.py:160-167);
a flame still lit one tick after ON->OFF does not restart ON (OFF timed from the ON->OFF edge, pwm.py:208); the
entry skip survives a missing flow temperature (pwm.py:123) and the hour-limit return (pwm.py:156-158); a millis()
wrap does not bring stale starts back into the rolling hour.

Nothing is re-implemented. The real code is cut out of the sources:
  - SATtypes.h: the SAT enums and structs (the real SATRuntimeSection and SATSection);
  - SATcycles.ino: the cut of tests/test_sat_cycle_classifier.py (constants, globals, _flowPercentile ..
    satCycleSample(), so the real satCycleOnFlameChange()), plus the real rolling-hour ring block
    (_hourCountRecord .. satCycleGetCyclesThisHour, and satCycleRecordPwmStart() when it exists),
    satCycleSkipAutoSwitch() and the flame-on/off accessors;
  - SATcontrol.ino: the PWM block (_pwm_effectiveBoilerTemp .. satPwmDisableOnHeaterOff()).
Stubbed: the heating-system getters (180 s minimum ON, cycles per hour from the test, period 3600/cph),
base offset 27.2, cold setpoint 28.2, the CS queue, the continuous setpoint (marker 1000 + requested).

Old-vs-fix: point OTGW_SAT_INO, OTGW_SATCYCLES_INO and OTGW_SATTYPES_H at the HEAD copies together
(`git show HEAD:src/OTGW-firmware/<file>`).

Run: python tests/test_sat_pwm_start_limit.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_cycle_classifier as cls_harness  # noqa: E402  (types + cycles cut)
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = cls_harness.FW
CYCLES_INO = cls_harness.CYCLES_INO
SAT_INO = cls_harness.SAT_INO
SATTYPES_H = cls_harness.SATTYPES_H

STUBS = r"""
struct SatDebug { bool bSAT = false; };
struct State { SATRuntimeSection sat; SatDebug debug; };
struct Settings { SATSection sat; };
static Settings settings;
static State state;
struct OTSys { uint16_t Statusflags = 0; uint8_t MasterStatus = 0; uint8_t SlaveStatus = 0; float Tr = 20.0f; float Tboiler = 0; float Tret = 30.0f; };
static OTSys OTcurrentSystemState;
static uint32_t g_ms = 1000000UL; static float g_flow = 50.0f; static bool g_flame = false; static uint8_t g_cph = 3;
static int g_csSent = 0; static float g_csLast = 0;
static const float SAT_MIN_SETPOINT = 10.0f;   // SATcontrol.ino:129
static uint32_t millis() { return g_ms; }
static float satGetFlowTemp() { return g_flow; }
static float satGetReturnTemp() { return 30.0f; }
static bool satNoReturnTemp() { return false; }   // TASK-1371: a boiler with a return temperature; the no-return hold is tested in test_sat_no_return_ignition.py
static bool satIsFlameOn() { return g_flame; }
static bool satIsEnabled() { return true; }
static float satGetColdSetpoint() { return 28.2f; }
static float satGetBaseOffset() { return 27.2f; }
static float satGetMaxSetpoint() { return 62.0f; }
static float satEffectiveMaxSetpoint() { return 62.0f; }
static float satApplyContinuous(float p, float /*previousCS*/) { return 1000.0f + p; }
static uint32_t satGetMinOnTimeSec() { return 180; }
static uint8_t satGetMaxCyclesPerHour() { return g_cph; }
static uint32_t satGetPwmPeriodSec() { return 3600UL / g_cph; }
static int g_source = SAT_SRC_GAS_BOILER;            // TASK-1419: the heating source; the hour-limit check itself is the real code
static uint8_t satGetEffectiveHeatingSource() { return (uint8_t)g_source; }
static bool hasOTCommandInterface() { return true; }
static bool satEnqueueIfChangedCS(float cs) { g_csSent++; g_csLast = cs; return true; }
static void sendWebSocketJSON(const char*) {}
static void _dailyRecord(uint32_t, uint32_t, uint32_t, SATCycleClass, float) {}
#define satNarratef_P(...) ((void)0)
#define snprintf_P snprintf
static size_t strlcpy_P(char* d, const char* s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
#define SATDebugf(...) ((void)0)
#define DUE(t) (false)
#define timerSATSuppress 0
bool satPwmIsIdle();
uint32_t satPwmLastOnTimeMs();
uint32_t satPwmLastOffTimeMs();
void satPwmReset();
"""

MAIN = r"""
bool satPwmIsIdle() { return _pwm_idle; }
uint32_t satPwmLastOnTimeMs() { return _pwm_lastOnTimeMs; }
uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }
static float g_pid = 47.72f;                 // OFF: duty 0.90 (range 4, off 180 s)
static const float PID_OFF = 47.72f, PID_ON = 29.5f;   // ON: duty 0.10 (range 2, on 180 s)
static float g_cs = 0;
static void edge(bool on) { g_flame = on; satCycleOnFlameChange(on); }
static float step() {
  g_cs = satApplyPWM(g_pid); _sat_lastControlCS = g_cs; state.sat.fFinalSetpoint = g_cs; return g_cs;
}
static int req() { return (int)state.sat.bPwmFlameRequested; }
// OFF phase: a step every 30 s until the ON phase starts or maxSec passes. 1 = started.
static int waitStart(int maxSec) {
  for (int t = 0; t <= maxSec; t += 30) { step(); if (req()) return 1; g_ms += 30000UL; }
  return 0;
}
// ON phase that waits for ignition: flame lights 10 s after the start; steps every 30 s until the ON phase ends; flame out.
static void burn() {
  g_pid = PID_ON; g_ms += 10000UL; edge(true);
  for (int i = 0; i < 400 && req(); i++) { g_ms += 30000UL; step(); }
  g_ms += 5000UL; edge(false); g_pid = PID_OFF;
}
static int pwmCycles(int n) {                 // n natural PWM cycles; returns how many started
  int started = 0;
  for (int i = 0; i < n; i++) { if (!waitStart(900)) break; started++; burn(); }
  return started;
}
static void dhwStart() {                      // a hot-water ignition: no PWM step runs while hot water is on
  state.sat.bDhwActive = true; OTcurrentSystemState.Statusflags = 0x0C;
  edge(true); g_ms += 120000UL; edge(false); g_ms += 60000UL;
  state.sat.bDhwActive = false; OTcurrentSystemState.Statusflags = 0x010A;
}
static unsigned cnt() { return (unsigned)satCycleGetCyclesThisHour(); }
// Continuous burn of burnSec, then the switch into PWM; returns after the entry step.
static void enterWithBurner(int offBeforeSec, int burnSec, float pid) {
  satSetControlMode(SAT_MODE_CONTINUOUS);
  g_pid = pid;
  g_ms += (uint32_t)offBeforeSec * 1000UL; edge(true);
  g_ms += (uint32_t)burnSec * 1000UL;
  satSetControlMode(SAT_MODE_PWM);
  step();
}
// Steps every 30 s while the ON phase lasts; returns seconds from the entry step to the ON end. 10 = CS cut seen while ON.
static int holdOn(int* cut, int maxSec) {
  int t = 0; *cut = 0;
  while (t < maxSec) { g_ms += 30000UL; t += 30; step(); if (!req()) break; if (g_cs <= SAT_MIN_SETPOINT) *cut = 1; }
  return t;
}
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  const char* sc = argv[1];
  state.sat.eControlMode = SAT_MODE_PWM; state.sat.bActive = true; state.sat.bValvesOpen = true;
  OTcurrentSystemState.Statusflags = 0x010A;
  settings.sat.bOvershootProtection = true;
  g_flow = 50.0f; g_cph = 3;
  if (!strcmp(sc, "shower")) {               // (a) 2 PWM starts + 3 hot-water flame starts, then the next PWM start
    const int s = pwmCycles(2);
    dhwStart(); dhwStart(); dhwStart();
    const unsigned before = cnt();
    g_pid = PID_OFF;
    const int ok = waitStart(900);
    if (ok) burn();                          // the ON phase is counted at its ignition
    printf("pwm=%d before=%u start=%d after=%u\n", s, before, ok, cnt());
    return 0;
  }
  if (!strcmp(sc, "slow")) {                 // regression guard: an ON start whose ignition takes several ticks
    edge(true); g_ms += 300000UL; edge(false); g_ms += 1000000UL;   // an older burn: on 410 s / off 790 s both passed
    g_pid = 35.0f;                           // duty 0.342: range 3, on 410 s, off 790 s
    int restarts = 0;
    for (int i = 0; i < 50; i++) { const int was = req(); step(); if (!was && req()) restarts++; g_ms += 30000UL; }   // TASK-1382: no abort at 180 s (pwm.py:194-199), the flameless ON phase ends at its ON time (410 s), OFF 790 s from that switch (pwm.py:155, :208), then the next ON start: 2 flameless starts in 1500 s
    edge(true); step();                      // ignition of the second phase
    const unsigned c1 = cnt();
    for (int i = 0; i < 400 && req(); i++) { g_ms += 30000UL; step(); }
    g_ms += 5000UL; edge(false); g_pid = PID_OFF;
    const int more = pwmCycles(2);
    printf("restarts=%d count=%u more=%d after=%u\n", restarts, c1, more, cnt());
    return 0;
  }
  if (!strcmp(sc, "limit")) {                // (b) 3 PWM starts in the rolling hour at 3/h, then a 4th OFF->ON
    const int s = pwmCycles(3);
    const unsigned before = cnt();
    const int ok = waitStart(900);
    printf("pwm=%d before=%u start=%d after=%u\n", s, before, ok, cnt());
    return 0;
  }
  if (!strcmp(sc, "hplimit")) {              // TASK-1419 (entry 81): 3 PWM starts at 3/h, the real limit check, then a 4th OFF->ON. argv[2] = heating source
    g_source = atoi(argv[2]);
    const int s = pwmCycles(3);
    const unsigned before = cnt();
    const int limit = (int)satCycleIsHourLimitReached();
    const int ok = waitStart(900);
    if (ok) burn();
    printf("pwm=%d before=%u limit=%d start=%d after=%u\n", s, before, limit, ok, cnt());
    return 0;
  }
  if (!strcmp(sc, "range5burn")) {           // (c) duty range 5 ON start from the OFF phase with the flame burning (lit > 1 h ago)
    edge(true); g_ms += 3700000UL;
    g_pid = 50.0f;                           // duty 1.0 >= dutyMax 0.925
    step();
    const unsigned atStart = cnt(); const int r = req();
    for (int i = 0; i < 5; i++) { g_ms += 30000UL; step(); }
    printf("req=%d start=%u later=%u\n", r, atStart, cnt());
    return 0;
  }
  if (!strcmp(sc, "reseed5lit")) {           // TASK-1385: range-5 ON start with the flame lit reseeds BT from the live flow (pwm.py:164-167)
    edge(true); g_ms += 600000UL;
    _pwm_effectiveBoilerTemp = 40.0f;        // the old effective temperature, different from the live flow
    g_flow = 55.0f; g_pid = 50.0f;           // duty 1.0: range 5
    step();
    printf("req=%d eff=%.1f\n", req(), _pwm_effectiveBoilerTemp);
    return 0;
  }
  if (!strcmp(sc, "reseed5ign")) {           // TASK-1385: range-5 ON start with the flame off keeps BT, the ignition sync reseeds it (pwm.py:186-193)
    _pwm_effectiveBoilerTemp = 40.0f;
    g_flow = 55.0f; g_pid = 50.0f;
    step();
    const float atStart = _pwm_effectiveBoilerTemp; const int r = req();
    g_ms += 10000UL; edge(true); step();
    printf("req=%d start=%.1f ign=%.1f\n", r, atStart, _pwm_effectiveBoilerTemp);
    return 0;
  }
  if (!strcmp(sc, "range5")) {               // (c) duty range 5 ON start with the flame off: counted at ignition, once (a relight is not a start)
    g_pid = 50.0f;
    step();
    const unsigned atStart = cnt(); const int r = req();
    g_ms += 10000UL; edge(true);
    for (int i = 0; i < 3; i++) { g_ms += 30000UL; step(); }
    const unsigned lit = cnt();
    edge(false); g_ms += 30000UL; step(); g_ms += 10000UL; edge(true);    // flame-out and relight inside the phase
    for (int i = 0; i < 3; i++) { g_ms += 30000UL; step(); }
    printf("req=%d start=%u lit=%u later=%u\n", r, atStart, lit, cnt());
    return 0;
  }
  if (!strcmp(sc, "entry")) {                // (d) continuous burn, switch into PWM with the burner on. argv: offBefore burn
    const int offBefore = atoi(argv[2]), burnSec = atoi(argv[3]);
    edge(true); g_ms += 300000UL; edge(false);    // an earlier burn: the flame-off the old OFF wait is timed from
    enterWithBurner(offBefore, burnSec, 40.0f);   // duty (40-27.2)/(50-27.2) = 0.561: range 3, on 673 s
    const int r0 = req(); const float cs0 = g_cs; const unsigned c0 = cnt();
    int cut = 0; const int onSec = holdOn(&cut, 1500);
    printf("req=%d cs=%.1f count=%u cut=%d onsec=%d offset=%.1f\n", r0, cs0, c0, cut, onSec, settings.sat.fModSupOffset);
    return 0;
  }
  if (!strcmp(sc, "entry4")) {               // (e)+(f) entry with the burner on as the 4th start of the hour, then the next OFF->ON
    const int s = pwmCycles(3);
    const unsigned before = cnt();
    enterWithBurner(100, 120, 40.0f);
    const int r0 = req(); const float cs0 = g_cs; const unsigned c0 = cnt();
    int cut = 0; const int onSec = holdOn(&cut, 1500);
    g_ms += 5000UL; edge(false);
    g_pid = PID_OFF;
    const int next = waitStart(600);
    printf("pwm=%d before=%u req=%d cs=%.1f count=%u cut=%d onsec=%d next=%d after=%u offset=%.1f\n", s, before, r0, cs0,
           c0, cut, onSec, next, cnt(), settings.sat.fModSupOffset);
    return 0;
  }
  if (!strcmp(sc, "entryoff")) {             // entry with the flame off as the 4th start: an ignition, the limit applies
    const int s = pwmCycles(3);
    satSetControlMode(SAT_MODE_CONTINUOUS);
    g_ms += 60000UL;
    satSetControlMode(SAT_MODE_PWM);
    g_pid = 40.0f; step();
    printf("pwm=%d req=%d count=%u\n", s, req(), cnt());
    return 0;
  }
  if (!strcmp(sc, "entryfresh")) {           // entry with the flame off, limit not reached, off wait not over: ON at once
    edge(true); g_ms += 600000UL; edge(false);  // last flame-off just now
    satSetControlMode(SAT_MODE_CONTINUOUS);
    g_ms += 60000UL;                            // 60 s since flame-off, off time 527 s
    satSetControlMode(SAT_MODE_PWM);
    g_pid = 40.0f; step();
    const int r = req(); const unsigned c0 = cnt();
    g_ms += 20000UL; edge(true); g_ms += 10000UL; step();   // ignition: the ON phase is counted now
    printf("req=%d count=%u lit=%u\n", r, c0, cnt());
    return 0;
  }
  if (!strcmp(sc, "entrylow")) {             // entry with ON time 0 (range 1, no flame): no ON at entry (pwm.py:155)
    edge(true); g_ms += 600000UL; edge(false);
    satSetControlMode(SAT_MODE_CONTINUOUS);
    g_ms += 60000UL;
    satSetControlMode(SAT_MODE_PWM);
    g_pid = 28.5f; step();                    // duty 0.057 < dutyMin 0.075
    printf("req=%d count=%u\n", req(), cnt());
    return 0;
  }
  if (!strcmp(sc, "entry5")) {               // entry in duty range 5 after 20 min of continuous burn, then range 3 (on 673 s)
    edge(true); g_ms += 300000UL; edge(false);
    enterWithBurner(1500, 1200, 50.0f);           // duty 1.0: range 5
    const int r0 = req(); const float cs0 = g_cs; const unsigned c0 = cnt();
    g_pid = 40.0f;
    int cut = 0; const int onSec = holdOn(&cut, 1500);
    printf("req=%d cs=%.1f count=%u cut=%d onsec=%d offset=%.1f\n", r0, cs0, c0, cut, onSec, settings.sat.fModSupOffset);
    return 0;
  }
  if (!strcmp(sc, "lag")) {                  // the flame is still lit one tick after ON->OFF (slow stop / CS latency)
    edge(true); g_ms += 300000UL; edge(false); g_ms += 2000000UL;   // an older burn: its flame-off is long past
    g_pid = PID_OFF;
    const int ok = waitStart(900);
    g_pid = PID_ON; g_ms += 10000UL; edge(true);
    for (int i = 0; i < 400 && req(); i++) { g_ms += 30000UL; step(); }
    const unsigned c1 = cnt();
    g_ms += 30000UL; step();                 // flame still lit after ON->OFF (off time 1620 s at duty 0.10)
    const int reon = req();
    g_ms += 5000UL; edge(false);
    printf("ok=%d c1=%u reon=%d c2=%u\n", ok, c1, reon, cnt());
    return 0;
  }
  if (!strcmp(sc, "entryflow")) {            // entry before MsgID 25 is seen: the first flow tick still skips the off wait
    edge(true); g_ms += 600000UL; edge(false);
    satSetControlMode(SAT_MODE_CONTINUOUS);
    g_ms += 60000UL; g_flow = 0.0f;
    satSetControlMode(SAT_MODE_PWM);
    g_pid = 40.0f; step();                   // no flow temperature: PWM idle
    const int r0 = req();
    g_ms += 30000UL; g_flow = 50.0f; step(); // flow arrives 90 s after the flame-off, off time 527 s
    printf("r0=%d req=%d\n", r0, req());
    return 0;
  }
  if (!strcmp(sc, "entrylimit")) {           // entry with the flame off at the limit, a hot-water burn just before the hour clears
    const int s = pwmCycles(3);
    satSetControlMode(SAT_MODE_CONTINUOUS); g_ms += 60000UL; satSetControlMode(SAT_MODE_PWM);
    g_pid = 40.0f; step();
    const int r0 = req();
    const uint32_t oldest = _hourCycleTs[(_hourCycleHead + SAT_RING_N - 3) % SAT_RING_N];
    int dhw = 0, onAtClear = -1;
    for (int i = 0; i < 400 && onAtClear < 0; i++) {
      if (!dhw && g_ms - oldest >= 3600000UL - 240000UL) { dhwStart(); dhw = 1; }   // a fresh flame-off ~60 s before the hour clears
      g_ms += 30000UL;
      const unsigned before = cnt();
      step();
      if (dhw && before < 3) onAtClear = req();
    }
    printf("pwm=%d r0=%d dhw=%d onAtClear=%d\n", s, r0, dhw, onAtClear);
    return 0;
  }
  if (!strcmp(sc, "wrap")) {                 // 3 PWM starts, an hour without starts, then millis() wraps around
    const int s = pwmCycles(3);
    const uint32_t newest = _hourCycleTs[(_hourCycleHead + SAT_RING_N - 1) % SAT_RING_N];
    g_ms += 3700000UL;
    const unsigned c0 = cnt();               // the status publish reads the count while the starts are stale
    g_ms = newest + 60000UL;                 // 2^32 ms later: all three old timestamps look recent again
    const unsigned wc = cnt();
    g_pid = PID_OFF;
    const int ok = waitStart(900);
    printf("pwm=%d c0=%u wrapcount=%u start=%d\n", s, c0, wc, ok);
    return 0;
  }
  return 2;
}
"""


def extract_ring(src: str):
    """(ring, tail): the rolling-hour ring block, and the accessors + satCycleSkipAutoSwitch() (after the cycles cut)."""
    src = src.replace("\r\n", "\n")
    globs = re.findall(r"^static \w+\s+_hourCycle\w*(?:\[[^\]]*\])?[^;\n(]*;", src, re.M)
    # SAT_MAX_CYCLES_PER_HOUR comes again with the cycles cut (after the ring): the ring uses a renamed copy
    size = cls_harness._grab(r"^static const uint8_t SAT_MAX_CYCLES_PER_HOUR\s*=\s*(\d+);", src, "ring size").group(1)
    a = cls_harness._grab(r"^//=== Per-hour (?:cycle|PWM start) counter helpers", src, "per-hour ring block").start()
    b = src.index("//=== Rolling 4-hour window statistics")
    globs = ["static const uint8_t SAT_RING_N = %s;" % size] + [g.replace("SAT_MAX_CYCLES_PER_HOUR", "SAT_RING_N") for g in globs]
    src = src[:a] + src[a:b].replace("SAT_MAX_CYCLES_PER_HOUR", "SAT_RING_N") + src[b:]
    b = src.index("//=== Rolling 4-hour window statistics")
    acc = re.findall(r"^uint32_t satCycleGetFlame(?:On|Off)StartMs\(\)\s*\{[^}]*\}", src, re.M)
    skip = cls_harness._grab(r"^void satCycleSkipAutoSwitch\(\)\s*\{.*?^\}", src, "satCycleSkipAutoSwitch").group(0)
    return "\n".join(globs) + "\n" + src[a:b] + "\n", "\n".join(acc) + "\n" + skip + "\n"


def extract_pwm(src: str) -> str:
    src = src.replace("\r\n", "\n")
    a = src.index("static float    _pwm_effectiveBoilerTemp")
    b = src.index("// Last PWM off-phase duration in ms")
    return src[a:b]


FWD = r"""
bool satCycleIsHourLimitReached();
uint8_t satCycleGetCyclesThisHour();
void satCycleRecordPwmStart();
uint32_t satCycleGetFlameOnStartMs();
uint32_t satCycleGetFlameOffStartMs();
void satCycleSkipAutoSwitch();
void satCycleResetSustainGuards();
"""


def build(wd: Path):
    cycles = CYCLES_INO.read_text(encoding="utf-8")
    sat = SAT_INO.read_text(encoding="utf-8")
    types_src = SATTYPES_H.read_text(encoding="utf-8").replace("\r\n", "\n")
    ring, tail = extract_ring(cycles)
    cpp = wd / "t.cpp"
    # order: the PWM block first (it defines _sat_requestedSetpoint, read by satCycleOnFlameChange), then the ring
    # (called by satCycleOnFlameChange on HEAD), then the cycles cut
    cpp.write_text(cls_harness.PRELUDE + cls_harness.extract_types(types_src, True) + STUBS + FWD
                   + extract_pwm(sat) + ring + cls_harness.extract_cycles(cycles) + tail + MAIN, encoding="utf-8")
    return compile_cpp(cpp, wd / "t.exe")


class SatPwmStartLimit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_sc(self, *args):
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    # (a) TASK-1401 AC#1: hot-water ignitions do not count (entry 59; pwm.py:159-160)
    def test_a_shower_does_not_block_the_next_pwm_start(self):
        r = self.run_sc("shower")
        self.assertEqual(r["pwm"], "2")
        self.assertEqual(r["before"], "2", "hot-water flame starts were counted against cycles per hour: %r" % r)
        self.assertEqual(r["start"], "1", "PWM ON start blocked after 2 PWM starts + 3 hot-water starts: %r" % r)
        self.assertEqual(r["after"], "3")

    # TASK-1419 AC#1 (Sergeant D entry 81): a heat pump has no hourly start limit, gas and hybrid keep it.
    # satCycleIsHourLimitReached() is the real function from SATcycles.ino; only the source getter is stubbed.
    def test_hp_has_no_hourly_start_limit(self):
        r = self.run_sc("hplimit", 2)             # SAT_SRC_HEAT_PUMP
        self.assertEqual((r["pwm"], r["before"]), ("3", "3"), r)
        self.assertEqual(r["limit"], "0", "heat pump reports the hour limit reached: %r" % r)
        self.assertEqual(r["start"], "1", "heat pump 4th start in the hour was held by the hour limit: %r" % r)
        self.assertEqual(r["after"], "4", "the 4th heat-pump start is still recorded for cycles_this_hour: %r" % r)

    def test_gas_and_hybrid_keep_the_hourly_start_limit(self):
        for src in (1, 3):                        # SAT_SRC_GAS_BOILER, SAT_SRC_HYBRID
            with self.subTest(source=src):
                r = self.run_sc("hplimit", src)
                self.assertEqual(r["limit"], "1", r)
                self.assertEqual(r["start"], "0", r)
                self.assertEqual(r["after"], "3", r)

    # (b) TASK-1401 AC#2: the 4th PWM ON start in the rolling hour at 3/h is still blocked
    def test_b_fourth_pwm_start_in_the_hour_is_blocked(self):
        r = self.run_sc("limit")
        self.assertEqual((r["pwm"], r["before"]), ("3", "3"), r)
        self.assertEqual(r["start"], "0", "4th OFF->ON start at 3 cycles/h was not blocked: %r" % r)
        self.assertEqual(r["after"], "3")

    # (c) a duty-range-5 ON start counts as a PWM ON start, once per phase (not every tick, not per relight)
    def test_c_range5_on_start_with_flame_burning_counts_once(self):
        r = self.run_sc("range5burn")
        self.assertEqual(r["req"], "1")
        self.assertEqual(r["start"], "1", "range-5 ON start not counted as a PWM ON start: %r" % r)
        self.assertEqual(r["later"], "1", "range-5 ON phase counted more than once: %r" % r)

    def test_c_range5_on_start_counts_at_the_start_not_at_ignition_or_relight(self):
        r = self.run_sc("range5")
        # TASK-1421, entry 90 (pwm.py:160): the start counts at the ON start, lit or not; the ignition adds nothing.
        # Before: start=0 (counted only at the ignition, lit=1). Now: start=1, lit=1.
        self.assertEqual((r["req"], r["start"]), ("1", "1"), r)   # counted at the start tick although no flame burns yet
        self.assertEqual(r["lit"], "1", "range-5 ON phase: the ignition added a count: %r" % r)
        self.assertEqual(r["later"], "1", "a relight inside the range-5 ON phase was counted: %r" % r)

    # TASK-1385 (thermo-nova pwm.py:164-167): a range-5 ON start with the flame lit reseeds the effective boiler
    # temperature from the live flow. Red when the reseed in satPwmBeginOn() is removed (eff stays 44.5 via the EMA).
    def test_c_range5_on_start_with_flame_lit_reseeds_effective_temp_from_live_flow(self):
        r = self.run_sc("reseed5lit")
        self.assertEqual(r["req"], "1", r)
        self.assertEqual(float(r["eff"]), 55.0, "effective boiler temperature not reseeded from the live flow at the range-5 ON start: %r" % r)

    # TASK-1385 (pwm.py:164, :186-193): with the flame off the start keeps the old value, the ignition sync reseeds it.
    def test_c_range5_on_start_with_flame_off_reseeds_at_ignition(self):
        r = self.run_sc("reseed5ign")
        self.assertEqual(r["req"], "1", r)
        self.assertEqual(float(r["start"]), 40.0, "flame-off range-5 start must not reseed yet: %r" % r)
        self.assertEqual(float(r["ign"]), 55.0, "ignition sync did not reseed the effective temperature: %r" % r)

    # TASK-1421, entry 90 (Q49 B, pwm.py:159-160): every ON start counts, lit or not, so flameless ON restarts (R3 / Q35: phases that
    # end at their ON time) do use up the hourly limit. Before (first-flame counting): count=1 more=2. Now: count=2 more=1 (cph 3).
    def test_flameless_on_restarts_count_against_the_limit(self):
        r = self.run_sc("slow")
        self.assertGreaterEqual(int(r["restarts"]), 2, "scenario did not reproduce the restarts before ignition: %r" % r)
        self.assertEqual(r["count"], "2", "flameless ON starts were not counted: %r" % r)
        self.assertEqual(r["more"], "1", "the 2 flameless starts did not use up the hourly limit: %r" % r)
        self.assertEqual(r["after"], "3", r)

    # (d) TASK-1369: continuous -> PWM with the burner on: first phase ON, no CS=10 cut, counted, lasts its ON time
    def assertEntryKeepsBurning(self, r):
        self.assertEqual(r["req"], "1", "first PWM phase is not ON: %r" % r)
        self.assertEqual(float(r["cs"]), 50.0 - float(r["offset"]), "entry CS is not flow - offset (entry 22): %r" % r)
        self.assertEqual(r["cut"], "0", "CS cut to 10 during the entry ON phase: %r" % r)
        self.assertGreaterEqual(int(r["onsec"]), 673, "entry ON phase ended before its ON time (673 s): %r" % r)
        self.assertLessEqual(int(r["onsec"]), 703, r)

    def test_d_entry_short_burn_first_phase_on(self):
        r = self.run_sc("entry", 100, 60)       # flame off 100 s, then 60 s of continuous burn: the off wait is not over
        self.assertEntryKeepsBurning(r)
        self.assertEqual(r["count"], "1", "entry ON phase not counted as a PWM ON start: %r" % r)

    def test_d_entry_long_burn_is_not_cut_on_the_next_tick(self):
        r = self.run_sc("entry", 1500, 1200)    # 20 min of continuous burn
        self.assertEntryKeepsBurning(r)
        self.assertEqual(r["count"], "1", r)

    # (e) the entry as the 4th start of the hour: still ON, not cut (maintainer decision, Q48 pending)
    def test_e_entry_at_the_limit_is_not_blocked(self):
        r = self.run_sc("entry4")
        self.assertEqual((r["pwm"], r["before"]), ("3", "3"), r)
        self.assertEqual(r["req"], "1", "entry ON phase blocked by the hourly limit: %r" % r)
        self.assertEqual(float(r["cs"]), 50.0 - float(r["offset"]), r)
        self.assertEqual(r["cut"], "0", r)
        self.assertEqual(r["count"], "4", "entry ON phase not counted: %r" % r)

    # (f) after that entry ON phase ends, the next OFF->ON at the limit is blocked
    def test_f_next_start_after_the_entry_is_blocked(self):
        r = self.run_sc("entry4")
        self.assertGreaterEqual(int(r["onsec"]), 673, r)
        self.assertEqual(r["next"], "0", "OFF->ON start after the entry phase was not blocked at the limit: %r" % r)
        self.assertEqual(r["after"], "4", r)

    # Scope of the exemption: only an entry with the burner burning; an entry with the flame off is an ignition
    def test_entry_with_flame_off_at_the_limit_is_blocked(self):
        r = self.run_sc("entryoff")
        self.assertEqual((r["pwm"], r["req"], r["count"]), ("3", "0", "3"), r)

    def test_entry_with_flame_off_starts_on_without_the_off_wait(self):
        r = self.run_sc("entryfresh")
        self.assertEqual(r["req"], "1", "entry with the flame off waited for the off time: %r" % r)
        # TASK-1421, entry 90: counted at the entry ON start (count=1), the ignition adds nothing. Before: count=0, lit=1.
        self.assertEqual((r["count"], r["lit"]), ("1", "1"), "entry ON phase not counted once at its start: %r" % r)

    def test_entry_with_on_time_below_180s_does_not_start(self):
        r = self.run_sc("entrylow")             # pwm.py:155: on_time_seconds >= HEATER_STARTUP_TIMEFRAME
        self.assertEqual((r["req"], r["count"]), ("0", "0"), r)

    # R2 review: an entry in duty range 5 anchors its ON phase (pwm.py:160-167), so a drop to range 3 is not cut at once
    def test_entry_in_range5_then_range3_keeps_its_on_time(self):
        r = self.run_sc("entry5")
        self.assertEqual(r["req"], "1", r)
        self.assertEqual(r["count"], "1", "range-5 entry ON phase not counted: %r" % r)
        self.assertEqual(r["cut"], "0", "CS cut to 10 after the drop from range 5 to range 3: %r" % r)
        self.assertGreaterEqual(int(r["onsec"]), 673, "range-5 ON phase timed from the old flame-on: %r" % r)
        self.assertLessEqual(int(r["onsec"]), 703, r)

    # R1 review: a flame still lit one tick after ON->OFF must not restart ON (and use up a start); the off wait
    # counts from the ON->OFF edge (pwm.py:208)
    def test_flame_lag_after_on_off_does_not_restart(self):
        r = self.run_sc("lag")
        self.assertEqual((r["ok"], r["c1"]), ("1", "1"), r)
        self.assertEqual(r["reon"], "0", "ON restarted at once with the flame still lit after ON->OFF: %r" % r)
        self.assertEqual(r["c2"], "1", r)

    # R2 review: the entry skip survives a missing flow temperature (pwm.py:123 keeps IDLE)
    def test_entry_before_flow_is_seen_starts_on_without_the_off_wait(self):
        r = self.run_sc("entryflow")
        self.assertEqual((r["r0"], r["req"]), ("0", "1"), "entry skip lost while the flow was missing: %r" % r)

    # R2 review: the entry skip survives the hour-limit return (pwm.py:156-158 keeps IDLE)
    def test_entry_blocked_by_the_limit_starts_on_once_it_clears(self):
        r = self.run_sc("entrylimit")
        self.assertEqual((r["pwm"], r["r0"], r["dhw"]), ("3", "0", "1"), r)
        self.assertEqual(r["onAtClear"], "1", "entry ON did not start when the limit cleared (off wait applied): %r" % r)

    # R1 review: after a millis() wrap old starts must not come back into the rolling hour
    def test_millis_wrap_does_not_revive_old_starts(self):
        r = self.run_sc("wrap")
        self.assertEqual((r["pwm"], r["c0"]), ("3", "0"), r)
        self.assertEqual(r["wrapcount"], "0", "stale starts counted again after the millis() wrap: %r" % r)
        self.assertEqual(r["start"], "1", r)


if __name__ == "__main__":
    unittest.main(verbosity=2)
