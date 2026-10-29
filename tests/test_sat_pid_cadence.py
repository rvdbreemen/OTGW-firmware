#!/usr/bin/env python3
"""TASK-1368: the SAT PID steps on its own 60 s timer, independent of the control tick (Sergeant D entries 42, 28, 111).

thermo-nova (pinned 7beaaf5, custom_components/sat): the PID runs on a 60 s timer (climate.py:712-716, pid.py:20) plus one
start-up call (climate.py:137, :143); pid.update() has no time or unchanged-error gate (pid.py:159-171); the output is
computed on every read from the current curve: curve + kp(curve)*held error + I + kd(curve)*held raw derivative
(pid.py:60-132, read every loop at climate.py:375). The area PIDs have their own 60 s timer (area.py:183-190).

Nothing is re-implemented. The test compiles the REAL code with a host C++ compiler:
  - the whole SATpid.ino (satPidReset/Step/Output/Service/CurveOnly, the integral and derivative helpers) and the real
    safeTimers.h behind DECLARE_TIMER_SEC / DUE;
  - from SATcontrol.ino the zone PID (satZoneUsable, satZoneKp, satZonePidAdvance, satZonePidAdvanceAll, satZonePidOutput,
    satZonePidExclude), satCalcHeatingCurve and satOutsideTempOrDefault; satRoomFiltered for the EMA property.
Stubs: the clock (g_ms), the sensors (satPidGatherInputs returns a scripted room temperature and curve, and counts its calls:
it is called exactly once per PID step) and settings/state.
A loop pass is simulated every second; the control tick runs every `tick_s` seconds and does what satControlLoopRun() does
for the PID: satPidNoteRoom() + satPidOutput() (HEAD: satPidUpdate()).

Not reachable from here: satControlLoopRun() itself (it cannot be compiled on the host). The order of the PID service against the
tick's early returns is checked at source level (SourceOrder, labelled structural).

OTGW_SATPID_INO / OTGW_SAT_INO point at other copies (`git show HEAD:src/OTGW-firmware/SATpid.ino`) for the old-vs-fix run. On
HEAD the harness drives satPidUpdate()/satZonePidStep() on every tick, as the old tick did, and the cadence cases fail on behaviour.

Run: python tests/test_sat_pid_cadence.py
"""
import math
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
# Automatic gains are the only mode (TASK-1357): kd = 0.07 * 8400 * kp, kp = coefficient * curve / 3 (radiators).
KD_AT_CURVE_40 = 0.07 * 8400.0 * (1.6 * 40.0 / 3.0)
PID_INO = Path(os.environ.get("OTGW_SATPID_INO", REPO / "src" / "OTGW-firmware" / "SATpid.ino"))
TIMERS = (REPO / "src" / "OTGW-firmware" / "safeTimers.h").as_posix()

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <ctime>
#define F(x) x
#define PSTR(x) x
#define SATDebugTf(...) ((void)0)
#define SATDebugTln(x) ((void)0)
#define DebugTln(...) ((void)0)
#define SAT_MIN_SETPOINT 10.0f
#define SAT_MAX_ZONES 4
#define SAT_HSYS_UNDERFLOOR 2
static const float SAT_HC_REF_TEMP = 20.0f;
typedef uint8_t byte;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static long random(long a, long) { return a; }
#include "@TIMERS@"
// bAutoGains/fK*Manual: pre-TASK-1357 fields at their old defaults, so an old SATpid.ino copy still compiles.
struct SatSettings { bool bSolarFreezeIntegral = false; bool bAutoGains = true;
  float fKpManual = 5.0f, fKiManual = 0.0005f, fKdManual = 0.0f;
  float fHeatingCurveCoeff = 1.6f, fDeadband = 0.1f, fTargetTemp = 21.0f;
  int iZoneCount = 1; int iZoneTimeoutS = 900; };
struct Settings { SatSettings sat; };
struct SatState { bool bSolarGainActive = false, bFallbackActive = false, bPvBoostActive = false;
  uint32_t iLastKnownRoomMs = 0; float fPvBoostAppliedC = 0;
  float fPidP = 0, fPidI = 0, fPidD = 0, fPidOutput = 0, fKp = 0, fKi = 0, fKd = 0, fRawDerivative = 0, fError = 0; };
struct State { SatState sat; };
struct SATZoneState { float fRoomTemp = 0, fSetpoint = 0, fPidOutput = 0, fPidIntegral = 0, fPrevError = 0;
                      uint32_t iLastUpdateMs = 0; bool bRoomValid = false, bSpValid = false, bOff = false; };
