#!/usr/bin/env python3
"""TASK-1371: PWM ignition of a boiler WITHOUT a return temperature (Sergeant D entries 45, 135, 141, 142, 149, 151).

The real code is cut out of src/OTGW-firmware/SATcontrol.ino and compiled with a host C++ compiler, nothing is re-implemented:
  - the command cache block (satSendCS/MM/CH, satCmdResend, satEnqueueIfChangedCS ...), as tests/test_sat_cmd_cache.py,
  - the PWM block (satPwmFlameOffHold, satApplyPWM, satPwmSuppressionRefresh ...), as tests/test_sat_pwm_override.py,
  - satNoReturnTemp() and the flood block (satNoReturnFloodOnFlameEdge/Step, satMiStep).
The harness drives them in the order of satControlLoopRun(): flame edge, satMiStep, flood step, 3 s suppression refresh, 5 s resend,
and the 30 s control tick (satApplyPWM + satSendCS/MM/CH). The device is simulated by stubs (flame, flow, queue, PM chain).

OTGW_SAT_INO points at another copy (e.g. `git show HEAD:...`) for the old-vs-fix run: where the source has no flood block the three
flood functions are stubbed empty, so the old code shows its real behaviour (return + 18, no flood, MI only at boot).
Without a compiler the tests are skipped.

Run: python tests/test_sat_no_return_ignition.py
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

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#define F(x) x
#define PSTR(x) x
#define snprintf_P snprintf
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
// TASK-1521 log helpers
static void satNarrate_P(const char*) {}
static bool _sat_followNarrPending = false;
static bool _sat_narrAtMax = false;
static bool _sat_narrCold = false;
static bool _sat_bootBoilerLogged = false;
static bool _sat_bootOutsideLogged = false;
static void satNarratef_P(const char *fmt, ...) { (void)fmt; }
#define SAT_MIN_SETPOINT 10.0f
#define SAT_QUIRK_MIN_MOD_10 0x01
#define SAT_QUIRK_IMMERGAS_TP 0x02
#define SAT_QUIRK_MI_500_BOOT 0x08
enum { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
typedef int SATControlMode;
struct SatSettings { float fOvershootMargin=2.0f; bool bEnabled=true, bOvershootProtection=true, bSolarGainEnable=false, bSummerSimmer=false, bSimulation=false;
                     int iMaxRelModulation=80; float fModSupDelay=20, fModSupOffset=1, fFlameOffOffset=18, fSolarSetpointOffset=2;
                     int iHeatingMode=0; };
struct Settings { SatSettings sat; };
struct SatState { int iCurrentModulation=0; float fFinalSetpoint=0, fPwmDutyCycle=0, fPidOutput=0; bool bPwmFlameRequested=false, bDhwActive=false,
                  bActive=true, bSafetyTripped=false, bValvesOpen=true, bSummerActive=false, bSolarGainActive=false;
                  int eControlMode=SAT_MODE_PWM;
                  bool bLastSentValid=false; float fLastSentCS=0; uint8_t iLastSentMM=0, iLastSentCH=0; float fLastSentTC=0; bool bTCAccountedValid=false;
                  uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0, iTCPushMs=0; bool bPushRefused=false; };
struct State { SatState sat; };
struct OTSys { int SlaveStatus = 0; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static bool satIsEnabled() { return settings.sat.bEnabled; }
static uint32_t g_ms = 1000000; static bool g_flame = false; static float g_flow = 40, g_ret = 0;
static uint32_t g_flameOnStart = 0, g_flameOffStart = 0; static bool g_due = false; static bool g_hourLimit = false;
static float g_cold = 28.2f, g_effMax = 62.0f, g_userMax = 62.0f;
static uint32_t millis() { return g_ms; }
static bool satIsFlameOn() { return g_flame; }
static float satGetFlowTemp() { return g_flow; }
static float satGetReturnTemp() { return g_ret; }
static float satApplyContinuous(float p, float) { return 1000.0f + p; }
static float satGetBaseOffset() { return 27.2f; }
static float satGetColdSetpoint() { return g_cold; }
static float satGetMaxSetpoint() { return g_userMax; }
static float satEffectiveMaxSetpoint() { return g_effMax; }
static uint32_t satGetMinOnTimeSec() { return 180; }
static uint8_t satGetMaxCyclesPerHour() { return 4; }
static uint32_t satGetPwmPeriodSec() { return 3600 / satGetMaxCyclesPerHour(); }
static uint32_t satCycleGetFlameOnStartMs() { return g_flameOnStart; }
static uint32_t satCycleGetFlameOffStartMs() { return g_flameOffStart; }
static bool satCycleIsHourLimitReached() { return g_hourLimit; }
static void satCycleRecordPwmStart() {}
void satCycleResetSustainGuards() {}
void satCycleSkipAutoSwitch() {}
void satPwmReset();
static bool g_pic = true, g_hasOT = true, g_noReturn = true, g_boardTret = false, g_confirmed = true, g_pmBusy = false; static uint8_t g_quirks = 0;
static bool isPICEnabled() { return g_pic; }
static bool hasOTCommandInterface() { return g_hasOT; }
static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 28 && g_noReturn; }
static bool boilerMsgId28UnknownConfirmed() { return g_confirmed; }   // a boiler frame of this boot said Unknown-DataID
static bool picBoardTretAbsent() { return !g_boardTret; }
static bool picBoardTretValid() { return g_boardTret; }                // only the version before the review fixes calls it            // settled: no board sensor reading
static bool pmChainBusy() { return g_pmBusy; }
static uint8_t satGetManufacturerQuirks() { return g_quirks; }
#define DUE(t) (g_due)
#define timerSATSuppress 0
static void addCommandToQueue(const char* c, size_t n, bool, int) { printf("%lu %.*s\n", (unsigned long)(g_ms - 1000000UL), (int)n, c); }
"""

