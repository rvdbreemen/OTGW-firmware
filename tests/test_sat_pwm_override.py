#!/usr/bin/env python3
"""TASK-1268: host test for the SAT PWM control-setpoint override and the MM selection.

The code under test is NOT re-implemented here. The test cuts the real PWM block
(from `_pwm_effectiveBoilerTemp` up to the satPwmLastOffTimeMs() accessor: schedule,
satApplyPWM(), the 3 s satPwmSuppressionRefresh(), satPwmReset(), satSetControlMode()) and
satComputeMM() out of src/OTGW-firmware/SATcontrol.ino, plus the sustained switches (satCycleCheckOvershootSwitch(), satCycleCheckPwmExits()) and
its constants out of SATcycles.ino, wraps them in a stub header and compiles them with a
host C++ compiler (g++, clang++ or MSVC cl via vswhere). Without a compiler the tests
are skipped, they do not pass silently.

OTGW_SAT_INO and OTGW_SATCYCLES_INO point at other copies, e.g. the output of
`git show HEAD:src/OTGW-firmware/SATcontrol.ino`, for old-vs-fix runs. On HEAD there is
no overshoot_protection gate, no satComputeMM(), no 3 s refresh and no PWM reset, so those
cases fail.

The "tick" scenario also cuts the real requested-setpoint -> PWM/continuous step -> final clamp
segment out of satControlLoopRun() (TASK-1323: solar offset on the requested setpoint). The
PID integral freeze (TASK-1323) is compiled from the real _pidUpdateIntegral() in SATpid.ino
(OTGW_SATPID_INO for old-vs-fix). Not reachable from here: the rest of satControlLoopRun()
(CH queueing), covered by tests/test_sat_ch_enable.cpp plus source-level checks. The cold cutoff
reaction (TASK-1327) is the real satPwmDisableOnHeaterOff(), cut out with the PWM block and called by the
"cutoff" scenario under the same condition as the loop (satColdCutoff from SATchEnable.h); that the loop
calls it inside `if (coldCutoff)` is a source-level check (ColdCutoffSource).

Run: python tests/test_sat_pwm_override.py
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
PID_INO = Path(os.environ.get("OTGW_SATPID_INO", REPO / "src" / "OTGW-firmware" / "SATpid.ino"))
CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", REPO / "src" / "OTGW-firmware" / "SATcycles.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#define F(x) x
#define PSTR(x) x
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
static void satNarratef_P(const char *fmt, ...) {   // SAT log sink: one "N <text>" line on stderr (stdout carries the timeline)
  char b[96]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); fprintf(stderr, "N %s\n", b);
}
#define SAT_MIN_SETPOINT 10.0f
#define SAT_QUIRK_MIN_MOD_10 0x01
#define SAT_QUIRK_IMMERGAS_TP 0x02
enum { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
typedef int SATControlMode;
struct SatSettings { @FORCEFIELD@ @AUTOFIELD@ float fOvershootMargin=2.0f; bool bEnabled=true, bOvershootProtection=true, bSolarGainEnable=false, bSummerSimmer=false;
                     int iMaxRelModulation=80; float fModSupDelay=20, fModSupOffset=1, fFlameOffOffset=18, fSolarSetpointOffset=2; };
struct Settings { SatSettings sat; };
struct SatState { int iCurrentModulation=0; float fFinalSetpoint=0, fPwmDutyCycle=0, fPidOutput=0; bool bPwmFlameRequested=false, bDhwActive=false,
                  bActive=true, bSafetyTripped=false, bValvesOpen=true, bSummerActive=false, bSolarGainActive=false;
                  int eControlMode=SAT_MODE_PWM; };
struct State { SatState sat; };
struct OTSys { int SlaveStatus = 0; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static bool satIsEnabled() { return settings.sat.bEnabled; }   // TASK-1345
static uint32_t g_ms = 1000000; static bool g_flame = false; static float g_flow = 33, g_ret = 30;
static uint32_t g_flameOnStart = 0, g_flameOffStart = 0; static bool g_due = true; static bool g_hourLimit = false; static int g_pwmStarts = 0;
static int g_csSent = 0; static float g_csLast = 0;
static uint32_t millis() { return g_ms; }
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
static float satApplyContinuous(float p) { return 1000.0f + p; }   // marker value: "the continuous setpoint was sent"
void satPwmReset();                                                 // defined later in the cut block (the .ino gets Arduino prototypes)
static float satGetReturnTemp() { return g_ret; }
static float satGetBaseOffset() { return 27.2f; }
static float g_cold = 28.2f;
static float satGetColdSetpoint() { return g_cold; }
static float satGetMaxSetpoint() { return 62.0f; }
static float satEffectiveMaxSetpoint() { return 62.0f; }
static uint32_t satGetMinOnTimeSec() { return 180; }
static uint8_t satGetMaxCyclesPerHour() { return 4; }
static uint32_t satGetPwmPeriodSec() { return 3600 / satGetMaxCyclesPerHour(); }   // TASK-1366: the PWM period getter (unused on HEAD)
static uint32_t satCycleGetFlameOnStartMs() { return g_flameOnStart; }
static uint32_t satCycleGetFlameOffStartMs() { return g_flameOffStart; }
static bool satCycleIsHourLimitReached() { return g_hourLimit; }
static void satCycleRecordPwmStart() { g_pwmStarts++; }             // counts the calls (TASK-1421; the ring itself: tests/test_sat_pwm_start_limit.py)
static bool g_hasOT = true;
static bool hasOTCommandInterface() { return g_hasOT; }
static bool satEnqueueIfChangedCS(float cs) { g_csSent++; g_csLast = cs; return true; }
void satCycleResetSustainGuards();                                    // defined in the cut SATcycles block
void satCycleSkipAutoSwitch();                                        // idem (TASK-1367; a no-op stub on HEAD)
#define DUE(t) (g_due)
#define timerSATSuppress 0
#include "@CHENABLE@"
"""

