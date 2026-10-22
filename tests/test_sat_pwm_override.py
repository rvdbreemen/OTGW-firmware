#!/usr/bin/env python3
"""TASK-1268: host test for the SAT PWM control-setpoint override and the MM selection.

The code under test is NOT re-implemented here. The test cuts the real PWM block
(from `_pwm_effectiveBoilerTemp` up to the satPwmLastOffTimeMs() accessor: schedule,
satApplyPWM(), the 3 s satPwmSuppressionRefresh(), satPwmReset(), satSetControlMode()) and
satComputeMM() out of src/OTGW-firmware/SATcontrol.ino, plus satCycleCheckAutoSwitch() and
its constants out of SATcycles.ino, wraps them in a stub header and compiles them with a
host C++ compiler (g++, clang++ or MSVC cl via vswhere). Without a compiler the tests
are skipped, they do not pass silently.

OTGW_SAT_INO and OTGW_SATCYCLES_INO point at other copies, e.g. the output of
`git show HEAD:src/OTGW-firmware/SATcontrol.ino`, for old-vs-fix runs. On HEAD there is
no overshoot_protection gate, no satComputeMM(), no 3 s refresh and no PWM reset, so those
cases fail.

Not reachable from here: the satControlLoop() block that used to add the flame-off
offset to every setpoint. It is covered by a source-level check plus the bench run.

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
CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", REPO / "src" / "OTGW-firmware" / "SATcycles.ino"))

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
#define SAT_QUIRK_MIN_MOD_10 0x01
#define SAT_QUIRK_IMMERGAS_TP 0x02
enum { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
typedef int SATControlMode;
struct SatSettings { bool bPwmAutoSwitch=true; float fOvershootMargin=2.0f; bool bEnabled=true, bOvershootProtection=true, bSolarGainEnable=false, bSummerSimmer=false;
                     float fModSupDelay=20, fModSupOffset=1, fFlameOffOffset=18, fSolarSetpointOffset=2; };
struct Settings { SatSettings sat; };
struct SatState { float fFinalSetpoint=0, fPwmDutyCycle=0, fPidOutput=0; bool bPwmFlameRequested=false, bDhwActive=false,
                  bActive=true, bSafetyTripped=false, bValvesOpen=true, bSummerActive=false, bSolarGainActive=false;
                  int eControlMode=SAT_MODE_PWM; };
struct State { SatState sat; };
struct OTSys { int SlaveStatus = 0; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static uint32_t g_ms = 1000000; static bool g_flame = false; static float g_flow = 33, g_ret = 30;
static uint32_t g_flameOnStart = 0, g_flameOffStart = 0; static bool g_due = true; static bool g_hourLimit = false;
static int g_csSent = 0; static float g_csLast = 0;
static uint32_t millis() { return g_ms; }
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
static float satApplyContinuous(float p) { return 1000.0f + p; }   // marker value: "the continuous setpoint was sent"
void satPwmReset();                                                 // defined later in the cut block (the .ino gets Arduino prototypes)
static float satGetReturnTemp() { return g_ret; }
static float satGetBaseOffset() { return 27.2f; }
static float satGetColdSetpoint() { return 28.2f; }
static float satGetMaxSetpoint() { return 62.0f; }
static float satEffectiveMaxSetpoint() { return 62.0f; }
static uint32_t satGetMinOnTimeSec() { return 180; }
static uint8_t satGetMaxCyclesPerHour() { return 4; }
static uint32_t satCycleGetFlameOnStartMs() { return g_flameOnStart; }
static uint32_t satCycleGetFlameOffStartMs() { return g_flameOffStart; }
static bool satCycleIsHourLimitReached() { return g_hourLimit; }
static bool g_hasOT = true;
static bool hasOTCommandInterface() { return g_hasOT; }
static bool satEnqueueIfChangedCS(float cs) { g_csSent++; g_csLast = cs; return true; }
#define DUE(t) (g_due)
#define timerSATSuppress 0
"""