MAIN = r"""
static bool g_prevFlame = false;
static void loopPass() {   // order of satControlLoopRun()
  const bool f = satIsFlameOn();
  if (f != g_prevFlame) {
    const bool dhw = (OTcurrentSystemState.SlaveStatus & 0x04) != 0;
    satNoReturnFloodOnFlameEdge(f, dhw);
    g_prevFlame = f;
  }
  satMiStep();
  satNoReturnFloodStep();
  g_due = ((g_ms - 1000000UL) % 3000UL) == 0;
  satPwmSuppressionRefresh();
  satCmdResend();
}
static void tick(float pid) {   // the PWM part of the 30 s control tick
  state.sat.fPidOutput = pid;
  _sat_requestedSetpoint = pid;
  float cs = satApplyPWM(pid);
  if (cs > g_effMax) cs = g_effMax;
  state.sat.fFinalSetpoint = cs; _sat_lastControlCS = cs;
  satSendCS(cs); satSendMM(80); satSendCH(1);
  printf("# tick %lu cs=%.1f\n", (unsigned long)(g_ms - 1000000UL), cs);
}
static void run(uint32_t untilMs) { while (g_ms - 1000000UL < untilMs) { g_ms += 100; loopPass(); } }
static void init() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_ms = 1000000; g_flame = false; g_flow = 40; g_ret = 0; g_prevFlame = false; g_hasOT = true; g_pic = true; g_noReturn = true;
  g_boardTret = false; g_confirmed = true; g_pmBusy = false; g_quirks = 0; g_cold = 28.2f; g_effMax = 62.0f; g_userMax = 62.0f; g_flameOnStart = g_ms;
  state.sat.bPwmFlameRequested = true;     // ON phase, flame not lit yet
  state.sat.fFinalSetpoint = 50.0f;        // heating was commanded before
  _sat_lastControlCS = 50.0f; _sat_requestedSetpoint = 55.0f;
  satResetCmdCache();
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  init();
  if (argc > 2 && atoi(argv[2]) == 0) g_noReturn = false, g_ret = 30;           // arg2 = 0: a normal boiler with a return temperature
  if (!strcmp(sc, "ignite") || !strcmp(sc, "ignite_tick") || !strcmp(sc, "ignite_normal")) {
    const bool withTick = !strcmp(sc, "ignite_tick");
    tick(55.0f);                          // before the flame: the hold
    run(4000);
    g_flame = true; g_flameOnStart = g_ms + 100;
    for (int s = 0; s < 40; s++) { g_flow = 40.0f + s * 0.5f; run(4000 + (s + 1) * 1000UL); if (withTick && s == 9) tick(55.0f); }   // flow rises 0.5/s; a tick inside the flood
    return 0;
  }
  if (!strcmp(sc, "hold_only")) { tick(55.0f); return 0; }
  if (!strcmp(sc, "hold_unconfirmed")) { g_confirmed = false; g_ret = 30; tick(55.0f); return 0; }
  if (!strcmp(sc, "clamp_user")) { g_userMax = g_effMax = 50.0f; tick(55.0f); return 0; }
  if (!strcmp(sc, "cold_floor")) { g_flow = 5.0f; tick(55.0f); return 0; }
  if (!strcmp(sc, "pm_busy")) {
    g_pmBusy = true; tick(55.0f); run(4000);
    g_flame = true; g_flameOnStart = g_ms + 100; run(30000);
    return 0;
  }
  if (!strcmp(sc, "relight")) {           // CH flame, flood, hot-water draw, DHW flame, CH relight: second flood
    tick(55.0f); run(1000);
    g_flame = true; g_flameOnStart = g_ms + 100; run(26000);
    printf("# draw\n"); OTcurrentSystemState.SlaveStatus = 0x04; g_flame = false; run(30000);
    g_flame = true; run(33000);           // the hot-water flame: no flood
    printf("# draw ends\n"); g_flame = false; run(40000);
    state.sat.bDhwActive = true;          // the control tick has not noticed yet (stale), the live bit is clear
    OTcurrentSystemState.SlaveStatus = 0x00;
    printf("# relight\n"); g_flame = true; g_flameOnStart = g_ms + 100; run(120000);
    return 0;
  }
  if (!strcmp(sc, "mi")) {                // argv[2] ignored; env-like args: pic noReturn quirk
    g_pic = atoi(argv[3]) != 0; g_noReturn = atoi(argv[4]) != 0; g_quirks = atoi(argv[5]) ? SAT_QUIRK_MI_500_BOOT : 0; g_ret = g_noReturn ? 0 : 30;
    if (argc > 6) g_boardTret = atoi(argv[6]) != 0;
    if (argc > 7) g_confirmed = atoi(argv[7]) != 0;
    run(400000);
    return 0;
  }
  if (!strcmp(sc, "mi_flap")) {           // the no-return fact breaks at 200 s for 10 s: the 5 min start over
    run(200000); g_boardTret = true; run(210000); g_boardTret = false; run(520000);
    return 0;
  }
  return 2;
}
"""