static Settings settings; static State state; static SATZoneState satZones[SAT_MAX_ZONES];
static int satGetEffectiveHeatingSystem() { return 1; }   // radiators
static float satGetBaseOffset() { return 27.2f; }
static float satGetMaxSetpoint() { return 62.0f; }
static float satGetOutsideTemp() { return 5.0f; }
float satPidError(float target, float room);
bool satPidGatherInputs(float* room, float* target, float* curve, float* deadbandExtra);
static float satGetRoomTemp();
@ZONE@
@PID@
// ---- scripted sensors ----
static uint32_t g_base = 0;            // clock at t = 0
static float g_curveNow = 40.0f;
static bool g_roomValid = true;
static int g_gathers = 0;
static int g_trace = 0;                // 0: error between .01 and .09 (inside the deadband), 1: ramp outside the deadband
static int g_stepLog[64]; static int g_stepN = 0;
static float roomAt(double t) {
  if (g_trace == 1) return (float)(19.0 + 0.002 * t);
  return (float)(21.0 - (0.05 + 0.04 * sin(t / 97.0)));
}
static float g_rawRoom = 20.5f;
static float satGetRoomTemp() { return g_rawRoom; }
@GATHER@
@SAVE@
"""

SAVE_STUBS = r"""
#define PROGMEM
#define FPSTR(x) x
#define snprintf_P snprintf
static char g_saved[256];
struct File { bool ok = true; operator bool() const { return ok; } void print(const char* s) { strncpy(g_saved, s, 255); } void close() {} };
struct FSStub { File open(const char*, const char*) { return File(); } };
static FSStub LittleFS;
static uint32_t _pidLastSaveMs = 0;
static const char* SAT_PID_STATE_FILE = "/x.json";
static int satPidStateAppendOutside(char*, int n, size_t, time_t) { return n; }   // TASK-1482: tested in test_sat_restart_quiet.py
"""

GATHER_STUB = r"""
bool satPidGatherInputs(float* room, float* target, float* curve, float* deadbandExtra) {
  g_gathers++;
  if (g_stepN < 64) g_stepLog[g_stepN++] = (int)((uint32_t)(g_ms - g_base) / 1000);
  *room = roomAt((g_ms - g_base) / 1000.0); *target = 21.0f; *curve = g_curveNow; *deadbandExtra = 0.0f;
  return g_roomValid;
}
"""

MAIN = r"""
#ifdef HAVE_SERVICE
#define ROOMSEEN satPidRoomSeen()
#else
#define ROOMSEEN 0
#endif
static void pass(int t) {
  g_ms = g_base + (uint32_t)t * 1000u;
  for (int z = 0; z < 2; z++) { satZones[z].iLastUpdateMs = g_ms; }
#ifdef HAVE_SERVICE
  satPidService(g_ms);
#endif
}
static float tickPid(int t, float curve) {      // what satControlLoopRun() does for the PID on a tick
  if (!g_roomValid) return satPidCurveOnly(curve);
#ifdef HAVE_SERVICE
  satPidNoteRoom(roomAt(t), g_ms);
  return satPidOutput(curve);
#else
  return satPidUpdate(roomAt(t), 21.0f, curve, 45.0f);
#endif
}
static float tickZone() {
#ifdef HAVE_SERVICE
  return satZonePidOutput(0, 5.0f);
#else
  return satZonePidStep(0, 5.0f);
#endif
}
int main(int argc, char** argv) {
  const char* sc = argv[1]; const int tick = atoi(argv[2]); g_base = (uint32_t)strtoul(argv[3], 0, 10);
  g_ms = g_base; satPidReset();
  if (!strcmp(sc, "ramp")) { g_trace = 1; }
  if (!strcmp(sc, "zone")) {
    settings.sat.iZoneCount = 2;
    satZones[0].fRoomTemp = 20.95f; satZones[0].fSetpoint = 21.0f; satZones[0].bRoomValid = satZones[0].bSpValid = true;
  }
  if (!strcmp(sc, "zone1")) {                       // TASK-1440: zone count 1, zone 1 pushes data
    settings.sat.iZoneCount = 1;
    satZones[0].fRoomTemp = 20.95f; satZones[0].fSetpoint = 21.0f; satZones[0].bRoomValid = satZones[0].bSpValid = true;
  }
  if (!strcmp(sc, "zone1none")) {                   // TASK-1440: zone count 1 (the default), no zone data pushed
    settings.sat.iZoneCount = 1;
  }
  const bool widen = !strcmp(sc, "widen");          // REAL satPidGatherInputs: argv[4] = age of the estimate (ms), argv[5] = takeover active
  const uint32_t estMs = argc > 4 ? (uint32_t)strtoul(argv[4], 0, 10) : 0;
  const bool takeover = argc > 5 && atoi(argv[5]) != 0;
  if (widen) {
    settings.sat.iZoneCount = 2;
    satZones[0].fRoomTemp = 20.5f; satZones[0].fSetpoint = 21.0f; satZones[0].bRoomValid = satZones[0].bSpValid = true;
  }
  if (!strcmp(sc, "fresh")) { g_trace = 1; }   // SAT enable, no restore: room 19.0 against target 21.0
  if (!strcmp(sc, "first")) { satPidRestoreState(0.5f, 0.02f); }
  if (!strcmp(sc, "hold") || !strcmp(sc, "holdsave")) { satPidRestoreState(0.5f, 0.02f); g_roomValid = false; }   // boot: state restored, no room reading yet
  for (int t = 0; t <= 600; t++) {
    if (!strcmp(sc, "stall") && t > 100 && t < 300) continue;      // the loop is blocked for 200 s: no pass at all
    if (!strcmp(sc, "nan") && t == 130) g_roomValid = false;
    if (!strcmp(sc, "recover") && t == 130) g_roomValid = false;
    if (!strcmp(sc, "recover") && t == 190) g_roomValid = true;
    if (!strcmp(sc, "reenable") && t == 131) { satPidReset(); }
    if ((!strcmp(sc, "hold") || !strcmp(sc, "holdsave")) && t == 200) g_roomValid = true;
    if (!strcmp(sc, "late") && t == 45) satPidRestoreState(0.5f, 0.02f);   // the deferred restore after NTP lands after the first output
    if (!strcmp(sc, "hold") && t == 400) g_roomValid = false;
    if (widen) {
      state.sat.bFallbackActive = takeover;
      state.sat.iLastKnownRoomMs = takeover ? (g_ms - estMs) : 0;   // the estimate is estMs old at this pass
      if (takeover && state.sat.iLastKnownRoomMs == 0) state.sat.iLastKnownRoomMs = 1;
    }
    pass(t);
    if (t % tick) continue;
    if (!strcmp(sc, "ramp")) {
      g_curveNow = 40.0f + 0.1f * (float)t;                          // outside/target change reaches the curve between steps
      float out = tickPid(t, g_curveNow);
      printf("t=%d curve=%.4f out=%.5f I=%.9f err=%.9f rawD=%.9f P=%.5f D=%.5f kp=%.9f kd=%.9f\n", t, g_curveNow, out,
             state.sat.fPidI, state.sat.fError, state.sat.fRawDerivative, state.sat.fPidP, state.sat.fPidD, state.sat.fKp, state.sat.fKd);
    } else if (!strcmp(sc, "nan")) {
      float out = tickPid(t, g_curveNow);
#ifdef HAVE_SERVICE
      float raw = satPidOutput(g_curveNow);                          // the output function alone, on the held (NAN) error
#else
      float raw = out;
#endif
      printf("t=%d valid=%d out=%.5f raw=%.5f P=%.5f I=%.5f D=%.5f\n", t, (int)g_roomValid, out, raw, state.sat.fPidP, state.sat.fPidI, state.sat.fPidD);
    } else if (!strcmp(sc, "recover")) {
      float out = tickPid(t, g_curveNow);
      printf("t=%d valid=%d out=%.5f P=%.5f I=%.5f D=%.5f\n", t, (int)g_roomValid, out, state.sat.fPidP, state.sat.fPidI, state.sat.fPidD);
    } else if (!strcmp(sc, "hold")) {
      float out = tickPid(t, g_curveNow);
      printf("t=%d valid=%d out=%.5f I=%.6f rawD=%.6f shownI=%.6f shownRawD=%.6f seen=%d\n", t, (int)g_roomValid, out, _pid_integral,
             _pid_rawDerivative, state.sat.fPidI, state.sat.fRawDerivative, (int)ROOMSEEN);
    } else if (!strcmp(sc, "holdsave")) {
      float out = tickPid(t, g_curveNow);
      satSavePidState();                                             // a save at this moment, whatever the callers' guards say
      float si = -99, sd = -99, sr = -99;
      sscanf(g_saved, "{\"i\":%f,\"d\":%f,\"rd\":%f", &si, &sd, &sr);
      printf("t=%d valid=%d out=%.5f I=%.5f D=%.5f seen=%d savedI=%.5f savedRd=%.5f\n", t, (int)g_roomValid, out, state.sat.fPidI,
             state.sat.fPidD, (int)ROOMSEEN, si, sr);
    } else if (!strcmp(sc, "fresh")) {
      float out = tickPid(t, g_curveNow);
      printf("t=%d curve=%.5f out=%.5f kp=%.9f err=%.9f I=%.9f D=%.9f\n", t, g_curveNow, out, state.sat.fKp, state.sat.fError,
             state.sat.fPidI, state.sat.fPidD);
    } else if (!strcmp(sc, "first") || !strcmp(sc, "late")) {
      float out = tickPid(t, g_curveNow);
      printf("t=%d out=%.5f D=%.5f rawD=%.5f kd=%.5f\n", t, out, state.sat.fPidD, state.sat.fRawDerivative, state.sat.fKd);
    } else if (!strcmp(sc, "zone") || !strcmp(sc, "zone1") || !strcmp(sc, "zone1none")) {
      tickPid(t, g_curveNow);
      float zo = tickZone();
      if (t == 600) printf("zoneI=%.6f zoneOut=%.5f steps=%d\n", satZones[0].fPidIntegral, zo, g_gathers);
    } else {
      tickPid(t, g_curveNow);
    }
  }
  if (widen) {
    printf("I=%.6f zoneI=%.6f deadband=%.6f\n", state.sat.fPidI, satZones[0].fPidIntegral, settings.sat.fDeadband);
  }
  if (!strcmp(sc, "cad") || !strcmp(sc, "stall") || !strcmp(sc, "reenable")) {
    printf("steps=%d I=%.6f log=", g_gathers, state.sat.fPidI);
    for (int i = 0; i < g_stepN; i++) printf("%d,", g_stepLog[i]);
    printf("\n");
  }
  return 0;
}
"""


def _fn(src: str, sig: str) -> str:
    m = re.search(r"^" + sig + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(f"{sig} not found")
    return m.group(0)


def zone_source(sat: str) -> str:
    parts = [_fn(sat, r"static float satCalcHeatingCurve\("), _fn(sat, r"static inline float satZonePidExclude\(")]
    if "satZonePidOutput(" in sat:
        parts += [_fn(sat, r"static float satOutsideTempOrDefault\("), _fn(sat, r"static bool satZoneUsable\("),
                  _fn(sat, r"static float satZoneKp\("), _fn(sat, r"static void satZonePidAdvance\("),
                  _fn(sat, r"void satZonePidAdvanceAll\("), _fn(sat, r"static float satZonePidOutput\(")]
    else:
        parts += [_fn(sat, r"static float satZonePidStep\(")]
    return "\n".join(parts)


def real_gather_source(sat: str) -> str:
    """The REAL input path of a PID step: satPidGatherInputs and the helpers it calls, cut from SATcontrol.ino."""
    limit = re.search(r"^static const uint32_t SAT_THERMAL_MAX_EST_MS[^\n]*", sat, re.M)
    if not limit:
        raise AssertionError("SAT_THERMAL_MAX_EST_MS not found")
    return "\n".join([limit.group(0), _fn(sat, r"static bool satRoomTempValid\("), _fn(sat, r"static float satRoomFiltered\("),
                      _fn(sat, r"static float satEffectiveTarget\("), _fn(sat, r"static float satEstDeadbandExtra\("),
                      _fn(sat, r"bool satPidGatherInputs\(")])


def save_source(sat: str) -> str:
    return SAVE_STUBS + _fn(sat, r"void satSavePidState\(")


def build_cpp(sat: str, pid: str, real_gather: bool = False) -> str:
    pid = pid[pid.index("// --- PID Constants ---"):]
    main = ("#define HAVE_SERVICE 1\n" if "void satPidService(" in pid else "") + MAIN
    gather = real_gather_source(sat) if real_gather else GATHER_STUB
    return (PRELUDE.replace("@TIMERS@", TIMERS).replace("@ZONE@", zone_source(sat)).replace("@PID@", pid)
            .replace("@GATHER@", gather).replace("@SAVE@", save_source(sat)) + main)


def parse(line):
    return {k: v for k, v in (kv.split("=", 1) for kv in line.split())}


class Cadence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cls.sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.pid = PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.head = "void satPidService(" not in cls.pid
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(cls.sat, cls.pid), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc, tick, base=1000000):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc, str(tick), str(base)], capture_output=True, text=True, check=True).stdout
        return [parse(line) for line in out.strip().split("\n")]

    def cad(self, tick, base=1000000):
        r = self.run_sc("cad", tick, base)[-1]
        if self.head:                      # HEAD has no step counter: the gather stub is never called
            return -1, float(r["I"]), []
        return int(r["steps"]), float(r["I"]), [int(x) for x in r["log"].split(",") if x]

    def test_a_integral_and_step_count_independent_of_the_control_tick(self):
        _, ref, _ = self.cad(60)
        rows = []
        for tick in (10, 30, 45, 60, 120, 300):
            steps, integral, _ = self.cad(tick)
            rows.append((tick, steps, integral))
        print("\n  tick_s steps I   (600 s, 60 s reference I=%.6f)" % ref)
        for tick, steps, integral in rows:
            print("  %5d %5d %.6f" % (tick, steps, integral))
        for tick, steps, integral in rows:
            with self.subTest(tick_s=tick):
                self.assertAlmostEqual(integral, ref, delta=1e-4, msg="I at tick %d s differs from the 60 s reference" % tick)
                # floor(600/60) = 10 timer steps (within one) plus the start-up step
                self.assertTrue(abs((steps - 1) - 10) <= 1, "steps=%d" % steps)

    def test_b_millis_wrap_gives_the_same_steps_and_integral(self):
        _, ref, _ = self.cad(30)
        steps, integral, _ = self.cad(30, base=4294967295 - 125000)    # millis() wraps 125 s into the run
        self.assertAlmostEqual(integral, ref, delta=1e-4)
        self.assertTrue(abs((steps - 1) - 10) <= 1, "steps=%d" % steps)

    def test_c_loop_stall_gives_one_step(self):
        r = self.run_sc("stall", 30)[-1]
        if self.head:
            self.fail("HEAD has no PID service: the stall case does not apply (steps are not counted)")
        log = [int(x) for x in r["log"].split(",") if x]
        inside = [t for t in log if 100 < t <= 300]
        self.assertEqual(len(inside), 1, "steps during the 200 s stall window: %s (log %s)" % (inside, log))
        after = [t for t in log if t > 300]
        self.assertTrue(all(b - a >= 60 for a, b in zip([inside[0]] + after, after)), "no catch-up burst: %s" % log)

    def test_d_startup_step_at_the_first_pass(self):
        steps, _, log = self.cad(30)
        self.assertEqual(log[0], 0, "the first step is the start-up step, before the first 60 s expiry: %s" % log)
        self.assertEqual(log[1], 60)

    def test_e_ticks_between_steps_hold_state_but_output_follows_the_curve(self):
        rows = self.run_sc("ramp", 10)
        by_step = {}
        for r in rows:
            t = int(r["t"])
            by_step.setdefault(t // 60, []).append(r)   # the PID steps at t = 0 (start-up), 60, 120, ... before that pass's tick
        self.assertTrue(any(abs(float(r["rawD"])) > 0 for r in rows), "premise: the raw derivative is non-zero somewhere")
        changed_curve = 0
        for r in rows:
            curve, out = float(r["curve"]), float(r["out"])
            kp, kd = float(r["kp"]), float(r["kd"])
            self.assertAlmostEqual(kp, 1.6 * curve / 3.0, places=3, msg="kp follows the CURRENT curve (pid.py:60-93)")
            self.assertAlmostEqual(out, curve + kp * float(r["err"]) + float(r["I"]) + kd * float(r["rawD"]), delta=2e-3,
                                   msg="output = curve + kp(curve)*held error + I + kd(curve)*held raw derivative (pid.py:126-132)")
        for key, group in by_step.items():
            if key < 0:
                continue
            with self.subTest(window=key):
                self.assertEqual(len({(g["I"], g["err"], g["rawD"]) for g in group}), 1,
                                 "I, error and raw derivative must not move between two PID steps")
                if len(group) > 1:
                    changed_curve += 1
                    self.assertNotEqual(group[0]["out"], group[-1]["out"], "the output must follow the changed curve between steps")
        self.assertGreater(changed_curve, 0)

    def test_f_no_room_gives_curve_only_never_nan(self):
        rows = self.run_sc("nan", 30)
        before = [r for r in rows if int(r["t"]) <= 120]
        self.assertNotEqual(float(before[-1]["I"]), 0.0, "premise: I is non-zero before the loss")
        after = [r for r in rows if int(r["t"]) >= 150]
        for r in after:
            for key in ("out", "raw"):
                out = float(r[key])
                self.assertTrue(math.isfinite(out), "output must never be NaN: %s" % r)
                self.assertAlmostEqual(out, 40.0, places=4, msg="curve only (%s)" % key)
            self.assertEqual((float(r["P"]), float(r["I"]), float(r["D"])), (0.0, 0.0, 0.0))

    def test_g_zone_integral_on_the_60_s_timer(self):
        _, ref, _ = self.cad(60)
        zi = {}
        for tick in (10, 30, 60, 120):
            r = self.run_sc("zone", tick)[-1]
            zi[tick] = float(r["zoneI"])
        print("\n  zone I after 600 s by tick_s:", zi)
        for tick, v in zi.items():
            with self.subTest(tick_s=tick):
                self.assertAlmostEqual(v, zi[60], delta=1e-4, msg="zone I must not depend on the control tick")
        if not self.head:
            steps = int(self.run_sc("zone", 10)[-1]["steps"])
            self.assertTrue(abs((steps - 1) - 10) <= 1)
            # kp = 1.6*curve/3, ki = kp/8400, error 0.05: one step adds ki*0.05*60
            curve = 27.2 + (1.6 / 4.0) * (4.0 * (21.0 - 20.0) + 0.03 * 15.0 ** 2 + 0.4 * 15.0)
            ki = (1.6 * curve / 3.0) / 8400.0
            self.assertAlmostEqual(zi[10], ki * 0.05 * 60.0 * steps, delta=1e-4, msg="zone I = steps * ki*error*60")

    def test_g2_zone_count_1_steps_zone_1_and_leaves_an_unconfigured_zone_excluded(self):
        """TASK-1440: the zone block counts a single zone, so zone 1 steps at zone count 1 too.
        A zone that never received data stays excluded (satZoneUsable): integral 0, output 10,
        so a default single-room setup sees no change."""
        if self.head:
            self.skipTest("HEAD has no step counter")
        two = float(self.run_sc("zone", 60)[-1]["zoneI"])
        one = self.run_sc("zone1", 60)[-1]
        print(f"\n  zone I after 600 s: count 2 {two:.6f}, count 1 {float(one['zoneI']):.6f}")
        self.assertGreater(two, 0.0)
        self.assertAlmostEqual(float(one["zoneI"]), two, delta=1e-6,
                               msg="zone 1 must step at zone count 1 as it does at count 2")
        none = self.run_sc("zone1none", 60)[-1]
        self.assertEqual(float(none["zoneI"]), 0.0, "an unconfigured zone accumulates nothing")
        self.assertAlmostEqual(float(none["zoneOut"]), 10.0, places=4,
                               msg="an unconfigured zone returns SAT_MIN_SETPOINT, so it does not count")

    def test_h_first_output_after_a_reset_has_no_d_term(self):
        """Entry 95 Q39b: D = 0 on the first tick after boot or SAT enable, although a restored raw derivative is held."""
        if self.head:
            self.fail("HEAD has no separate output function: the first tick adds the restored D")
        rows = self.run_sc("first", 30)
        first, second = rows[0], rows[1]
        self.assertAlmostEqual(float(first["rawD"]), 0.02, places=5, msg="premise: the restored raw derivative is held")
        self.assertGreater(float(first["kd"]), 1.0, "premise: Kd is non-zero")
        self.assertEqual(float(first["D"]), 0.0, "first output after the reset: D must be 0 (%s)" % first)
        self.assertAlmostEqual(float(second["D"]), float(second["kd"]) * 0.02, delta=1e-3,
                               msg="later outputs carry D = kd * raw derivative (%s)" % second)

    def test_i_reenable_takes_a_startup_step_again(self):
        """satDisable() -> satPidReset(): the next service call is a start-up step and restarts the 60 s timer."""
        if self.head:
            self.fail("HEAD has no PID service")
        r = self.run_sc("reenable", 30)[-1]
        log = [int(x) for x in r["log"].split(",") if x]
        self.assertEqual(log[:3], [0, 60, 120], log)
        self.assertEqual(log[3], 131, "start-up step on the pass of the reset at t=131: %s" % log)
        self.assertEqual(log[4], 191, "the timer restarted at the start-up step: %s" % log)

    def test_j_room_back_gives_curve_only_until_the_next_step(self):
        """After a loss the held error is NaN. When the room temperature is valid again the output stays curve-only (P=I=D=0)
        until the next 60 s step stores a real error: up to 60 s. That matches thermo-nova, whose output only
        changes when its PID steps (the step at t=240 here)."""
        if self.head:
            self.fail("HEAD updates the PID on the tick: the room is back at once")
        rows = {int(r["t"]): r for r in self.run_sc("recover", 30)}
        for t in (150, 180):
            self.assertEqual(rows[t]["valid"], "0")
        back = rows[210]                                   # valid again since t=190, the steps are at 180 (lost) and 240
        self.assertEqual(back["valid"], "1")
        self.assertAlmostEqual(float(back["out"]), 40.0, places=4, msg="curve only until the next step: %s" % back)
        self.assertEqual((float(back["P"]), float(back["I"]), float(back["D"])), (0.0, 0.0, 0.0))
        step = rows[240]                                   # the service steps before this tick, with the real error
        self.assertNotAlmostEqual(float(step["out"]), 40.0, places=2, msg="the step at 240 stores a real error: %s" % step)
        self.assertNotEqual(float(step["P"]), 0.0)

    def test_k_restored_state_survives_until_a_room_temperature_was_seen(self):
        """Entry 115: after a boot with a restored PID state and no valid room reading yet, the steps and ticks without a room
        keep the restored integral and raw derivative and give the curve; once a reading was seen, a lost reading resets (entry 46)."""
        if self.head:
            self.fail("HEAD resets the restored state when the room temperature is missing")
        rows = {int(r["t"]): r for r in self.run_sc("hold", 30)}
        for t in range(0, 200, 30):                         # no reading since the boot: start-up step and 60 s steps
            r = rows[t]
            self.assertEqual(r["valid"], "0")
            self.assertEqual((float(r["I"]), float(r["rawD"])), (0.5, 0.02), "restored state kept at t=%d: %s" % (t, r))
            self.assertEqual((float(r["shownI"]), float(r["shownRawD"])), (0.5, 0.02))
            self.assertAlmostEqual(float(r["out"]), 40.0, places=4, msg="output is the curve meanwhile: %s" % r)
            self.assertEqual(r["seen"], "0", "satSavePidState() skips while this is 0")
        r = rows[210]                                        # a reading is valid since t=200, the next step is at 240
        self.assertEqual(r["seen"], "1")
        self.assertGreater(float(r["I"]), 0.5, "the first reading takes a step at once on top of the restored I (round 3): %s" % r)
        self.assertLess(float(r["I"]), 0.6, r)
        r = rows[270]                                        # the step at 240 integrated on top of the restored I
        self.assertGreater(float(r["I"]), 0.5, r)
        self.assertLess(float(r["I"]), 0.7, "two steps add ki*error*60 = a few hundredths each: %s" % r)
        r = rows[420]                                        # lost again at t=400, after a reading was seen: reset
        self.assertEqual((float(r["I"]), float(r["rawD"])), (0.0, 0.0), r)
        self.assertAlmostEqual(float(r["out"]), 40.0, places=4)


    def test_l_first_reading_after_boot_steps_at_once_with_the_restored_integral(self):
        """Entry 115 (round 3): restored state, no room until t=200 s, uptime > 5 min. At the first pass with a valid room the PID
        takes a step at once (as the start-up step would), so the output and the published I use the restored I and the new error,
        instead of the curve alone until the next 60 s step. A save at that moment writes the live integral, never 0. The timer
        restarts at that step (next step at 260, not 240)."""
        if self.head:
            self.fail("HEAD has no restore hold")
        rows = {int(r["t"]): r for r in self.run_sc("holdsave", 10)}
        for t in range(0, 200, 10):
            r = rows[t]
            self.assertEqual((float(r["I"]), float(r["savedI"]), float(r["savedRd"])), (0.5, 0.5, 0.02), "before the reading: %s" % r)
        for t in (200, 210, 230, 250):
            r = rows[t]
            self.assertEqual(r["valid"], "1")
            self.assertGreater(float(r["I"]), 0.5, "published I uses the restored I at t=%d: %s" % (t, r))
            self.assertLess(float(r["I"]), 0.6, r)
            self.assertAlmostEqual(float(r["savedI"]), float(r["I"]), places=3, msg="a save writes the live integral, never 0: %s" % r)
            self.assertAlmostEqual(float(r["savedRd"]), 0.02, places=4, msg=str(r))
            self.assertNotAlmostEqual(float(r["out"]), 40.0, places=2, msg="output has the PID terms from the first pass: %s" % r)
        self.assertGreater(float(rows[260]["I"]), float(rows[250]["I"]), "the timer restarted at t=200: the next step is at 260")

    def test_o_first_tick_after_enable_sends_a_computed_setpoint(self):
        """TASK-1270 AC#1: after satPidReset() (SAT enable) the first control tick (t=0) and the one 30 s later send a computed
        setpoint, not the uninitialised 0 that gave CS=10/CH=0. The start-up step makes the first output curve + kp*error + I,
        with I the integral after that one step (0 here: no restored state) and D = 0 (entry 95 Q39b). kp and the error are
        what the real code computed (printed), not values derived from the fix."""
        rows = {int(r["t"]): r for r in self.run_sc("fresh", 30)}
        first, second = rows[0], rows[30]
        print("\n  first tick : %s\n  tick t=30  : %s" % (first, second))
        curve, out = float(first["curve"]), float(first["out"])
        kp, err, i_term = float(first["kp"]), float(first["err"]), float(first["I"])
        self.assertGreater(out, 10.0, "first tick must not send the SAT minimum CS=10, first output is %s (%s)" % (first["out"], first))
        self.assertGreater(err, 1.0, "premise: room 19.0 is well below target 21.0 (%s)" % first)
        self.assertGreater(kp, 0.0, "premise: the code computed a gain (%s)" % first)
        self.assertGreater(out, curve, "first output must carry the P term above the curve (%s)" % first)
        self.assertAlmostEqual(out, curve + kp * err + i_term, delta=2e-3,
                               msg="first output = curve + kp*error + I (%s)" % first)
        self.assertEqual(float(first["D"]), 0.0, "D = 0 on the first output (%s)" % first)
        self.assertGreater(float(second["out"]), curve, "the tick 30 s later is still a computed setpoint (%s)" % second)

    def test_m_first_output_with_pid_terms_has_d_zero_then_d(self):
        """Entry 95 Q39b (round 3): the first output that includes the PID terms has D = 0, also when it comes after a restore and
        a stretch of curve-only outputs; the next output has D = kd * raw derivative (curve 40, coefficient 1.6: kd = 0.07 * 8400 * 1.6 * 40 / 3 = 12544, raw 0.02 -> 250.88)."""
        if self.head:
            self.fail("HEAD has no restore hold")
        rows = {int(r["t"]): r for r in self.run_sc("holdsave", 10)}
        self.assertEqual(float(rows[200]["D"]), 0.0, rows[200])
        self.assertAlmostEqual(float(rows[210]["D"]), KD_AT_CURVE_40 * 0.02, places=3, msg=str(rows[210]))

    def test_n_late_restore_gives_d_zero_on_the_next_output(self):
        """Round 3: the deferred restore (after NTP) lands at t=45 after the first outputs. It re-arms the first-output rule, so the
        next output has D = 0; the one after has D = kd * raw derivative. (Whether a late restore should overwrite a live PID at
        all is a separate question and not changed here.)"""
        if self.head:
            self.fail("HEAD has no first-output flag")
        rows = {int(r["t"]): r for r in self.run_sc("late", 10)}
        self.assertEqual(float(rows[40]["rawD"]), 0.0, "premise: nothing restored yet at t=40: %s" % rows[40])
        self.assertAlmostEqual(float(rows[50]["rawD"]), 0.02, places=4, msg=str(rows[50]))
        self.assertEqual(float(rows[50]["D"]), 0.0, "the first output after the restore has D = 0: %s" % rows[50])
        self.assertAlmostEqual(float(rows[60]["D"]), KD_AT_CURVE_40 * 0.02, places=3, msg=str(rows[60]))


class Widening(unittest.TestCase):
    """The thermal-estimation deadband widening through the REAL satPidGatherInputs / satEstDeadbandExtra / satEffectiveTarget /
    satRoomFiltered path, with the real satPidService: the main step and the zone step both see the widened deadband while
    the room temperature is estimated (MQTT takeover), the widening ends where the control tick returns (2 h), and the
    deadband is restored afterwards. Room 20.5 against target 21.0 is an error of 0.5, outside the normal 0.1 deadband."""

    HOUR = 3600000

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.has = "bool satPidGatherInputs(" in sat and "void satPidService(" in pid
        cls.exe = None
        if cls.has:
            cpp = d / "w.cpp"
            cpp.write_text(build_cpp(sat, pid, real_gather=True), encoding="utf-8")
            cls.exe = compile_cpp(cpp, d / "w.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_widen(self, est_ms, takeover):
        if not self.has:
            self.fail("satPidGatherInputs()/satPidService() do not exist (HEAD widens inside the tick)")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), "widen", "30", "1000000", str(est_ms), str(takeover)], capture_output=True, text=True,
                             check=True).stdout
        return parse(out.strip().split("\n")[-1])

    def test_takeover_estimate_widens_main_and_zone_step(self):
        r = self.run_widen(1 * self.HOUR, 1)               # 1 h of estimation: deadband 0.1 + 0.5 = 0.6 > error 0.5
        self.assertGreater(float(r["I"]), 0.0, "main PID integrates inside the widened deadband: %s" % r)
        self.assertGreater(float(r["zoneI"]), 0.0, "the zone step sees the same widened value: %s" % r)
        self.assertAlmostEqual(float(r["deadband"]), 0.1, places=6, msg="the deadband is restored after the service")

    def test_no_widening_without_takeover(self):
        r = self.run_widen(1 * self.HOUR, 0)
        self.assertEqual(float(r["I"]), 0.0, r)
        self.assertEqual(float(r["zoneI"]), 0.0, r)

    def test_no_widening_once_the_estimate_is_older_than_the_tick_limit(self):
        r = self.run_widen(3 * self.HOUR, 1)               # the tick returned before widening from 2 h on
        self.assertEqual(float(r["I"]), 0.0, r)
        self.assertEqual(float(r["zoneI"]), 0.0, r)
        self.assertAlmostEqual(float(r["deadband"]), 0.1, places=6)


class SourceTruth(unittest.TestCase):
    """STRUCTURAL: a comment that says the opposite of the code is a bug."""

    def test_comment_about_the_takeover_is_true(self):
        sat = SAT_INO.read_text(encoding="utf-8")
        self.assertNotIn("no MQTT takeover", sat)
        self.assertNotIn("Don't update PID integral", sat)

    def test_save_skips_until_a_room_temperature_was_seen(self):
        # Structural: the periodic and persist-all saves skip while no room was seen (entry 115); the explicit
        # resets in satDisable() and satFlushShortLivedData() still persist their zeros (unguarded calls).
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = sat.index("void satSavePidState()")
        self.assertNotIn("satPidRoomSeen", sat[a:a + 200])
        self.assertIn("if (satPidRoomSeen()) satSavePidState();", sat)
        self.assertIn(">= 300000UL && satPidRoomSeen()) {", sat)
        for fn, save in (("void satDisable(", "satPidReset();\n  if (persistPid) satSavePidState();"),
                         ("void satFlushShortLivedData(", "satPidReset();\n  satSavePidState();")):
            b = sat.index(fn)
            body = sat[b:sat.index("\n}\n", b)]
            self.assertIn(save, body, fn)


class RoomFilter(unittest.TestCase):
    """The room EMA is shared by the PID step and the tick: a second call in the same ms must not filter again."""

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.has = "static float satRoomFiltered(" in src
        if not cls.has:
            cls.exe = None
            return
        body = _fn(src, r"static float satRoomFiltered\(")
        cpp = d / "f.cpp"
        cpp.write_text('#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\nstatic uint32_t g_ms = 0;\n'
                       'static uint32_t millis() { return g_ms; }\n' + body + r"""