MAIN = r"""
static void reset() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 33; g_ret = 30; g_due = true; g_hourLimit = false; g_csSent = 0; g_csLast = 0; g_hasOT = true; g_cold = 28.2f;
  _pwm_waitingForFlame = false; _pwm_flameOffHoldSetpoint = 0; _pwm_flameOnMs = 0; _pwm_lastOffTimeMs = 1;
  _pwm_effectiveBoilerTemp = 0; _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
}
#ifdef HAS_REQSP
#define SETREQ(v) (_sat_requestedSetpoint = (v))
#else
#define SETREQ(v) (state.sat.fPidOutput = (v))
#endif
static void setPrev(float cs) { _sat_lastControlCS = cs; state.sat.fFinalSetpoint = cs; }
int main(int argc, char** argv) {
  if (argc < 3) return 2;
  const char* sc = argv[1];
  if (!strcmp(sc, "mm")) {   // mm pwm hp hyb maxrel quirks
    if (argc != 7) return 2;
    printf("%u\n", (unsigned)satComputeMM(atoi(argv[2]) != 0, atoi(argv[3]) != 0, atoi(argv[4]) != 0,
                                          (uint8_t)atoi(argv[5]), (uint8_t)atoi(argv[6])));
    return 0;
  }
  reset();
  settings.sat.bOvershootProtection = atoi(argv[2]) != 0;
  _pwm_effectiveBoilerTemp = 47.2f;        // a BT learned in an earlier cycle (TASK-1273: no placeholder any more); keeps the legacy scenarios at duty 0.39
  const float pid = 35.0f;                 // duty (35-27.2)/20 = 0.39: range 3, on 351 s, off 549 s
  state.sat.fPidOutput = pid; SETREQ(pid);
  setPrev(28.0f);                          // previous CS (what the old +18 path produced)
  if (!strcmp(sc, "sw")) {                 // TASK-1341: sustained switches. argv[3]=PWM auto switch setting (HEAD only) argv[4]=os|uh|sat
#ifdef HAS_AUTOSWITCH
    settings.sat.bPwmAutoSwitch = atoi(argv[3]) != 0;
#endif
    const bool os = !strcmp(argv[4], "os");
    const bool sat = !strcmp(argv[4], "sat");
    state.sat.eControlMode = os ? SAT_MODE_CONTINUOUS : SAT_MODE_PWM;
    state.sat.bPwmFlameRequested = !os; state.sat.fFinalSetpoint = pid; SYNC_SUSTAIN_CLOCK();
    g_flow = os ? 45.0f : (sat ? 35.0f : 20.0f);     // overshoot: flow above request + margin; underheat: far below; saturation: on target
    _pwm_lastOffTimeMs = sat ? 0 : 1;                // saturation: the PWM off time stays 0
    int calls = 0;
    for (int i = 0; i < 400 && state.sat.eControlMode == (os ? SAT_MODE_CONTINUOUS : SAT_MODE_PWM); i++) { g_ms += 1000; CALL_AUTOSWITCH(pid); calls++; }
    printf("mode=%d calls=%d\n", state.sat.eControlMode, calls);
    return 0;
  }
  if (!strcmp(sc, "idle")) {               // TASK-1387: PWM IDLE flag (pwm.py PWMStatus.IDLE), read by the cycle classifier
#ifdef HAS_IDLE
    g_flameOffStart = g_ms - 600000UL; _pwm_lastUpdateMs = g_ms - 600000UL;     // long OFF: the schedule step would start an ON phase
    state.sat.eControlMode = SAT_MODE_PWM;
    _pwm_idle = false; satPwmReset();                         const int a = (int)_pwm_idle;   // reset -> IDLE (pwm.py:92)
    satApplyPWM(pid);                                         const int b = (int)_pwm_idle;   // first step with a flow -> not IDLE (pwm.py:154-180)
    // duty 0.39 (range 3, BT 47.2 as the legacy scenarios): range 5 would return before the hour-limit check
    satPwmReset(); _pwm_lastUpdateMs = g_ms - 600000UL; /* reset() timestamps now (pwm.py:91): age the off wait */ _pwm_effectiveBoilerTemp = 47.2f; g_hourLimit = true; satApplyPWM(pid); const int c = (int)_pwm_idle; // hour-limit return keeps IDLE
    _pwm_idle = false; satApplyPWM(pid);                      const int c2 = (int)_pwm_idle;  // ... and keeps not-IDLE
    g_hourLimit = false;
    g_flow = 0.0f; satApplyPWM(pid);                          const int d = (int)_pwm_idle;   // no flow temperature -> IDLE (pwm.py:122-128)
    // overshoot switch CONTINUOUS -> PWM through the real satCycleCheckOvershootSwitch(): not IDLE on the switch tick
    state.sat.eControlMode = SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = false; _pwm_idle = true;
    g_flow = 45.0f; SYNC_SUSTAIN_CLOCK();
    for (int i = 0; i < 400 && state.sat.eControlMode == SAT_MODE_CONTINUOUS; i++) { g_ms += 1000; CALL_AUTOSWITCH(pid); }
    const int e = (int)_pwm_idle; const int emode = state.sat.eControlMode;
    // a switch to PWM without a flow temperature stays IDLE (the first step would idle it too)
    state.sat.eControlMode = SAT_MODE_CONTINUOUS; g_flow = 0.0f; satSetControlMode(SAT_MODE_PWM);
    const int f = (int)_pwm_idle;
    printf("reset=%d step=%d limit=%d limit_busy=%d noflow=%d switch=%d switchmode=%d switch_noflow=%d\n", a, b, c, c2, d, e, emode, f);
#else
    printf("reset=-1\n");
#endif
    return 0;
  }
  if (!strcmp(sc, "swtick")) {             // TASK-1420 loop level: the real overshoot gate + the real apply step in ONE tick
#ifdef HAS_SWITCH_TICK
    state.sat.eControlMode = SAT_MODE_CONTINUOUS; state.sat.bPwmFlameRequested = false;
    satPwmReset(); _pwm_effectiveBoilerTemp = 47.2f; g_flameOffStart = g_ms - 600000UL; _pwm_lastUpdateMs = g_ms - 600000UL;
    g_flow = 45.0f; setPrev(40.0f);        // 45 >= 35 + 2: a sustained overshoot
    float cs = 0; int ticks = 0; int contBefore = 0;
    for (; ticks < 400; ticks++) {
      g_ms += 1000; _sat_tickMs = g_ms;
      const int before = state.sat.eControlMode;
      cs = tickRunSw(pid);
      if (before == SAT_MODE_CONTINUOUS && state.sat.eControlMode == SAT_MODE_CONTINUOUS && cs >= 61.9f) contBefore++;
      if (state.sat.eControlMode == SAT_MODE_PWM) break;
    }
    const int ch = (int)satChEnableDecide(pid, satGetColdSetpoint(), g_swDrives, g_swPwmOn);
    printf("mode=%d ticks=%d cs=%.1f final=%.1f cont_marker=%d continuous_ticks=%d req=%d drives=%d ch=%d\n", state.sat.eControlMode, ticks + 1, cs,
           state.sat.fFinalSetpoint, (int)(cs >= 61.9f), contBefore, (int)g_swPwmOn, (int)g_swDrives, ch);
#else
    printf("mode=-1\n");
#endif
    return 0;
  }
  if (!strcmp(sc, "uh")) {                 // uh protection pwmFlameRequested: 200 s of low flow, 1 s calls
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = atoi(argv[3]) != 0;
    state.sat.fFinalSetpoint = pid; g_flow = 20.0f; SYNC_SUSTAIN_CLOCK();
    for (int i = 0; i < 200; i++) { g_ms += 1000; CALL_AUTOSWITCH(pid); }
    printf("mode=%d req=%d\n", state.sat.eControlMode, (int)state.sat.bPwmFlameRequested);
    return 0;
  }
  if (!strcmp(sc, "duty")) {               // TASK-1273: duty BT. argv[3]=flame argv[4]=flow argv[5]=pid argv[6]=seed(0/1) argv[7]=ms since flame-on (EMA window)
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    g_flame = atoi(argv[3]) != 0; g_flow = (float)atof(argv[4]); const float p = (float)atof(argv[5]);
    _pwm_effectiveBoilerTemp = atoi(argv[6]) ? 50.0f : 0.0f;       // seeded by an earlier cycle, or nothing learned yet
    uint32_t onAgo = (argc > 7) ? (uint32_t)atoi(argv[7]) : 5000UL;
    _pwm_flameOnMs = g_ms - onAgo; g_flameOnStart = g_ms - onAgo; _pwm_waitingForFlame = false;
    float cs = satApplyPWM(p);
    printf("duty=%.2f bt=%.1f req=%d cs=%.1f\n", state.sat.fPwmDutyCycle, _pwm_effectiveBoilerTemp,
           (int)state.sat.bPwmFlameRequested, cs);
    return 0;
  }
  if (!strcmp(sc, "sync")) {               // TASK-1273: ON phase waiting for ignition, flame appears at flow argv[3]
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    _pwm_waitingForFlame = true; _pwm_lastUpdateMs = g_ms - 5000UL; _pwm_flameOnMs = 0;
    _pwm_effectiveBoilerTemp = 50.0f; g_flame = true; g_flow = (float)atof(argv[3]); g_flameOnStart = g_ms;
    satApplyPWM(pid);
    printf("bt=%.1f wait=%d onago=%u\n", _pwm_effectiveBoilerTemp, (int)_pwm_waitingForFlame, (unsigned)(g_ms - _pwm_flameOnMs));
    return 0;
  }
  if (!strcmp(sc, "timeout")) {            // TASK-1273: flame-out mid-phase, no relight, ignition timeout aborts the ON phase
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    _pwm_waitingForFlame = true; _pwm_lastUpdateMs = g_ms - 181000UL; _pwm_flameOnMs = 0;
    _pwm_effectiveBoilerTemp = 46.0f; g_flame = false; g_flameOnStart = g_ms - 10000UL;
    satApplyPWM(pid);
    printf("req=%d wait=%d anchor=%u bt=%.1f\n", (int)state.sat.bPwmFlameRequested, (int)_pwm_waitingForFlame,
           (unsigned)_pwm_flameOnMs, _pwm_effectiveBoilerTemp);
    return 0;
  }
  if (!strcmp(sc, "relight")) {            // TASK-1273: flame-out and relight inside one ON phase, window already over
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    _pwm_waitingForFlame = false; _pwm_flameOnMs = g_ms - 200000UL; _pwm_lastUpdateMs = g_ms - 200000UL; g_flameOnStart = g_ms - 10000UL;
    _pwm_effectiveBoilerTemp = 46.0f; g_flame = true; g_flow = 50.0f;
    satApplyPWM(pid);                                                   // window over: BT stays 46
    g_ms += 30000UL; g_flame = false; satApplyPWM(pid);                 // flame out mid-phase: wait re-armed
    int rearmed = (int)_pwm_waitingForFlame;
    g_ms += 30000UL; g_flame = true; g_flow = 58.0f; satApplyPWM(pid);  // relight at a hotter flow
    printf("bt=%.1f rearmed=%d wait=%d onago=%u req=%d\n", _pwm_effectiveBoilerTemp, rearmed, (int)_pwm_waitingForFlame,
           (unsigned)(g_ms - _pwm_flameOnMs), (int)state.sat.bPwmFlameRequested);
    return 0;
  }
  if (!strcmp(sc, "reset")) {              // dirty state -> satPwmReset()/satSetControlMode()
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true; _pwm_waitingForFlame = true;
    _pwm_flameOffHoldSetpoint = 48; _pwm_flameOnMs = 123; _pwm_lastUpdateMs = 456; _pwm_effectiveBoilerTemp = 55;
    if (argc > 3 && !strcmp(argv[3], "mode")) satSetControlMode(SAT_MODE_CONTINUOUS); else satPwmReset();
    printf("req=%d wait=%d hold=%.1f onms=%u mode=%d bt=%.1f\n", (int)state.sat.bPwmFlameRequested, (int)_pwm_waitingForFlame,
           _pwm_flameOffHoldSetpoint, (unsigned)_pwm_flameOnMs, state.sat.eControlMode, _pwm_effectiveBoilerTemp);
    return 0;
  }
  if (!strcmp(sc, "tick")) {               // tick prot solar pid: first ON tick (ignition) through the real requested-setpoint segment
    state.sat.bSolarGainActive = atoi(argv[3]) != 0; settings.sat.bSolarGainEnable = atoi(argv[3]) != 0;
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 600000UL; _pwm_lastUpdateMs = g_ms - 600000UL;
    const float cs = tickRun((float)atof(argv[4]));
    printf("cs=%.1f req=%d\n", cs, (int)state.sat.bPwmFlameRequested);
    return 0;
  }
  if (!strcmp(sc, "cutoff")) {             // TASK-1327: held CS 45 (protection off), heater OFF, recovery. argv[3]=cut|valves
    settings.sat.bOvershootProtection = false;
#ifdef HAS_FORCE_PWM
    settings.sat.bForcePWM = true;           // HEAD only: the removed option, on, must no longer keep PWM
#endif
    const bool valves = argc > 3 && !strcmp(argv[3], "valves");
    state.sat.eControlMode = SAT_MODE_PWM;
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 600000UL; _pwm_lastUpdateMs = g_ms - 600000UL;
    setPrev(45.0f);
    auto tick = [&](float req, bool dirty) {   // tickRun + the lines of satControlLoopRun() after the cut segment
      SETREQ(req);
      if (dirty) { state.sat.bPwmFlameRequested = true; _pwm_flameOffHoldSetpoint = 48.0f; }   // dirty PWM phase
      float cs = tickRun(req);
      _sat_lastControlCS = cs;
      return cs;
    };
    const float held = tick(40.0f, true);  // above cold: PWM without override keeps prevCS 45
#ifdef HAS_OS_START
    _sustain_overshootRunning = true; _sustain_overshootStartMs = g_ms - 45000UL;   // TASK-1367: a started overshoot timer
#else
    _sustain_overshootSec = 45.0f;
#endif
#ifdef HAS_EXIT_START
    _sustain_underheatRunning = true; _sustain_underheatStartMs = g_ms - 12000UL;   // TASK-1420: started exit timers
    _sustain_saturationRunning = true; _sustain_saturationStartMs = g_ms - 7000UL;
#else
    _sustain_underheatSec = 12.0f; _sustain_saturationSec = 7.0f;
#endif
    state.sat.iCurrentModulation = 55;
    float cutoff = 0;
    if (valves) {                          // valves-closed early return of satControlLoopRun()
      state.sat.bValvesOpen = false; satPwmDisableOnHeaterOff();
    } else {                               // cold cutoff block
      cutoff = tick(20.0f, true);
      if (satColdCutoff(20.0f, satGetColdSetpoint())) satPwmDisableOnHeaterOff();
    }
    const int reqAfter = (int)state.sat.bPwmFlameRequested; const float holdAfter = _pwm_flameOffHoldSetpoint;
    const int modeAfter = state.sat.eControlMode; const float lastAfter = _sat_lastControlCS;
    const int mmAfter = state.sat.iCurrentModulation;
#if defined(HAS_OS_START) && defined(HAS_EXIT_START)
    const float sus = (_sustain_overshootRunning ? 45.0f : 0.0f) + (_sustain_underheatRunning ? 12.0f : 0.0f) + (_sustain_saturationRunning ? 7.0f : 0.0f);
#elif defined(HAS_OS_START)
    const float sus = (_sustain_overshootRunning ? 45.0f : 0.0f) + _sustain_underheatSec + _sustain_saturationSec;
#else
    const float sus = _sustain_overshootSec + _sustain_underheatSec + _sustain_saturationSec;
#endif
    state.sat.bValvesOpen = true;
    const float rec = tick(30.0f, false);  // solar clears / valves reopen: a clean phase, so the result shows the reset
    printf("held=%.1f cutoff=%.1f req=%d hold=%.1f mode=%d last=%.1f mm=%d sustain=%.1f recovery=%.1f\n", held, cutoff, reqAfter,
           holdAfter, modeAfter, lastAfter, mmAfter, sus, rec);
    return 0;
  }
  if (!strcmp(sc, "solar")) {              // three ticks, no override (protection off), solar offset subtracted after each
    setPrev(48.0f); state.sat.fFinalSetpoint = 46.0f;
    float cs = 0;
    for (int i = 0; i < 3; i++) { cs = satApplyPWM(pid); _sat_lastControlCS = cs; state.sat.fFinalSetpoint = cs - 2.0f; }
    printf("cs=%.1f\n", cs);
    return 0;
  }
  if (!strcmp(sc, "range5")) {             // duty >= 0.9: the tick and the 3 s refresh must send the same CS
    const float hi = 46.0f;
    setPrev(48.0f); state.sat.fPidOutput = hi; SETREQ(hi); g_flame = true; g_flameOnStart = g_ms - 60000UL; _pwm_flameOnMs = g_ms - 60000UL;
    float tick = satApplyPWM(hi);
    state.sat.fFinalSetpoint = tick;
    satPwmSuppressionRefresh();
    printf("tick=%.1f sent=%d refresh=%.1f\n", tick, g_csSent, g_csLast);
    return 0;
  }
  if (!strcmp(sc, "pending")) {            // flame already on when the ON phase starts; argv[3] = ms since flame on
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 600000UL; _pwm_lastUpdateMs = g_ms - 600000UL;
    g_flame = true; g_flameOnStart = g_ms - (uint32_t)atoi(argv[3]); _pwm_flameOnMs = g_flameOnStart;
    float cs = satApplyPWM(pid);
    printf("cs=%.1f req=%d wait=%d bt=%.1f onago=%u\n", cs, (int)state.sat.bPwmFlameRequested, (int)_pwm_waitingForFlame,
           _pwm_effectiveBoilerTemp, (unsigned)(g_ms - _pwm_flameOnMs));
    return 0;
  }
  if (!strcmp(sc, "off")) {
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 100000UL; _pwm_lastUpdateMs = g_ms - 100000UL;
  } else if (!strcmp(sc, "start")) {
    if (argc > 4) { g_cold = (float)atof(argv[3]); g_ret = (float)atof(argv[4]); }   // cold start: cold setpoint, return temperature
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 600000UL; _pwm_lastUpdateMs = g_ms - 600000UL;
  } else {                                  // hold, suppress, refresh
    setPrev(48.0f);
    state.sat.bPwmFlameRequested = true; g_flame = true;
    bool inHold = !strcmp(sc, "hold") || (argc > 3 && !strcmp(argv[3], "hold"));
    uint32_t onAgo = inHold ? 5000UL : 60000UL;
    _pwm_flameOnMs = g_ms - onAgo; g_flameOnStart = g_ms - onAgo; _pwm_flameOffHoldSetpoint = 48.0f;
  }
  if (!strcmp(sc, "refresh")) {
    if (argc > 3) {
      if (!strcmp(argv[3], "dhw")) state.sat.bDhwActive = true;
      if (!strcmp(argv[3], "cont")) state.sat.eControlMode = SAT_MODE_CONTINUOUS;
      if (!strcmp(argv[3], "cold")) { state.sat.fPidOutput = 20.0f; SETREQ(20.0f); }
      if (!strcmp(argv[3], "solar")) { state.sat.bSolarGainActive = true; settings.sat.bSolarGainEnable = true; }
      if (!strcmp(argv[3], "closed")) state.sat.bValvesOpen = false;
      if (!strcmp(argv[3], "thermal")) _sat_thermalSafeActive = true;
      if (!strcmp(argv[3], "noot")) g_hasOT = false;
    }
    g_flow = 41.0f;
    satPwmSuppressionRefresh();
    printf("sent=%d cs=%.1f final=%.1f\n", g_csSent, g_csLast, state.sat.fFinalSetpoint);
    return 0;
  }
  float cs = satApplyPWM(pid);
  printf("cs=%.1f req=%d\n", cs, (int)state.sat.bPwmFlameRequested);
  return 0;
}
"""