def _cut(src, start, end):
    a = src.index(start)
    b = src.index(end, a)
    return src[a:b]


def build_source(src: str) -> str:
    cmd = _cut(src, "static const uint32_t SAT_CMD_REFRESH_MS", "// --- Thermal Drop Learning State")
    pwm = _cut(src, "static float    _pwm_effectiveBoilerTemp", "// Last PWM off-phase duration in ms")
    if "static void satNoReturnFloodStep" in src:
        m = re.search(r"static bool satNoReturnTemp\(\)\s*\{.*?^\}", src, re.S | re.M)
        flood = m.group(0) + "\n" + _cut(src, "static bool     _flood_active", "//=====") if False else None
        a = src.index("static bool satNoReturnTemp()")
        nr = src[a:src.index("\n}\n", a) + 3]
        f0 = src.index("static bool     _flood_active")
        mi = src.index("static void satMiStep()")
        f1 = src.index("\n}\n", mi) + 3
        flood = nr + "\n" + src[f0:f1]
        # the real block needs satNoReturnTemp() before satPwmFlameOffHold(): declare it first
        pre = "static bool satNoReturnTemp();\n"
    else:   # old code: the flood does not exist; the harness hooks do nothing
        pre = ("static void satNoReturnFloodOnFlameEdge(bool, bool) {}\nstatic void satNoReturnFloodStep() {}\n"
               "static void satMiStep() {}\n")
        flood = ""
    return STUBS + pre + cmd + "\n" + pwm + "\n" + flood + "\n" + MAIN