int main(int argc, char** argv) {   // argv: "ms:temp" pairs
  for (int i = 1; i < argc; i++) {
    unsigned long ms; double t; sscanf(argv[i], "%lu:%lf", &ms, &t);
    g_ms = (uint32_t)ms; printf("%.6f\n", satRoomFiltered((float)t));
  }
  return 0;
}
""", encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "f.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def values(self, *pairs):
        if not self.has:
            self.fail("satRoomFiltered() does not exist (HEAD keeps the EMA inline in the tick)")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)] + ["%d:%s" % p for p in pairs], capture_output=True, text=True, check=True).stdout
        return [float(x) for x in out.split()]

    def test_second_call_in_the_same_ms_does_not_filter_again(self):
        a = self.values((1000, 20.0), (31000, 21.0), (31000, 21.0))
        self.assertAlmostEqual(a[1], a[2], places=6)
        self.assertNotAlmostEqual(a[1], 21.0, places=2, msg="premise: the filter really smooths")

    def test_tick_and_step_in_one_pass_equal_a_tick_alone(self):
        alone = self.values((1000, 20.0), (31000, 21.0))[1]
        both = self.values((1000, 20.0), (31000, 21.0), (31000, 21.0), (31001, 21.0))[3]
        self.assertAlmostEqual(alone, both, delta=1e-3)


class SourceOrder(unittest.TestCase):
    """STRUCTURAL: satControlLoopRun() cannot be compiled on the host; the order of the calls is checked in the source."""

    @classmethod
    def setUpClass(cls):
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = sat.index("static void satControlLoopRun()\n{")
        cls.loop = sat[a:sat.index("\n}\n", a)]
        cls.pid = PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.sat = sat

    def at(self, text):
        self.assertIn(text, self.loop, text)
        return self.loop.index(text)

    def test_service_after_the_sat_off_returns_before_every_tick_return(self):
        svc = self.at("satPidService(millis());")
        for before in ("if (satMqttFallbackStep()) return;", "if (!satIsEnabled() || isFlashing()) {",
                       "if (state.sat.bSafetyTripped) return;", "if (satOtLinkStep()) return;"):
            self.assertLess(self.at(before), svc, "%s must stop the PID" % before)
        for after in ("if (!DUE(timerSATControl)) return;", "if (state.sat.bDhwActive) {",
                      "if (OTcurrentSystemState.SlaveStatus & 0x01) {", "_sat_thermalSafeActive = true;",
                      "if (state.sat.bSummerActive && settings.sat.bSummerSimmer) {", "if (!state.sat.bValvesOpen) {"):
            self.assertLess(svc, self.at(after), "the PID must step also where %s returns" % after)

    def test_tick_reads_the_pid_and_never_steps_it(self):
        self.assertIn("satPidOutput(curveValue)", self.loop)
        self.assertNotIn("satPidStep(", self.loop)
        self.assertNotIn("satPidUpdate(", self.sat)
        self.assertIn("satZonePidOutput(z, outsideTemp)", self.loop)
        self.assertNotIn("satZonePidAdvance(", self.loop)

    def test_timer_is_typed_and_never_retyped(self):
        self.assertIn("DECLARE_TIMER_SEC(timerSATPid, 60, SKIP_MISSED_TICKS);", self.pid)
        for text in (self.sat, self.pid):
            self.assertNotRegex(text, r"CHANGE_INTERVAL_(SEC|MS|MIN)\(\s*timerSATPid")
        settings_ino = (REPO / "src" / "OTGW-firmware" / "settingStuff.ino").read_text(encoding="utf-8")
        self.assertNotRegex(settings_ino, r"CHANGE_INTERVAL_(SEC|MS|MIN)\(\s*timerSATPid")

    def test_no_gates_left_in_the_step(self):
        self.assertNotIn("SAT_PID_SAMPLE_TIME_LIMIT", self.pid)
        self.assertNotIn("_pid_lastUpdateMs", self.pid)


if __name__ == "__main__":
    unittest.main(verbosity=2)