def extract_tick(src: str) -> str:
    """The real segment of satControlLoopRun(): solar offset (fix only) + clamp + mode step + final clamp."""
    loop = src[src.index("void satControlLoopRun"):] if "void satControlLoopRun" in src else src[src.index("void satControlLoop"):]
    solar = loop.find("  // --- Solar gain compensation on the requested setpoint")
    a = solar if solar >= 0 else loop.index("  // --- Clamp to valid range ---")
    ov = loop.find("  // --- Sustained overshoot: continuous -> PWM")
    if ov >= 0:
        # TASK-1420: the overshoot switch sits between the max-setpoint check and the mode step, the exits after the
        # final clamp. The switches are not under test here (tests/test_sat_overshoot_sustain.py), so both are cut out.
        ap = loop.index("  // --- Apply control mode ---", ov)
        ex = loop.index("  // --- Sustained underheat and saturation", ap)
        seg = loop[a:ov] + loop[ap:ex]
    else:
        seg = loop[a:loop.index("  // --- Auto-switch between continuous and PWM modes")]
    return ("\nstatic float tickRun(float pidOutput) {\n" + seg +
            "\n  return state.sat.fFinalSetpoint;\n}\n")


def extract_switch_tick(src: str) -> str:
    """TASK-1420: the whole real tick segment INCLUDING the real overshoot-switch gate (tickRunSw), or "" when the source has none."""
    loop = src[src.index("void satControlLoopRun"):] if "void satControlLoopRun" in src else ""
    ov = loop.find("  // --- Sustained overshoot: continuous -> PWM")
    if ov < 0:
        return ""
    solar = loop.find("  // --- Solar gain compensation on the requested setpoint")
    a = solar if solar >= 0 else loop.index("  // --- Clamp to valid range ---")
    ex = loop.index("  // --- Sustained underheat and saturation", ov)
    return ("\n#define HAS_SWITCH_TICK 1\nstatic uint32_t _sat_tickMs = 0; static bool g_swPwmOn = false, g_swDrives = false;\n"
            "bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs);\n"
            "static float tickRunSw(float pidOutput) {\n" + loop[a:ex] +
            "\n  g_swPwmOn = pwmOn; g_swDrives = pwmDrivesSetpoint;\n  return state.sat.fFinalSetpoint;\n}\n")