class NoReturnIgnition(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_source(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), *map(str, args)], capture_output=True, text=True, check=True).stdout.splitlines()
        cmds = []
        for l in out:
            if l.startswith("#"):
                continue
            t, c = l.split(" ", 1)
            cmds.append((int(t), c))
        return cmds, out

    @staticmethod
    def css(cmds, a, b):
        return [(t, c) for t, c in cmds if a <= t < b and c.startswith("CS=")]

    # --- (a) hold value before the flame -------------------------------------------------------------
    def test_hold_is_flow_plus_offset_without_return_temperature(self):
        cmds, _ = self.run_sc("hold_only", 1)
        self.assertEqual([c for _, c in cmds if c.startswith("CS=")], ["CS=58.0"])     # flow 40 + 18, not return(0) + 18 floored at 28.2

    def test_hold_is_clamped_to_the_user_max(self):
        cmds, _ = self.run_sc("clamp_user", 1)
        self.assertEqual([c for _, c in cmds if c.startswith("CS=")], ["CS=50.0"])     # 58 -> user MaxTSet 50

    def test_hold_is_floored_at_the_cold_setpoint(self):
        cmds, _ = self.run_sc("cold_floor", 1)
        self.assertEqual([c for _, c in cmds if c.startswith("CS=")], ["CS=28.2"])     # 5 + 18 = 23 -> cold 28.2

    # --- (b) flood ---------------------------------------------------------------------------------------
    def test_flood_alternates_cs_and_pm25_each_second_for_20_s_then_suppression(self):
        cmds, _ = self.run_sc("ignite", 1)
        flood = [(t, c) for t, c in cmds if 4100 <= t < 24100 and (c.startswith("CS=") or c == "PM=25")]
        self.assertEqual([t for t, _ in flood], list(range(4100, 24100, 1000)))           # one command per second, 20 commands
        for i, (t, c) in enumerate(flood):
            self.assertEqual(c.startswith("CS="), i % 2 == 0, (t, c))                  # CS at t=0,2,4 ..., PM=25 at t=1,3,5 ...
            self.assertEqual(c == "PM=25", i % 2 == 1, (t, c))
        # the CS value is the live flow + 18 at its second (flow rises 0.5 per second from the flame-on; the effective max 62 clamps it)
        for t_ms, c in flood:
            if c.startswith("CS="):
                self.assertEqual(c, "CS=%.1f" % min(62.0, 58.0 + 0.5 * ((t_ms - 4100) // 1000)), (t_ms, c))
        self.assertEqual(flood[0][1], "CS=58.0")
        # after 20 s: suppression = flow - 1, every CS from there is below the flow
        after = [(t_ms, c) for t_ms, c in cmds if t_ms >= 24100 and c.startswith("CS=")]
        self.assertTrue(after)
        for t_ms, c in after:   # the live flow of that moment minus fModSupOffset (1); the harness flow is 40 + 0.5 per second
            self.assertEqual(c, "CS=%.1f" % (40.0 + 0.5 * ((t_ms - 4001) // 1000) - 1.0), (t_ms, c))
        self.assertFalse([1 for t, c in cmds if t >= 24100 and c == "PM=25"])

    def test_no_stale_cs_interleaves_during_the_flood(self):
        cmds, _ = self.run_sc("ignite", 1)
        in_flood = self.css(cmds, 4100, 24100)
        # CS only at the even seconds of the flood (4100, 6100 ...): no resend, no tick value in between, and every value is flow + 18
        self.assertEqual([t for t, _ in in_flood], list(range(4100, 24100, 2000)))
        for t, c in in_flood:
            self.assertGreaterEqual(float(c[3:]), 58.0, (t, c))      # never the pre-flame tick value (or an older, lower one)

    def test_tick_inside_the_flood_sends_the_live_hold(self):
        cmds, out = self.run_sc("ignite_tick", 1)
        tick = [l for l in out if l.startswith("# tick")][1]           # the second tick: s == 9, 13 s into the run
        cs = float(tick.split("cs=")[1])
        t = int(tick.split()[2])
        around = [float(c[3:]) for tt, c in cmds if c.startswith("CS=") and tt <= t][-2:]
        self.assertEqual(cs, around[-1] if len(around) == 1 else cs)
        self.assertGreaterEqual(cs, 58.0)                              # flow + 18, not the stored pre-ignition hold

    def test_relight_after_hot_water_repeats_the_flood_and_a_dhw_flame_does_not(self):
        cmds, out = self.run_sc("relight", 1)
        pm = [t for t, c in cmds if c == "PM=25"]
        first = [t for t in pm if t < 27000]
        self.assertEqual(len(first), 10)                               # first flood: 10 PM=25 in 20 s
        dhw_flame = [t for t in pm if 27000 <= t < 40000]
        self.assertEqual(dhw_flame, [])                                # a DHW flame floods nothing
        again = [t for t in pm if t >= 40000]
        self.assertEqual(len(again), 10, pm)                           # CH relight: the flood again, even with a stale bDhwActive
        cs_again = self.css(cmds, 40000, 60000)
        self.assertEqual(len(cs_again), 10)

    def test_normal_boiler_is_unchanged(self):
        cmds, _ = self.run_sc("ignite_normal", 0)
        self.assertFalse([1 for _, c in cmds if c == "PM=25"])
        first = [c for _, c in cmds if c.startswith("CS=")][0]
        self.assertEqual(first, "CS=48.0")                             # return 30 + 18, as thermo-nova
        hold = self.css(cmds, 4100, 24100)
        self.assertEqual({c for _, c in hold}, {"CS=48.0"})            # only the 5 s resend of the hold, no flood values, no 2 s cadence
        self.assertLessEqual(len(hold), 4)

    def test_pm25_is_skipped_while_another_pm_is_pending(self):
        cmds, _ = self.run_sc("pm_busy", 1)
        self.assertFalse([1 for _, c in cmds if c == "PM=25"])
        self.assertEqual(len(self.css(cmds, 4100, 24100)), 10)         # the CS half of the flood still runs

    def test_relight_after_hot_water_hands_over_to_the_suppression(self):
        # Reviewer probe: after the flood the 5 s resend repeated CS=58.0 (flow + 18) for ever, because the flood re-armed the resend
        # and the 3 s refresh stayed out on the stale bDhwActive. Flow is 40: suppression = 39.0.
        cmds, _ = self.run_sc("relight", 1)
        end = [(t, c) for t, c in self.css(cmds, 60000, 120001)]
        self.assertTrue(end)
        self.assertEqual({c for _, c in end}, {"CS=39.0"}, end)
        self.assertLessEqual(end[0][0], 60200)                           # at once at the end of the flood, not at the next 3 s refresh

    # --- (e) MI=500 -----------------------------------------------------------------------------------------------
    def mi(self, pic, no_return, quirk, board=0, confirmed=1, times=False):
        cmds, _ = self.run_sc("mi", 1, pic, no_return, quirk, board, confirmed)
        return [(t, c) if times else c for t, c in cmds if c.startswith("MI=")]

    def test_mi500_once_on_pic_after_no_return_detection(self):
        self.assertEqual(self.mi(1, 1, 0), ["MI=500"])

    def test_no_return_mi500_waits_5_minutes_the_quirk_does_not(self):
        t = self.mi(1, 1, 0, times=True)
        self.assertEqual(len(t), 1)
        self.assertGreaterEqual(t[0][0], 300000)                         # 5 min continuously no-return
        self.assertLessEqual(t[0][0], 300300)
        q = self.mi(1, 0, 1, times=True)
        self.assertEqual(len(q), 1)
        self.assertLessEqual(q[0][0], 200)                               # the manufacturer quirk keeps its immediate send

    def test_a_break_in_the_no_return_fact_restarts_the_5_minutes(self):
        cmds, _ = self.run_sc("mi_flap", 1)
        mi = [t for t, c in cmds if c.startswith("MI=")]
        self.assertEqual(len(mi), 1)
        self.assertGreaterEqual(mi[0], 510000)                           # 210 s + 300 s, not the 300 s of the first stretch

    def test_a_restored_unknown_dataid_verdict_alone_is_no_return_nothing(self):
        # boot window: the verdict is restored from file, no boiler frame of this boot confirmed it yet, the board sensor is unknown
        self.assertEqual(self.mi(1, 1, 0, 0, 0), [])
        cmds, _ = self.run_sc("hold_unconfirmed", 1)
        self.assertEqual([c for _, c in cmds if c.startswith("CS=")], ["CS=48.0"])   # return 30 + 18, not flow + 18

    def test_mi500_for_the_manufacturer_quirk_on_pic_only(self):
        self.assertEqual(self.mi(1, 0, 1), ["MI=500"])
        self.assertEqual(self.mi(0, 0, 1), [])

    def test_mi500_never_on_ot_direct(self):
        self.assertEqual(self.mi(0, 1, 0), [])
        self.assertEqual(self.mi(0, 1, 1), [])

    def test_no_mi500_for_a_boiler_with_a_return_temperature(self):
        self.assertEqual(self.mi(1, 0, 0), [])

    def test_valid_board_sensor_counts_as_a_return_temperature(self):
        self.assertEqual(self.mi(1, 1, 0, 1), [])                      # Unknown-DataID on 28 but a valid 'R' reading: not "no return"

    def test_initSAT_no_longer_sends_mi500_on_every_board(self):
        src = SAT_INO.read_text(encoding="utf-8")
        a = src.index("void initSAT()") if "void initSAT()" in src else src.index("initSAT(")
        body = src[a:src.index("Simulation Mode (Task #37)", a)]
        self.assertNotIn('"MI=500"', body)                              # the unconditional boot send (also on OT-Direct) is gone


if __name__ == "__main__":
    unittest.main()