MAIN = r"""
static void reset() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 33; g_ret = 30; g_due = true; g_hourLimit = false; g_csSent = 0; g_csLast = 0; g_hasOT = true;
  _pwm_waitingForFlame = false; _pwm_flameOffHoldSetpoint = 0; _pwm_flameOnMs = 0; _pwm_lastOffTimeMs = 1;
  _pwm_effectiveBoilerTemp = 0; _pwm_waitForFlameStartMs = 0; _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
}
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
  state.sat.fPidOutput = pid;
  setPrev(28.0f);                          // previous CS (what the old +18 path produced)
  if (!strcmp(sc, "uh")) {                 // uh protection pwmFlameRequested: 200 s of low flow, 1 s calls
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = atoi(argv[3]) != 0;
    state.sat.fFinalSetpoint = pid; g_flow = 20.0f; _sustain_lastCheckMs = g_ms;
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
    _pwm_waitingForFlame = true; _pwm_waitForFlameStartMs = g_ms - 5000UL; _pwm_flameOnMs = 0;
    _pwm_effectiveBoilerTemp = 50.0f; g_flame = true; g_flow = (float)atof(argv[3]); g_flameOnStart = g_ms;
    satApplyPWM(pid);
    printf("bt=%.1f wait=%d onago=%u\n", _pwm_effectiveBoilerTemp, (int)_pwm_waitingForFlame, (unsigned)(g_ms - _pwm_flameOnMs));
    return 0;
  }
  if (!strcmp(sc, "timeout")) {            // TASK-1273: flame-out mid-phase, no relight, ignition timeout aborts the ON phase
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    _pwm_waitingForFlame = true; _pwm_waitForFlameStartMs = g_ms - 181000UL; _pwm_flameOnMs = g_ms - 400000UL;
    _pwm_effectiveBoilerTemp = 46.0f; g_flame = false; g_flameOnStart = g_ms - 10000UL;
    satApplyPWM(pid);
    printf("req=%d wait=%d anchor=%u bt=%.1f\n", (int)state.sat.bPwmFlameRequested, (int)_pwm_waitingForFlame,
           (unsigned)_pwm_flameOnMs, _pwm_effectiveBoilerTemp);
    return 0;
  }
  if (!strcmp(sc, "relight")) {            // TASK-1273: flame-out and relight inside one ON phase, window already over
    state.sat.eControlMode = SAT_MODE_PWM; state.sat.bPwmFlameRequested = true;
    _pwm_waitingForFlame = false; _pwm_flameOnMs = g_ms - 200000UL; g_flameOnStart = g_ms - 10000UL;
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
    _pwm_flameOffHoldSetpoint = 48; _pwm_flameOnMs = 123; _pwm_waitForFlameStartMs = 456; _pwm_effectiveBoilerTemp = 55;
    if (argc > 3 && !strcmp(argv[3], "mode")) satSetControlMode(SAT_MODE_CONTINUOUS); else satPwmReset();
    printf("req=%d wait=%d hold=%.1f onms=%u mode=%d bt=%.1f\n", (int)state.sat.bPwmFlameRequested, (int)_pwm_waitingForFlame,
           _pwm_flameOffHoldSetpoint, (unsigned)_pwm_flameOnMs, state.sat.eControlMode, _pwm_effectiveBoilerTemp);
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
    setPrev(48.0f); state.sat.fPidOutput = hi; g_flame = true; g_flameOnStart = g_ms - 60000UL; _pwm_flameOnMs = g_ms - 60000UL;
    float tick = satApplyPWM(hi);
    state.sat.fFinalSetpoint = tick;
    satPwmSuppressionRefresh();
    printf("tick=%.1f sent=%d refresh=%.1f\n", tick, g_csSent, g_csLast);
    return 0;
  }
  if (!strcmp(sc, "pending")) {            // flame already on when the ON phase starts; argv[3] = ms since flame on
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 600000UL;
    g_flame = true; g_flameOnStart = g_ms - (uint32_t)atoi(argv[3]); _pwm_flameOnMs = g_flameOnStart;
    float cs = satApplyPWM(pid);
    printf("cs=%.1f req=%d wait=%d bt=%.1f onago=%u\n", cs, (int)state.sat.bPwmFlameRequested, (int)_pwm_waitingForFlame,
           _pwm_effectiveBoilerTemp, (unsigned)(g_ms - _pwm_flameOnMs));
    return 0;
  }
  if (!strcmp(sc, "off")) {
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 100000UL;
  } else if (!strcmp(sc, "start")) {
    state.sat.bPwmFlameRequested = false; g_flameOffStart = g_ms - 600000UL;
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
      if (!strcmp(argv[3], "cold")) state.sat.fPidOutput = 20.0f;
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


def extract(src: str) -> str:
    a = src.index("static float    _pwm_effectiveBoilerTemp")
    b = src.index("// Last PWM off-phase duration in ms")
    block = src[a:b]
    m = re.search(r"^static uint8_t satComputeMM\(.*?^\}\s*$", src, re.S | re.M)
    mm = m.group(0) if m else ""
    return block + "\n" + mm


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
    src = CYCLES_INO.read_text(encoding="utf-8")
    consts = re.findall(r"^static (?:const )?\w+\s+(?:SAT_(?:OVERSHOOT|UNDERSHOOT|UNDERHEAT|SATURATION|DHW)\w*|_sustain_\w+)\s*=[^;]*;",
                        src, re.M)
    m = re.search(r"^bool satCycleCheckAutoSwitch\([^)]*\)\s*\{.*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError("satCycleCheckAutoSwitch() not found in SATcycles.ino")
    fn = m.group(0)
    call = "satCycleCheckAutoSwitch(sp)" if re.search(r"satCycleCheckAutoSwitch\(float", fn) else \
        "(state.sat.fFinalSetpoint = (sp), satCycleCheckAutoSwitch())"
    return "\n".join(consts) + "\n" + fn + f"\n#define CALL_AUTOSWITCH(sp) {call}\n"


def build(workdir: Path):
    body = extract(SAT_INO.read_text(encoding="utf-8"))
    for name, stub in (
        ("satComputeMM", "static uint8_t satComputeMM(bool,bool,bool,uint8_t,uint8_t){ return 255; }"),
        ("satPwmSuppressionRefresh", "static void satPwmSuppressionRefresh(){ }"),
        ("satPwmReset", "static void satPwmReset(){ }"),
        ("satSetControlMode", "static void satSetControlMode(int m){ state.sat.eControlMode = m; }"),
        ("_sat_lastControlCS", "static float _sat_lastControlCS = 0;"),
        ("_sat_thermalSafeActive", "static bool _sat_thermalSafeActive = false;"),
    ):
        if name not in body:   # HEAD lacks these: keep the harness compiling so the cases fail, not the build
            body += "\n" + stub + "\n"
    cycles = "static uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }\n" + extract_cycles()
    cpp = workdir / "t.cpp"
    cpp.write_text(STUBS + body + cycles + MAIN, encoding="utf-8")
    exe = workdir / "t.exe"
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = _vcvars()
    if bat:
        cmd = f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc /Fe:"{exe}" /Fo:"{workdir}\\\\" "{cpp}"'
        r = subprocess.run(cmd, shell=True, cwd=workdir, capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("host compile failed:\n" + r.stdout[-3000:] + r.stderr[-1000:])
        return exe
    return None


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

    def run_sc(self, *args):
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())


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

    def test_ignition_timeout_abort_clears_the_anchor_and_keeps_bt(self):
        r = self.run_sc("timeout", 1)
        self.assertEqual((r["req"], r["wait"], r["anchor"], r["bt"]), ("0", "0", "0", "46.0"))

    def test_relight_inside_the_on_phase_keeps_bt_and_the_closed_window(self):
        # pwm.py:145-146 and 206: _flame_on_at survives a mid-phase flame-out, so no reseed and no new window
        r = self.run_sc("relight", 1)
        self.assertEqual((r["bt"], r["rearmed"], r["wait"], r["onago"], r["req"]), ("46.0", "1", "0", "260000", "1"))

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

    def test_no_solar_ratchet_without_override(self):
        self.assertEqual(float(self.run_sc("solar", 0)["cs"]), 48.0)

    def test_flame_already_on_keeps_previous_cs_inside_delay(self):
        r = self.run_sc("pending", 1, 5000)
        self.assertEqual((float(r["cs"]), r["req"]), (28.0, "1"))      # not return + 18 = 48

    def test_flame_already_on_suppresses_after_delay(self):
        self.assertEqual(float(self.run_sc("pending", 1, 60000)["cs"]), 32.0)

    def test_pwm_reset_clears_stale_state(self):
        self.assertEqual(self.run_sc("reset", 1), {"req": "0", "wait": "0", "hold": "0.0", "onms": "0", "mode": "2", "bt": "0.0"})

    def test_mode_switch_resets_pwm_state(self):
        r = self.run_sc("reset", 1, "mode")
        self.assertEqual((r["req"], r["wait"], r["mode"]), ("0", "0", "1"))

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


class SourceLevel(unittest.TestCase):
    def test_no_generic_flame_off_offset_in_control_loop(self):
        src = SAT_INO.read_text(encoding="utf-8")
        loop = src[src.index("void satControlLoop"):]
        self.assertFalse("fFlameOffOffset" in loop, "satControlLoop still uses fFlameOffOffset")


if __name__ == "__main__":
    unittest.main(verbosity=2)