def extract(src: str) -> str:
    a = src.index("static float    _pwm_effectiveBoilerTemp")
    b = src.index("// Last PWM off-phase duration in ms")
    block = src[a:b]
    m = re.search(r"^static uint8_t satComputeMM\(.*?^\}\s*$", src, re.S | re.M)
    mm = m.group(0) if m else ""
    return block + "\n" + mm + "\n" + extract_tick(src)


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


def extract_cycles() -> str:
    src = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    consts = re.findall(r"^static (?:const )?\w+\s+(?:SAT_(?:OVERSHOOT|UNDERSHOOT|UNDERHEAT|SATURATION|DHW)\w*|_sustain_\w+)\s*=[^;]*;",
                        src, re.M)
    if "bool satCycleCheckOvershootSwitch(" in src:
        # TASK-1420: the sustained timers on the tick clock. Cut the whole state/helper block plus both switch functions.
        a = src.index("// --- Sustained State Detection ---")
        b = src.index("// --- Cycle History ---", a)
        blocks = []
        for head in ("bool satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs)",
                     "bool satCycleCheckPwmExits(float requestedSetpoint, uint32_t tickMs)"):
            m = re.search(r"^" + re.escape(head) + r"\s*\{.*?^\}[ \t]*$", src, re.S | re.M)
            if not m:
                raise AssertionError(head + " not found in SATcycles.ino")
            blocks.append(m.group(0))
        both = ("static bool satCycleCheckBoth(float sp) {   // the control loop order: overshoot switch, (apply), exits\n"
                "  const bool a = satCycleCheckOvershootSwitch(sp, g_ms);\n"
                "  const bool b = satCycleCheckPwmExits(sp, g_ms);\n  return a || b; }\n")
        sat_consts = "\n".join(c for c in consts if c.startswith("static const"))
        return (sat_consts + "\n" + src[a:b] + "\n" + "\n".join(blocks) + "\n" + both +
                "\n#define CALL_AUTOSWITCH(sp) satCycleCheckBoth(sp)\n#define SYNC_SUSTAIN_CLOCK() ((void)0)\n")
    m = re.search(r"^bool satCycleCheckAutoSwitch\([^)]*\)\s*\{.*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError("neither satCycleCheckOvershootSwitch() nor satCycleCheckAutoSwitch() found in SATcycles.ino")
    fn = m.group(0)
    call = "satCycleCheckAutoSwitch(sp)" if re.search(r"satCycleCheckAutoSwitch\(float", fn) else \
        "(state.sat.fFinalSetpoint = (sp), satCycleCheckAutoSwitch())"
    g = re.search(r"^void satCycleResetSustainGuards\(\)\s*\{.*?^\}\s*$", src, re.S | re.M)
    guards = g.group(0) if g else "void satCycleResetSustainGuards() { }"   # HEAD: no guard reset
    k = re.search(r"^void satCycleSkipAutoSwitch\(\)\s*\{.*?^\}\s*$", src, re.S | re.M)
    skip = k.group(0) if k else "void satCycleSkipAutoSwitch() { }"          # HEAD: no overshoot-start clear (TASK-1367)
    return ("\n".join(consts) + "\n" + guards + "\n" + skip + "\n" + fn + f"\n#define CALL_AUTOSWITCH(sp) {call}\n"
            "#define SYNC_SUSTAIN_CLOCK() (_sustain_lastCheckMs = g_ms)\n")


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


def build(workdir: Path):
    body = extract(SAT_INO.read_text(encoding="utf-8"))
    pre = ""
    for name, stub in (
        ("satComputeMM", "static uint8_t satComputeMM(bool,bool,bool,uint8_t,uint8_t){ return 255; }"),
        ("satPwmSuppressionRefresh", "static void satPwmSuppressionRefresh(){ }"),
        ("satPwmReset", "static void satPwmReset(){ }"),
        ("satSetControlMode", "static void satSetControlMode(int m){ state.sat.eControlMode = m; }"),
        ("_sat_lastControlCS", "static float _sat_lastControlCS = 0;"),
        ("_sat_thermalSafeActive", "static bool _sat_thermalSafeActive = false;"),
        ("satPwmDisableOnHeaterOff", "static void satPwmDisableOnHeaterOff(){ }"),   # HEAD: no cutoff reaction
    ):
        if name not in body:   # HEAD lacks these: keep the harness compiling so the cases fail, not the build
            pre += stub + "\n"
    cycles = "static uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }\n" + extract_cycles()
    cpp = workdir / "t.cpp"
    flag = "#define HAS_REQSP 1\n" if "_sat_requestedSetpoint" in body else ""
    # HEAD still has the Force PWM setting: the stub carries the field so HEAD compiles and the forced case shows the old behaviour
    has_force = "bForcePWM" in body
    if has_force:
        flag += "#define HAS_FORCE_PWM 1\n"
    # HEAD still gates satCycleCheckAutoSwitch() on settings.sat.bPwmAutoSwitch (TASK-1341): the stub carries the field so HEAD compiles
    has_auto = "bPwmAutoSwitch" in cycles
    if has_auto:
        flag += "#define HAS_AUTOSWITCH 1\n"
    # TASK-1367: the overshoot timer is a start timestamp, not a seconds counter
    if "_sustain_overshootRunning" in cycles:
        flag += "#define HAS_OS_START 1\n"
    # TASK-1420: the exit timers are start timestamps too
    if "_sustain_underheatRunning" in cycles:
        flag += "#define HAS_EXIT_START 1\n"
    # TASK-1387: the PWM IDLE flag read by the cycle classifier
    if "_pwm_idle" in body:
        flag += "#define HAS_IDLE 1\n"
    chenable = (REPO / "src" / "OTGW-firmware" / "SATchEnable.h").as_posix()
    cpp.write_text(flag + STUBS.replace("@FORCEFIELD@", "bool bForcePWM=false;" if has_force else "").replace("@AUTOFIELD@", "bool bPwmAutoSwitch=true;" if has_auto else "").replace("@CHENABLE@", chenable) + pre + body + cycles + extract_switch_tick(SAT_INO.read_text(encoding="utf-8")) + MAIN, encoding="utf-8")
    return compile_cpp(cpp, workdir / "t.exe")


PID_HARNESS = r"""
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define SAT_PID_UPDATE_INTERVAL 60.0f
struct SatSettings { float fDeadband = 0.1f; bool bSolarFreezeIntegral = true; };
struct Settings { SatSettings sat; };
struct SatState { float fKi = 0.01f; bool bSolarGainActive = false; };
struct State { SatState sat; };
static Settings settings; static State state;
static float _pid_integral = 0;
"""
PID_MAIN = r"""
int main(int argc, char** argv) {   // active freezeSetting error integral
  state.sat.bSolarGainActive = atoi(argv[1]) != 0; settings.sat.bSolarFreezeIntegral = atoi(argv[2]) != 0;
  _pid_integral = (float)atof(argv[4]);
  _pidUpdateIntegral((float)atof(argv[3]), 20.0f, true);
  printf("integral=%.2f\n", _pid_integral);
  return 0;
}
"""


def build_pid(workdir: Path):
    src = PID_INO.read_text(encoding="utf-8")
    m = re.search(r"^static void _pidUpdateIntegral\(.*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError("_pidUpdateIntegral() not found in SATpid.ino")
    cpp = workdir / "pid.cpp"
    cpp.write_text(PID_HARNESS + m.group(0) + PID_MAIN, encoding="utf-8")
    return compile_cpp(cpp, workdir / "pid.exe")


class SatPwmOverride(unittest.TestCase):
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

    has_reqsp = "_sat_requestedSetpoint" in SAT_INO.read_text(encoding="utf-8")

    def run_sc(self, *args):
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    # --- TASK-1341: the PWM auto switch setting is gone, the three sustained switches always run (Sergeant D entries 18, 19) ---
    # thermo-nova heating_control.py:206/217: no setting guards the overshoot / underheat / saturation switches.
    # On HEAD the stub still carries bPwmAutoSwitch and the scenario sets it to argv[3] (0 = off), which blocks all three.
    def test_sustained_overshoot_switches_to_pwm_even_with_the_old_setting_off(self):
        r = self.run_sc("sw", 1, 0, "os")
        self.assertEqual(r["mode"], "2")                  # CONTINUOUS -> PWM after SAT_OVERSHOOT_SUSTAIN_SEC
        self.assertLessEqual(int(r["calls"]), 61)

    def test_sustained_underheat_switches_to_continuous_even_with_the_old_setting_off(self):
        self.assertEqual(self.run_sc("sw", 1, 0, "uh")["mode"], "1")

    def test_sustained_saturation_switches_to_continuous_even_with_the_old_setting_off(self):
        self.assertEqual(self.run_sc("sw", 1, 0, "sat")["mode"], "1")

    def test_overshoot_switch_with_the_old_setting_on_is_unchanged(self):
        self.assertEqual(self.run_sc("sw", 1, 1, "os")["mode"], "2")

    # --- TASK-1273: duty-cycle BT (Sergeant D 2026-10-22, docs/sat/sergeant-d-corrections.md entry 1) ---
    def test_duty_uses_the_live_flow_at_the_first_flame_not_a_placeholder(self):
        # requested 40, boiler at 60, nothing learned yet: (40-27.2)/(60-27.2) = 0.39; the old
        # base+20 placeholder gave (40-27.2)/(47.2-27.2) = 0.64 and burned much longer per cycle
        r = self.run_sc("duty", 1, 1, 60.0, 40.0, 0)
        self.assertEqual(r["duty"], "0.39")
        self.assertEqual(r["bt"], "60.0")

    def test_duty_before_any_flame_uses_the_live_flow(self):
        # pwm.py:232 `_effective_on_temperature or device_state.flow_temperature`
        r = self.run_sc("duty", 1, 0, 60.0, 40.0, 0)
        self.assertEqual(r["duty"], "0.39")

    def test_duty_floor_is_base_plus_one_only_against_division_by_zero(self):
        r = self.run_sc("duty", 1, 1, 20.0, 40.0, 0)         # boiler colder than base: BT floored to 28.2
        self.assertEqual(r["duty"], "1.00")
        r = self.run_sc("duty", 1, 1, 20.0, 28.0, 0)         # pins the +1: (28-27.2)/(28.2-27.2) = 0.80 (a +5 floor gives 0.16)
        self.assertEqual(r["duty"], "0.80")

    def test_no_flow_temperature_idles_pwm_and_sends_the_continuous_setpoint(self):
        # pwm.py:122-128 (PWM IDLE) and heating_control.py:149-150 (control mode reads continuous)
        r = self.run_sc("duty", 1, 1, 0.0, 40.0, 1)
        self.assertEqual((r["duty"], r["bt"], r["req"], r["cs"]), ("0.00", "0.0", "0", "1040.0"))

    def test_first_ignition_of_the_on_phase_reseeds_bt_and_anchors_the_window(self):
        # pwm.py:186-191
        r = self.run_sc("sync", 1, 35.0)
        self.assertEqual((r["bt"], r["wait"], r["onago"]), ("35.0", "0", "0"))

    def test_ignition_timeout_does_not_abort_the_on_phase_and_keeps_bt(self):
        # pwm.py:194-199 (entry 80, Q35 A, TASK-1382): no flame after 180 s stops the wait ("starting timer anyway");
        # it is no abort. The scenario's ON time (351 s) is not over 181 s after the phase start, so ON continues.
        r = self.run_sc("timeout", 1)
        self.assertEqual((r["req"], r["wait"], r["anchor"], r["bt"]), ("1", "0", "0", "46.0"))

    def test_relight_inside_the_on_phase_keeps_bt_and_the_closed_window(self):
        # pwm.py:145-146 and 206: _flame_on_at survives a mid-phase flame-out, so no reseed and no new window
        # TASK-1382 (entry 80): a flame-out mid-phase no longer re-arms the ignition wait (rearmed 0, pwm.py:144-146)
        r = self.run_sc("relight", 1)
        self.assertEqual((r["bt"], r["rearmed"], r["wait"], r["onago"], r["req"]), ("46.0", "0", "0", "260000", "1"))

    def test_learned_bt_is_still_filtered_between_30_and_180_s_after_ignition(self):
        # seeded 50, flow 60, 100 s after flame-on: EMA 0.3*60+0.7*50 = 53.0 (the old 30 s window froze it at 50)
        r = self.run_sc("duty", 1, 1, 60.0, 40.0, 1, 100000)
        self.assertEqual(r["bt"], "53.0")

    def test_learned_bt_is_held_after_the_180_s_window(self):
        r = self.run_sc("duty", 1, 1, 60.0, 40.0, 1, 200000)
        self.assertEqual(r["bt"], "50.0")

    def test_on_start_with_the_flame_already_burning_reseeds_bt_at_once(self):
        # pwm.py:164-167: no ignition wait, BT = the live flow (33) instead of the value learned earlier (47.2)
        r = self.run_sc("pending", 1, 5000)
        self.assertEqual((r["wait"], r["bt"], r["onago"]), ("0", "33.0", "0"))   # window counts from the ON start (pwm.py:164)

    def test_pwm_reset_clears_the_learned_bt(self):
        r = self.run_sc("reset", 1)
        self.assertEqual(r["bt"], "0.0")

    # --- satApplyPWM override gate -------------------------------------------------
    def test_off_phase_with_protection_sends_minimum_not_28(self):
        self.assertEqual(float(self.run_sc("off", 1)["cs"]), 10.0)

    def test_protection_off_keeps_previous_cs_in_off_phase(self):
        self.assertEqual(float(self.run_sc("off", 0)["cs"]), 28.0)

    def test_start_tick_with_protection_holds_return_plus_offset(self):
        r = self.run_sc("start", 1)
        self.assertEqual(float(r["cs"]), 48.0)       # return 30 + 18
        self.assertEqual(r["req"], "1")

    def test_cold_start_hold_is_floored_at_the_cold_setpoint(self):
        # TASK-1316 (Sergeant D 2026-10-23): return 5 + offset 18 = 23 < 28.2, so the hold is 28.2
        # (underfloor: return 2 + 18 = 20 < 21, hold 21) and CH stays on during ignition
        self.assertEqual(float(self.run_sc("start", 1, 28.2, 5.0)["cs"]), 28.2)
        self.assertEqual(float(self.run_sc("start", 1, 21.0, 2.0)["cs"]), 21.0)

    def test_hold_above_the_cold_setpoint_is_unchanged(self):
        self.assertEqual(float(self.run_sc("start", 1, 28.2, 20.0)["cs"]), 38.0)   # 20 + 18

    def test_start_tick_protection_off_keeps_previous_cs(self):
        self.assertEqual(float(self.run_sc("start", 0)["cs"]), 28.0)

    def test_hold_after_ignition_keeps_return_plus_offset(self):
        self.assertEqual(float(self.run_sc("hold", 1)["cs"]), 48.0)

    def test_suppression_after_hold_is_flow_minus_offset(self):
        self.assertEqual(float(self.run_sc("suppress", 1)["cs"]), 32.0)   # flow 33 - 1

    def test_protection_off_has_no_suppression_override(self):
        self.assertEqual(float(self.run_sc("suppress", 0)["cs"]), 48.0)   # previous CS kept

    # --- 3 s refresh ----------------------------------------------------------------
    def test_refresh_sends_flow_minus_offset_after_hold(self):
        r = self.run_sc("refresh", 1)
        self.assertEqual((r["sent"], float(r["cs"]), float(r["final"])), ("1", 40.0, 40.0))   # flow 41 - 1

    def test_refresh_idle_cases(self):
        for extra, why in (("hold", "inside the hold"), ("dhw", "DHW"), ("cont", "continuous mode"),
                           ("cold", "requested setpoint below cold"), ("closed", "valves closed")):
            with self.subTest(why):
                self.assertEqual(self.run_sc("refresh", 1, extra)["sent"], "0")

    def test_refresh_idle_with_protection_off(self):
        self.assertEqual(self.run_sc("refresh", 0)["sent"], "0")

    # --- review round 1 ---------------------------------------------------------------
    def test_underheat_not_evaluated_in_off_phase(self):
        r = self.run_sc("uh", 1, 0)          # PWM OFF phase, flow 20 against a 35 request for 200 s
        self.assertEqual(r["mode"], "2")     # stays PWM

    def test_underheat_still_switches_in_on_phase(self):
        r = self.run_sc("uh", 1, 1)
        self.assertEqual(r["mode"], "1")     # continuous
        self.assertEqual(r["req"], "0")      # the mode switch cleared the PWM state

    def test_range5_tick_and_refresh_send_the_same_cs(self):
        r = self.run_sc("range5", 1)
        self.assertEqual((float(r["tick"]), r["sent"], float(r["refresh"])), (32.0, "1", 32.0))   # flow 33 - 1

    # --- TASK-1323: solar offset on the requested setpoint ------------------------------
    def test_solar_ignition_tick_sends_the_hold_not_hold_minus_solar(self):
        # PWM ON start tick (ignition): hold = return 30 + 18 = 48. Solar lowers the REQUESTED setpoint
        # (45 -> 43) before the PWM step, never the hold (thermo-nova heating_control.py:398 has no solar term).
        r = self.run_sc("tick", 1, 1, 45)
        self.assertEqual((float(r["cs"]), r["req"]), (48.0, "1"))

    def test_ignition_tick_without_solar_is_the_hold(self):
        self.assertEqual(float(self.run_sc("tick", 1, 0, 45)["cs"]), 48.0)

    def test_solar_offset_lowers_the_requested_setpoint_before_the_mode_step(self):
        # protection off: PWM keeps the previous CS (28); the continuous marker shows what the step received
        if not self.has_reqsp:
            self.skipTest("HEAD has no requested-setpoint segment")
        self.assertEqual(float(self.run_sc("tick", 0, 1, 35)["cs"]), 28.0)

    def test_refresh_after_hold_has_no_solar_term(self):
        r = self.run_sc("refresh", 1, "solar")
        self.assertEqual((r["sent"], float(r["cs"]), float(r["final"])), ("1", 40.0, 40.0))   # flow 41 - 1, no - 2

    def test_no_solar_ratchet_without_override(self):
        self.assertEqual(float(self.run_sc("solar", 0)["cs"]), 48.0)

    def test_flame_already_on_keeps_previous_cs_inside_delay(self):
        r = self.run_sc("pending", 1, 5000)
        self.assertEqual((float(r["cs"]), r["req"]), (28.0, "1"))      # not return + 18 = 48

    def test_flame_already_on_suppresses_after_delay(self):
        self.assertEqual(float(self.run_sc("pending", 1, 60000)["cs"]), 32.0)

    def test_pwm_reset_clears_stale_state(self):
        self.assertEqual(self.run_sc("reset", 1), {"req": "0", "wait": "0", "hold": "0.0", "onms": "0", "mode": "2", "bt": "0.0"})

    # --- TASK-1387: PWM IDLE (thermo-nova PWMStatus.IDLE), read by the cycle classifier (classifier.py:28-29) ---
    def test_pwm_idle_flag_follows_pwm_py(self):
        r = self.run_sc("idle", 1)
        self.assertNotEqual(r["reset"], "-1", "no _pwm_idle flag in this SATcontrol.ino")
        self.assertEqual(r["reset"], "1")         # reset(): IDLE (pwm.py:92)
        self.assertEqual(r["step"], "0")          # a step with a flow temperature leaves IDLE (pwm.py:154-180)
        self.assertEqual(r["limit"], "1")         # the hour-limit return keeps the status (pwm.py:156-158)
        self.assertEqual(r["limit_busy"], "0")
        self.assertEqual(r["noflow"], "1")        # flow missing: IDLE (pwm.py:122-128)

    def test_overshoot_switch_tick_is_not_idle(self):
        # thermo-nova runs pwm.update() in the same pass that enables PWM (heating_control.py:206-215): a cycle
        # that ends before the firmware's next control tick must not be classified as an IDLE PWM cycle.
        r = self.run_sc("idle", 1)
        self.assertNotEqual(r["reset"], "-1", "no _pwm_idle flag in this SATcontrol.ino")
        self.assertEqual(r["switchmode"], "2")
        self.assertEqual(r["switch"], "0")
        self.assertEqual(r["switch_noflow"], "1")

    # TASK-1420, Q47 A (entry 88), LOOP LEVEL: the real overshoot gate of satControlLoopRun() followed by the real
    # apply step in the same tick (tickRunSw cuts them out of SATcontrol.ino; the switch itself is the real
    # satCycleCheckOvershootSwitch()). On the switch tick the mode is PWM and the setpoint is the PWM value, not the
    # continuous one (the continuous stub returns 1000+p, which the final clamp cuts to the 62 max), and CH stays 1 (satChEnableDecide, real).
    # Not covered: the CS enqueue after the segment (satEnqueueIfChangedCS is not part of the cut).
    def test_overshoot_switch_tick_sends_the_pwm_setpoint_not_the_continuous_one(self):
        r = self.run_sc("swtick", 1)
        self.assertNotEqual(r["mode"], "-1", "no overshoot switch gate in this SATcontrol.ino")
        self.assertEqual(r["mode"], "2", r)
        self.assertEqual(r["ticks"], "61", r)                       # first qualifying tick starts the timer, 60 s later it fires
        self.assertEqual(r["continuous_ticks"], "60", r)            # every tick before it still sent the continuous setpoint
        self.assertEqual(r["cont_marker"], "0", "the switch tick sent the continuous setpoint: %r" % r)
        self.assertEqual(r["final"], r["cs"], r)
        self.assertEqual(r["cs"], "48.0", r)                         # PWM ON start: return 30 + flame-off offset 18, not the clamped continuous 62
        self.assertEqual(r["drives"], "1", r)
        self.assertEqual(r["ch"], "1", r)

    def test_mode_switch_resets_pwm_state(self):
        r = self.run_sc("reset", 1, "mode")
        self.assertEqual((r["req"], r["wait"], r["mode"]), ("0", "0", "1"))

    # --- TASK-1327: cold cutoff disables PWM and drops the held CS (Sergeant D 2026-10-23, entry 12) ---
    def test_cutoff_then_recovery_does_not_return_the_stale_held_cs(self):
        r = self.run_sc("cutoff", 0)
        self.assertEqual(float(r["held"]), 45.0)          # sanity: PWM without override keeps prevCS
        self.assertEqual(r["mode"], "1")                  # TASK-1332: no Force PWM option, the cutoff always leaves PWM

    def test_cutoff_resets_the_pwm_phase_and_the_held_cs(self):
        r = self.run_sc("cutoff", 0)
        self.assertEqual((r["req"], float(r["hold"]), float(r["last"])), ("0", 0.0, 10.0))

    def test_cutoff_leaves_pwm_mode(self):
        r = self.run_sc("cutoff", 0)
        self.assertEqual(r["mode"], "1")                  # thermo-nova control_mode is CONTINUOUS while PWM is not enabled
        self.assertEqual((r["req"], float(r["hold"]), float(r["last"])), ("0", 0.0, 10.0))
        self.assertEqual(float(r["recovery"]), 62.0)      # satApplyContinuous marker (1030) after the final clamp, not the stale 45

    def test_cutoff_clears_the_sustain_guards(self):
        # heating_control.py:231 -> _reset_pwm_disable_guards (:525-528): overshoot 45 s, underheat 12 s, saturation 7 s -> 0
        self.assertEqual(float(self.run_sc("cutoff", 0)["sustain"]), 0.0)

    def test_cutoff_sets_the_max_relative_modulation(self):
        # heating_control.py:234; the MM= of the cutoff tick is state.sat.iCurrentModulation (satEnqueueIfChangedMM)
        self.assertEqual(self.run_sc("cutoff", 0)["mm"], "80")

    def test_closed_valves_reset_pwm_like_the_cold_cutoff(self):
        # heating_control.py:69-70: valves closed -> heater OFF -> same else branch (:229-234)
        r = self.run_sc("cutoff", 0, "valves")
        self.assertEqual(float(r["held"]), 45.0)
        self.assertEqual((r["req"], float(r["hold"]), float(r["last"]), r["mm"], float(r["sustain"])), ("0", 0.0, 10.0, "80", 0.0))
        self.assertEqual(r["mode"], "1")
        self.assertEqual(float(r["recovery"]), 62.0)      # continuous marker, not the stale 45, after the valves reopen

    def test_refresh_idle_while_thermal_safe_fallback_active(self):
        self.assertEqual(self.run_sc("refresh", 1, "thermal")["sent"], "0")

    def test_refresh_updates_final_setpoint_without_ot_interface(self):
        r = self.run_sc("refresh", 1, "noot")
        self.assertEqual((r["sent"], float(r["final"])), ("0", 40.0))

    # --- MM selection ----------------------------------------------------------------
    def mm(self, pwm, hp, hyb, maxrel, quirks=0):
        return int(subprocess.run([str(self.exe), "mm", str(int(pwm)), str(int(hp)), str(int(hyb)), str(maxrel), str(quirks)],
                                  capture_output=True, text=True, check=True).stdout)

    def test_gas_pwm_is_zero_continuous_is_max(self):
        self.assertEqual(self.mm(True, False, False, 100), 0)
        self.assertEqual(self.mm(False, False, False, 80), 80)

    def test_geminox_never_zero(self):
        self.assertEqual(self.mm(True, False, False, 100, 0x01), 10)
        self.assertEqual(self.mm(False, False, False, 5, 0x01), 10)

    def test_heat_pump_and_hybrid_never_zero(self):
        self.assertEqual(self.mm(True, True, True, 70), 70)
        self.assertEqual(self.mm(True, False, True, 70), 70)
        self.assertEqual(self.mm(True, True, True, 0), 100)
        self.assertEqual(self.mm(False, True, True, 70), 100)   # heat pump outside PWM: always max

    def test_immergas_cap(self):
        self.assertEqual(self.mm(False, False, False, 100, 0x02), 80)


class PidIntegralFreeze(unittest.TestCase):
    """TASK-1323: the real _pidUpdateIntegral() from SATpid.ino (thermo-nova pid.py:167-168, :187-189)."""
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build_pid(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def integral(self, active, setting, error, start):
        out = subprocess.run([str(self.exe), str(int(active)), str(int(setting)), str(error), str(start)],
                             capture_output=True, text=True, check=True).stdout
        return float(out.split("=")[1])

    def test_frozen_keeps_the_integral_outside_the_deadband(self):
        self.assertEqual(self.integral(True, True, 1.0, 3.0), 3.0)      # HEAD: reset to 0

    def test_frozen_keeps_the_integral_inside_the_deadband(self):
        self.assertEqual(self.integral(True, True, 0.05, 3.0), 3.0)

    def test_freeze_setting_off_still_accumulates_during_solar(self):
        self.assertGreater(self.integral(True, False, 0.05, 3.0), 3.0)  # HEAD: frozen whatever the setting

    def test_freeze_setting_off_still_resets_outside_the_deadband(self):
        self.assertEqual(self.integral(True, False, 1.0, 3.0), 0.0)

    def test_no_solar_resets_outside_and_accumulates_inside(self):
        self.assertEqual(self.integral(False, True, 1.0, 3.0), 0.0)
        self.assertGreater(self.integral(False, True, 0.05, 3.0), 3.0)


class SolarSource(unittest.TestCase):
    def test_solar_offset_is_applied_to_the_requested_setpoint_before_the_clamp(self):
        src = SAT_INO.read_text(encoding="utf-8")
        loop = src[src.index("static void satControlLoopRun() {") if "static void satControlLoopRun() {" in src else src.index("static void satControlLoopRun()\n"):]
        solar = loop.index("pidOutput -= settings.sat.fSolarSetpointOffset;")
        self.assertLess(solar, loop.index("if (pidOutput < SAT_MIN_SETPOINT) pidOutput = SAT_MIN_SETPOINT;"))
        self.assertLess(solar, loop.index("finalSetpoint = satApplyPWM(pidOutput);"))
        self.assertNotIn("finalSetpoint -= settings.sat.fSolarSetpointOffset", loop)

    def test_refresh_has_no_solar_term_and_reads_the_requested_setpoint(self):
        src = SAT_INO.read_text(encoding="utf-8")
        refresh = src[src.index("static void satPwmSuppressionRefresh()"):src.index("void satPwmReset()")]
        self.assertNotIn("fSolarSetpointOffset", refresh)
        self.assertIn("_sat_requestedSetpoint <= satGetColdSetpoint()", refresh)

    def test_cold_cutoff_is_at_or_below_like_the_pwm_gate(self):
        src = SAT_INO.read_text(encoding="utf-8")
        self.assertIn("coldCutoff = satColdCutoff(pidOutput, satGetColdSetpoint())", src)
        self.assertIn("pidOutput <= satGetColdSetpoint()", src[src.index("static float satApplyPWM"):])
        self.assertNotIn("(pidOutput < satGetColdSetpoint())", src)

    def test_pid_freeze_follows_the_setting(self):
        pid = PID_INO.read_text(encoding="utf-8")
        self.assertIn("state.sat.bSolarGainActive && settings.sat.bSolarFreezeIntegral", pid)


class ColdCutoffSource(unittest.TestCase):
    def test_loop_calls_the_pwm_disable_inside_the_cold_cutoff_block(self):
        src = SAT_INO.read_text(encoding="utf-8")
        loop = src[src.index("void satControlLoop"):]
        block = loop[loop.index("if (coldCutoff) {"):]
        block = block[:block.index("\n  }\n")]
        self.assertIn("satPwmDisableOnHeaterOff();", block)
        self.assertNotIn("iCurrentModulation = 100", block)   # the helper sets the max relative modulation

    def test_loop_calls_the_pwm_disable_in_the_valves_closed_path(self):
        src = SAT_INO.read_text(encoding="utf-8")
        loop = src[src.index("void satControlLoop"):]
        block = loop[loop.index("if (!state.sat.bValvesOpen) {"):]
        block = block[:block.index("return;")]
        self.assertIn("satPwmDisableOnHeaterOff();", block)


class PwmAutoSwitchRemoved(unittest.TestCase):
    """TASK-1341: the PWM auto switch setting is gone from the firmware sources (Sergeant D 2026-10-24, entry 19)."""
    NAMES = re.compile(r"bPwmAutoSwitch|satpwmautoswitch|pwm_auto_switch", re.I)

    def test_no_pwm_auto_switch_name_left_in_firmware_sources(self):
        left = []
        for f in (REPO / "src" / "OTGW-firmware").rglob("*"):
            if f.suffix not in (".ino", ".cpp", ".h", ".js", ".html", ".css", ".ini") or not f.is_file():
                continue
            for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if self.NAMES.search(line) and "RETIRED-SAT-ENTITY" not in line:
                    left.append(f"{f.relative_to(REPO).as_posix()}:{n}")
        self.assertEqual(left, [])

    def test_retired_entity_table_clears_the_old_discovery_and_state_topic(self):
        src = (REPO / "src" / "OTGW-firmware" / "MQTTHaDiscovery.cpp").read_text(encoding="utf-8")
        self.assertIn('"switch", "sat_pwm_auto_switch_enable", "/sat/pwm_auto_switch_enable"', src)

    def test_old_settings_key_is_ignored_silently(self):
        src = (REPO / "src" / "OTGW-firmware" / "settingStuff.ino").read_text(encoding="utf-8")
        body = src[src.index("void updateSetting("):src.index("} // updateSetting()")]
        self.assertNotIn("SATpwmautoswitch", body)
        self.assertNotRegex(body, r"(?i)unknown (setting|key|field)")


class ForcePwmRemoved(unittest.TestCase):
    """TASK-1332: the Force PWM option is gone from the firmware sources (Sergeant D 2026-10-23, entry 13)."""
    NAMES = re.compile(r"bForcePWM|satforcepwm|force_pwm", re.I)

    def test_no_force_pwm_name_left_in_firmware_sources(self):
        left = []
        for f in (REPO / "src" / "OTGW-firmware").rglob("*"):
            if f.suffix not in (".ino", ".cpp", ".h", ".js", ".html", ".css", ".ini") or not f.is_file():
                continue
            for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                # the retired-entity table row is the one place that must keep the old topic names
                if self.NAMES.search(line) and "RETIRED-SAT-ENTITY" not in line:
                    left.append(f"{f.relative_to(REPO).as_posix()}:{n}")
        self.assertEqual(left, [])

    def test_retired_entity_table_clears_the_old_discovery_and_state_topic(self):
        src = (REPO / "src" / "OTGW-firmware" / "MQTTHaDiscovery.cpp").read_text(encoding="utf-8")
        rows = [l for l in src.splitlines() if "RETIRED-SAT-ENTITY" in l and "{" in l]
        self.assertEqual(len(rows), 2)
        self.assertIn('"switch", "sat_force_pwm_enable", "/sat/force_pwm_enable"', rows[0])
        self.assertIn("clearRetiredSatEntities(ctx);", (REPO / "src" / "OTGW-firmware" / "MQTTstuff.ino").read_text(encoding="utf-8"))

    def test_old_settings_key_is_ignored_silently(self):
        # updateSetting() has no catch-all: an unknown key (SATforcepwm in an old file) falls through the
        # else-if chain and leaves the struct unchanged (ADR-192: removed keys are ignored)
        src = (REPO / "src" / "OTGW-firmware" / "settingStuff.ino").read_text(encoding="utf-8")
        body = src[src.index("void updateSetting("):src.index("} // updateSetting()")]
        self.assertNotRegex(body, r"(?i)unknown (setting|key|field)")


class SourceLevel(unittest.TestCase):
    def test_no_generic_flame_off_offset_in_control_loop(self):
        src = SAT_INO.read_text(encoding="utf-8")
        loop = src[src.index("void satControlLoop"):]
        self.assertFalse("fFlameOffOffset" in loop, "satControlLoop still uses fFlameOffOffset")


class ChEnableSource(unittest.TestCase):
    def test_main_block_sends_ch_through_the_decide_helper(self):
        # TASK-1323: CH = 0 on the cold cutoff or the PWM OFF phase driving the CS, else 1
        src = SAT_INO.read_text(encoding="utf-8")
        loop = src[src.index("void satControlLoop"):]
        self.assertIn("satEnqueueIfChangedCH(satChEnableDecide(pidOutput, satGetColdSetpoint(), pwmDrivesSetpoint, pwmOn))", loop)
        # The PWM phase must be read before satCycleCheckPwmExits(): a switch to continuous
        # runs satPwmReset(), which clears bPwmFlameRequested and would send CH=0 mid-burn.
        self.assertLess(loop.index("pwmOn = state.sat.bPwmFlameRequested;"),
                        loop.index("satCycleCheckPwmExits(pidOutput, _sat_tickMs);"))
        self.assertNotIn("finalSetpoint > SAT_MIN_SETPOINT", loop)


if __name__ == "__main__":
    unittest.main(verbosity=2)
