#!/usr/bin/env python3
"""TASK-1387: host test for the end-of-cycle classifier against thermo-nova cycles/classifier.py.

Sergeant D entry 55 (docs/sat/sergeant-d-corrections.md, Q4 answer A): the firmware classifier must match
thermo-nova cycles/classifier.py (pin 7beaaf5) exactly, with these firmware deviations from the plan:
  - cold setpoint 28.2 (radiators, entry 11) through satGetColdSetpoint(), not COLD_SETPOINT 22;
  - overshoot margin settings.sat.fOvershootMargin = 2.0 (entry 15), not OVERSHOOT_MARGIN_CELSIUS 3.0;
  - PWM underheat is labelled SAT_CYCLE_UNDERHEAT_PWM, continuous underheat SAT_CYCLE_UNDERHEAT;
  - controlSp <= 0 is the firmware stand-in for "flow_control_setpoint_error p50/p90 missing";
  - cycle kind from tracker.py:314-324, CH = master CH-enable bit (Statusflags & 0x0100) and not DHW,
    DHW = slave DHW bit (Statusflags & 0x04).

Nothing is re-implemented. The real code is cut out of src/OTGW-firmware/SATcycles.ino with regexes:
every SAT_* constant, every _cycle_/_flow_/_tail_/_sustain_/_ema_/_win4h globals, SATCycleRecord, and the
contiguous block from _flowPercentile() to the end of satCycleSample() (so _cycleDetectKind, _cycleRecord,
_cycleClassify, satCycleOnFlameChange and any helper the fix adds in that block come along). The real
SAT enums/structs (SATRuntimeSection, SATSection) come from SATtypes.h. Compiled with a host C++ compiler
(compile_cpp from test_sat_pwm_override); without one the compiled cases are skipped.

Seam contract (what the fix must keep reachable):
  - fFinalSetpoint (control setpoint) and _sat_requestedSetpoint (requested setpoint, also mirrored to
    state.sat.fPidOutput) are set before satCycleOnFlameChange(true) and stay constant over the cycle
    (per-sample setpoints are out of scope, TASK-1405).
  - Per 1 Hz sample: millis() += 1000, then flow, OTcurrentSystemState.Statusflags, state.sat.bDhwActive
    (= Statusflags & 0x04), state.sat.eControlMode and state.sat.bPwmFlameRequested are set and
    satCycleSample() is called. The cycle ends with satCycleOnFlameChange(false) at the last sample's time,
    so the duration in seconds equals the sample count.
  - "PWM ON at cycle end" = mode PWM and bPwmFlameRequested on the last sample. "PWM IDLE" =
    satPwmIsIdle() true. satPwmIsIdle() is always a stub returning a test-controlled flag (SATcontrol.ino is
    not compiled here); when SATcontrol.ino defines it, the regex only copies its return type. Case R4 so
    tests how the classifier reacts to the IDLE flag, not the idle detection itself.
  - The result is read from state.sat.eLastCycleClass / eLastCycleKind, written by the real _cycleRecord().
  - satPwmLastOnTimeMs() is the PWM on-time (stub).
  - settings.sat.bSimulation selects the simulation inputs of the kind count (DHW = bDhwActive, CH = CS above
    SAT_MIN_SETPOINT); SAT_MIN_SETPOINT is a stub copy of SATcontrol.ino's constant.
  - The oracle's tail follows tracker.py (warm-up exclusion included), not the firmware's 180 s ring.

An optional oracle (class ThermoNovaOracle) loads the real classifier.py without Home Assistant (stub
package with COLD_SETPOINT=28.2, the real types.py and cycles/const.py, OVERSHOOT_MARGIN_CELSIUS forced
to 2.0 on the loaded module) and checks the expected class of every case against it. The kind passed to
the oracle is computed from the scenario's flags with tracker.py:314-324's thresholds (mirrored here,
four comparisons); percentiles use percentile_interpolated (helpers.py:187) semantics.

OTGW_SATCYCLES_INO / OTGW_SAT_INO / OTGW_SATTYPES_H point at other copies (e.g. `git show HEAD:...`)
for old-vs-fix runs.

Run: python tests/test_sat_cycle_classifier.py
"""
import importlib.util
import os
import re
import subprocess
import sys
import tempfile
import types as pytypes
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402  (shared compiler discovery)

FW = REPO / "src" / "OTGW-firmware"
CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", FW / "SATcycles.ino"))
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
SATTYPES_H = Path(os.environ.get("OTGW_SATTYPES_H", FW / "SATtypes.h"))
TN = REPO / "other-projects" / "SAT-releases-thermo-nova" / "custom_components" / "sat"

# --- status flags ---------------------------------------------------------------------------------------
CH = 0x0100 | 0x02 | 0x08          # master CH enable + slave CH mode + flame
DHW = 0x04 | 0x08                  # slave DHW mode + flame, no master CH enable
CHDHW = CH | 0x04                  # CH enabled but the boiler is on DHW
NONE = 0x08                        # flame only: neither CH nor DHW
CONT, PWM = 1, 2
CLS = {"NONE": 0, "GOOD": 1, "OVERSHOOT": 2, "UNDERHEAT": 3, "SHORT": 4, "UNCERTAIN": 5,
       "UNDERHEAT_PWM": 6, "INSUFFICIENT": 7}
CLS_NAME = {v: k for k, v in CLS.items()}
KIND = {"UNKNOWN": 0, "CH": 1, "DHW": 2, "MIXED": 3}
KIND_NAME = {v: k for k, v in KIND.items()}

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define PROGMEM
#define PGM_P const char*
#define SAT_TAIL_SAMPLE_SIZE 180
#define SAT_FLOW_SAMPLE_SIZE 256
#define SAT_WIN4H_SIZE 360
#define HCR_DAYS 30
#define HCR_INTRADAY_SIZE 1440
#define SAT_BLE_MAX_ROSTER 8
#define SAT_MAX_ZONES 4
typedef uint16_t SAT_RING_IDX_T;
"""

STUBS = r"""
struct SatDebug { bool bSAT = false; };
struct State { SATRuntimeSection sat; SatDebug debug; };
@SETTINGS@
static State state;
struct OTSys { uint16_t Statusflags = 0; uint8_t MasterStatus = 0; uint8_t SlaveStatus = 0; float Tr = 20.0f; float Tboiler = 0; float Tret = 30.0f; };
static OTSys OTcurrentSystemState;
static uint32_t g_ms = 1000000UL; static float g_flow = 50.0f; static float g_cold = 28.2f;
static uint32_t g_onTimeMs = 0; static bool g_idle = false;
static float _sat_requestedSetpoint = 0.0f;
static const float SAT_MIN_SETPOINT = 10.0f;   // SATcontrol.ino:129
static uint32_t millis() { return g_ms; }
static float satGetFlowTemp() { return g_flow; }
static float satGetReturnTemp() { return 30.0f; }
static float satGetColdSetpoint() { return g_cold; }
static uint32_t satPwmLastOnTimeMs() { return g_onTimeMs; }
static uint32_t satPwmLastOffTimeMs() { return 0; }
static @IDLE_RET@ satPwmIsIdle() { return g_idle; }
static void sendWebSocketJSON(const char*) {}
#define satNarratef_P(...) ((void)0)
#define snprintf_P snprintf
static size_t strlcpy_P(char* d, const char* s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
static void _hourCountRecord(uint32_t) {}
static uint8_t _hourCountGet(uint32_t) { return 1; }
static void _dailyRecord(uint32_t, uint32_t, uint32_t, SATCycleClass, float) {}
#define SATDebugTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
#define SATDebugf(...) ((void)0)
"""

SETTINGS_REAL = "struct Settings { SATSection sat; };\nstatic Settings settings;\n"
SETTINGS_STUB = ("struct SatSettingsStub { float fOvershootMargin = 2.0f; bool bPwmAutoSwitch = true; bool bSimulation = false; };\n"
                 "struct Settings { SatSettingsStub sat; };\nstatic Settings settings;\n")

MAIN = r"""
static const unsigned CHFLAGS = 0x0100 | 0x02 | 0x08;   // master CH enable + slave CH mode + flame
static void apply(float flow, unsigned flags, int mode, int pwmOn) {
  g_flow = flow;
  OTcurrentSystemState.Statusflags = (uint16_t)flags;
  OTcurrentSystemState.MasterStatus = (uint8_t)(flags >> 8);
  OTcurrentSystemState.SlaveStatus = (uint8_t)(flags & 0xFF);
  state.sat.bDhwActive = (flags & 0x04) != 0;
  state.sat.eControlMode = (SATControlMode)mode;
  state.sat.bPwmFlameRequested = pwmOn != 0;
}
// TASK-1409: two CH cycles with a 600 s off gap, run through the real satCycleOnFlameChange()/satCycleSample()
// and the real satGetWindow4hStats(). Prints what the real code stored after each flame-off.
static void report(int n) {
  const SATWindowRecord& w = _win4h[(_win4hHead + SAT_WIN4H_SIZE - 1) % SAT_WIN4H_SIZE];
  printf("c%d_off_ms=%lu c%d_on_ms=%lu c%d_duty=%.9g count=%d\n", n, (unsigned long)w.offDurationMs, n,
         (unsigned long)w.onDurationMs, n, (double)state.sat.fDutyRatio, (int)_win4hCount);
  satGetWindow4hStats();
  printf("c%d_4h_avg_off_s=%.9g c%d_4h_avg_on_s=%.9g c%d_4h_duty=%.9g c%d_4h_n=%d\n", n,
         (double)state.sat.f4hAvgOffSec, n, (double)state.sat.f4hAvgOnSec, n, (double)state.sat.f4hDutyRatio,
         n, (int)state.sat.i4hCycles);
}
static int twoCycle() {
  settings.sat.fOvershootMargin = 2.0f;
  state.sat.fFinalSetpoint = 50.0f; _sat_requestedSetpoint = 50.0f; state.sat.fPidOutput = 50.0f;
  printf("init_ema_alpha=%.9g init_duty=%.9g\n", (double)EMA_ALPHA, (double)state.sat.fDutyRatio);
  for (int n = 1; n <= 2; n++) {
    if (n == 2) g_ms += 600000;                    // the 600 s off gap (flame off since cycle 1 ended)
    apply(50.0f, CHFLAGS, 1, 0); satCycleOnFlameChange(true);
    for (int k = 0; k < 300; k++) { g_ms += 1000; apply(50.0f, CHFLAGS, 1, 0); satCycleSample(); }
    satCycleOnFlameChange(false);                  // flame off at the last sample's time
    report(n);
  }
  return 0;
}
// argv: ctrlSp reqSp onTimeSec idle sim seg...   seg = count:flow:flags:mode:pwmOn
int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "twocycle") == 0) return twoCycle();
  if (argc < 6) return 2;
  settings.sat.fOvershootMargin = 2.0f;
  const float ctrlSp = (float)atof(argv[1]), reqSp = (float)atof(argv[2]);
  g_onTimeMs = (uint32_t)(atof(argv[3]) * 1000.0 + 0.5);
  g_idle = atoi(argv[4]) != 0;
  settings.sat.bSimulation = atoi(argv[5]) != 0;   // SAT simulation: no OT status flags (TASK-1387 review)
  state.sat.fFinalSetpoint = ctrlSp;
  _sat_requestedSetpoint = reqSp;
  state.sat.fPidOutput = reqSp;
  int first = 1;
  for (int i = 6; i < argc; i++) {
    unsigned cnt = 0, flags = 0; float flow = 0; int mode = 1, pwmOn = 0;
    if (sscanf(argv[i], "%u:%f:%i:%d:%d", &cnt, &flow, &flags, &mode, &pwmOn) != 5) return 3;
    if (first) { apply(flow, flags, mode, pwmOn); satCycleOnFlameChange(true); first = 0; }
    for (unsigned k = 0; k < cnt; k++) {
      g_ms += 1000;
      apply(flow, flags, mode, pwmOn);
      satCycleSample();
    }
  }
  if (first) { apply(50.0f, 0x010A, 1, 0); satCycleOnFlameChange(true); }   // no samples: zero-length cycle
  satCycleOnFlameChange(false);
  printf("cls=%d kind=%d\n", (int)state.sat.eLastCycleClass, (int)state.sat.eLastCycleKind);
  return 0;
}
"""


def _grab(pattern, src, what):
    m = re.search(pattern, src, re.S | re.M)
    if not m:
        raise AssertionError("%s not found" % what)
    return m


def extract_types(src: str, real_settings: bool) -> str:
    """The SAT enums and structs from SATtypes.h, up to the end of SATRuntimeSection (and SATSection)."""
    a = src.index("enum SATHeatingSystem")
    b = _grab(r"^struct SATRuntimeSection\b.*?^\};", src, "struct SATRuntimeSection").end()
    out = src[a:b] + "\n"
    if real_settings:
        out += _grab(r"^struct SATSection\b.*?^\};", src, "struct SATSection").group(0) + "\n"
    return out


def extract_cycles(src: str) -> str:
    src = src.replace("\r\n", "\n")
    consts = re.findall(r"^static const \w+\s+(?:SAT_\w+|EMA_ALPHA)\s*=[^;\n]*;", src, re.M)
    globs = re.findall(r"^static (?:const )?\w+\s+(?:_cycle|_flow|_tail|_sustain|_ema|_win4h)\w*(?:\[[^\]]*\])?[^;\n(]*;",
                       src, re.M)
    rec = _grab(r"^struct SATCycleRecord\s*\{.*?^\};", src, "struct SATCycleRecord").group(0)
    guards = _grab(r"^void satCycleResetSustainGuards\(\)\s*\{.*?^\}", src, "satCycleResetSustainGuards").group(0)
    win = _grab(r"^void satGetWindow4hStats\(\)\s*\{.*?^\}", src, "satGetWindow4hStats").group(0)   # TASK-1409
    a = _grab(r"^static float _flowPercentile\(", src, "_flowPercentile").start()
    b = _grab(r"^void satCycleSample\(\)\s*\{.*?^\}", src, "satCycleSample").end()
    return "\n".join(consts) + "\n" + rec + "\n" + "\n".join(globs) + "\n" + guards + "\n" + src[a:b] + "\n" + win + "\n"


def idle_return_type(sat_src: str) -> str:
    m = re.search(r"^(?:static\s+)?(?:inline\s+)?(\w+)\s+satPwmIsIdle\s*\(\s*(?:void)?\s*\)\s*\{", sat_src, re.M)
    return m.group(1) if m else "bool"


def build(wd: Path):
    cycles = CYCLES_INO.read_text(encoding="utf-8")
    sat = SAT_INO.read_text(encoding="utf-8")
    types_src = SATTYPES_H.read_text(encoding="utf-8").replace("\r\n", "\n")
    body = extract_cycles(cycles)
    last_err = None
    for real in (True, False):   # the real SATSection first; a hand-written settings stub as fallback
        stubs = (STUBS.replace("@SETTINGS@", SETTINGS_REAL if real else SETTINGS_STUB)
                 .replace("@IDLE_RET@", idle_return_type(sat)))
        cpp = wd / ("t_real.cpp" if real else "t_stub.cpp")
        cpp.write_text(PRELUDE + extract_types(types_src, real) + stubs + body + MAIN, encoding="utf-8")
        try:
            exe = compile_cpp(cpp, wd / ("t_real.exe" if real else "t_stub.exe"))
            return exe, ("real SATSection" if real else "settings stub")
        except (AssertionError, subprocess.CalledProcessError) as e:
            last_err = e
    raise AssertionError("host compile failed for both settings variants:\n%s" % last_err)


# --- scenarios ------------------------------------------------------------------------------------------
# seg(count, flow, flags=CH, mode=CONT, pwmOn=0)
def seg(n, flow, flags=CH, mode=CONT, on=0):
    return (n, flow, flags, mode, on)


class Case:
    def __init__(self, segs, expect, ctrl=50.0, req=50.0, on_time=0.0, idle=False, kind=None, oracle=True, sim=False):
        self.segs, self.expect, self.ctrl, self.req, self.on_time, self.idle = segs, expect, ctrl, req, on_time, idle
        self.kind, self.oracle, self.sim = kind, oracle, sim


CASES = {
    # R1 cls:19-20 duration <= 0 -> INSUFFICIENT_DATA
    "R1": Case([], "INSUFFICIENT"),
    # R2 cls:22-23 kind UNKNOWN -> UNCERTAIN (no CH-enable, no DHW: both fractions 0, tracker.py:314-324)
    "R2": Case([seg(300, 50.0, NONE)], "UNCERTAIN"),
    # R3 cls:25-26 hot water in the tail -> UNCERTAIN (DHW in the last 10 s only: kind stays CH)
    "R3": Case([seg(290, 50.0), seg(10, 50.0, CHDHW)], "UNCERTAIN"),
    # R3 warm-up (tracker.py:113-116): a 100 s cycle has tail_start = max(min(60, 25), 100 - 180) = 25 s.
    # DHW at second 5 is in the warm-up, not in the tail -> classified (GOOD); DHW at second 30 is in it -> UNCERTAIN.
    "R3_warmup_out": Case([seg(4, 50.0), seg(1, 50.0, CHDHW), seg(95, 50.0)], "GOOD"),
    "R3_warmup_in": Case([seg(29, 50.0), seg(1, 50.0, CHDHW), seg(70, 50.0)], "UNCERTAIN"),
    # R2 with the slave in CH mode but the master CH-enable bit (8) clear: thermo-nova's central_heating is
    # the master CH enable (coordinator/serial/__init__.py:53-55), so the kind is UNKNOWN -> UNCERTAIN (correct)
    "R2_slave_ch_only": Case([seg(300, 50.0, 0x0A)], "UNCERTAIN"),
    # R2 at 75 % CH: ch 0.75 is not > 0.8 and dhw 0 is not > 0.1 -> UNKNOWN (tracker.py:314-324) -> UNCERTAIN.
    # A PWM cycle whose flame outlasts the CH=1 command (entry 5) lands here.
    "R2_ch75": Case([seg(225, 50.0, CH, PWM, 0), seg(75, 50.0, NONE, PWM, 0)], "UNCERTAIN", on_time=300),
    # SAT simulation: no OT status flags (0x08 = flame only); the commanded CS (> 10) stands in for the CH enable,
    # so the kind is CH and the cycle is classified. Oracle off: thermo-nova has no simulation input.
    "SIM_ch": Case([seg(300, 50.0, NONE)], "GOOD", sim=True, oracle=False),
    "SIM_ch_off": Case([seg(300, 50.0, NONE)], "UNCERTAIN", ctrl=10.0, sim=True, oracle=False),
    # R4 cls:28-29 PWM enabled and IDLE -> UNCERTAIN
    "R4": Case([seg(300, 50.0, CH, PWM, 0)], "UNCERTAIN", on_time=300, idle=True),
    # P1 cls:42-43 PWM requested-setpoint error p50/p90 missing -> UNCERTAIN (stand-in: _sat_requestedSetpoint <= 0)
    "P1": Case([seg(300, 50.0, CH, PWM, 1)], "UNCERTAIN", req=0.0, on_time=300, oracle=False),
    # P2 cls:45-54 PWM requested error p90 <= -3, requested >= cold -> UNDERHEAT (firmware UNDERHEAT_PWM)
    "P2": Case([seg(300, 45.0, CH, PWM, 1)], "UNDERHEAT_PWM", on_time=300),
    # P2cold cls:50-51 PWM underheat with requested 25 < cold 28.2 -> UNCERTAIN
    "P2cold": Case([seg(300, 20.0, CH, PWM, 1)], "UNCERTAIN", req=25.0, on_time=300),
    # P3 cls:57-61 PWM ON with on-time <= 0 -> SHORT_CYCLING (no 60 s fallback)
    "P3": Case([seg(300, 50.0, CH, PWM, 1)], "SHORT", on_time=0),
    # P4 cls:63-64 PWM ON, duration 100 < 0.8 * 300 -> SHORT_CYCLING
    "P4": Case([seg(100, 50.0, CH, PWM, 1)], "SHORT", on_time=300),
    # P5 cls:63-66 PWM ON, on-time 25 s, duration 25 s >= 20 -> GOOD (no 30 s floor; HEAD gives SHORT)
    "P5": Case([seg(25, 50.0, CH, PWM, 1)], "GOOD", on_time=25),
    # P6 cls:56 PWM OFF phase at cycle end: no SHORT check -> GOOD even at 20 s with on-time 300 s
    "P6": Case([seg(20, 50.0, CH, PWM, 0)], "GOOD", on_time=300),
    # P7 cls:66 PWM has no OVERSHOOT branch: flow 54 over a 50 setpoint -> GOOD
    "P7": Case([seg(300, 54.0, CH, PWM, 1)], "GOOD", on_time=300),
    # C1 cls:71-72 continuous control error missing (controlSp <= 0 stand-in) -> UNCERTAIN
    "C1": Case([seg(300, 50.0)], "UNCERTAIN", ctrl=0.0),
    # C2 cls:74-78 continuous control error p90 >= margin -> OVERSHOOT
    "C2": Case([seg(300, 54.0)], "OVERSHOOT"),
    # C2 boundary: p90 error exactly 2.0 >= 2.0 and no overshoot-seconds gate (HEAD counts only flow > 52)
    "C2_boundary": Case([seg(300, 52.0)], "OVERSHOOT"),
    # C2order (a): OVERSHOOT before any duration rule: 40 s continuous overshooting cycle -> OVERSHOOT
    "C2order_short": Case([seg(40, 54.0)], "OVERSHOOT"),
    # C2order (b): OVERSHOOT before UNDERHEAT: tail p90 53 (err +3), p50 45 (err -5) -> OVERSHOOT
    "C2order_underheat": Case([seg(220, 45.0), seg(80, 53.0)], "OVERSHOOT"),
    # C3 cls:80-88 continuous control error p50 <= -3, requested >= cold -> UNDERHEAT
    "C3": Case([seg(300, 45.0)], "UNDERHEAT"),
    # C3cold cls:85-86 continuous underheat with requested 25 < cold 28.2 -> UNCERTAIN
    "C3cold": Case([seg(300, 45.0)], "UNCERTAIN", req=25.0),
    # C4a cls:33-36 continuous, no rule fires -> GOOD
    "C4a": Case([seg(300, 50.0)], "GOOD"),
    # C4b no continuous SHORT and no blanket < 120 s UNCERTAIN (AC#2): 40 s and 90 s -> GOOD
    "C4b_40s": Case([seg(40, 50.0)], "GOOD"),
    "C4b_90s": Case([seg(90, 50.0)], "GOOD"),
    # kind (tracker.py:314-324): 15 % DHW, 15 % CH-only, 70 % neither -> MIXED (HEAD: CH, dhwFrac < 0.2)
    "kind_mixed": Case([seg(45, 50.0, DHW), seg(45, 50.0, CH), seg(210, 50.0, NONE)], None, kind="MIXED", oracle=False),
}


class CycleClassifier(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe, cls.variant = build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, name):
        if self.exe is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")
        c = CASES[name]
        args = [str(self.exe), repr(c.ctrl), repr(c.req), repr(float(c.on_time)), "1" if c.idle else "0", "1" if c.sim else "0"]
        args += ["%d:%r:0x%X:%d:%d" % s for s in c.segs]
        r = subprocess.run(args, capture_output=True, text=True, timeout=20)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr[-300:])
        m = re.match(r"cls=(\d+) kind=(\d+)", r.stdout)
        self.assertTrue(m, r.stdout)
        return int(m.group(1)), int(m.group(2))

    def check(self, name):
        cls_, _ = self.run_case(name)
        exp = CASES[name].expect
        self.assertEqual(CLS_NAME.get(cls_, cls_), exp, "%s: expected %s, firmware gave %s" % (name, exp, CLS_NAME.get(cls_, cls_)))

    def test_R1_zero_duration_is_insufficient(self):
        self.check("R1")

    def test_R2_kind_unknown_is_uncertain(self):
        self.check("R2")

    def test_R3_dhw_in_tail_is_uncertain(self):
        self.check("R3")

    def test_R3_dhw_in_the_warm_up_is_not_in_the_tail(self):
        with self.subTest("DHW at 5 s of a 100 s cycle: warm-up, classified"):
            self.check("R3_warmup_out")
        with self.subTest("DHW at 30 s of a 100 s cycle: in the tail, UNCERTAIN"):
            self.check("R3_warmup_in")

    def test_R2_master_ch_enable_decides_the_kind(self):
        with self.subTest("slave CH mode, master CH enable clear"):
            self.check("R2_slave_ch_only")
        with self.subTest("75 % CH, 25 % neither"):
            self.check("R2_ch75")

    def test_simulation_takes_ch_from_the_commanded_setpoint(self):
        with self.subTest("CS 50: CH"):
            self.check("SIM_ch")
        with self.subTest("CS 10 (CH=0): neither, UNCERTAIN"):
            self.check("SIM_ch_off")

    def test_R4_pwm_idle_is_uncertain(self):
        self.check("R4")

    def test_P1_pwm_requested_error_missing_is_uncertain(self):
        # Firmware stand-in for "flow_requested_setpoint_error p50/p90 missing" (cls:42-43):
        # _sat_requestedSetpoint <= 0 at flame-off. Not checked against the oracle (oracle=False):
        # classifier.py has no "requested setpoint 0" case, only None.
        self.check("P1")

    def test_P2_pwm_underheat(self):
        self.check("P2")

    def test_P2cold_pwm_underheat_below_cold_setpoint_is_uncertain(self):
        self.check("P2cold")

    def test_P3_pwm_on_with_zero_on_time_is_short(self):
        self.check("P3")

    def test_P4_pwm_on_shorter_than_80pct_on_time_is_short(self):
        self.check("P4")

    def test_P5_pwm_on_25s_cycle_with_25s_on_time_is_good(self):
        self.check("P5")

    def test_P6_pwm_off_phase_has_no_short_check(self):
        self.check("P6")

    def test_P7_pwm_has_no_overshoot(self):
        self.check("P7")

    def test_C1_control_setpoint_missing_is_uncertain(self):
        self.check("C1")

    def test_C2_continuous_overshoot(self):
        with self.subTest("flow 54"):
            self.check("C2")
        with self.subTest("p90 error exactly at the 2.0 margin, no overshoot-seconds gate"):
            self.check("C2_boundary")

    def test_C2order_overshoot_before_duration_and_underheat(self):
        with self.subTest("40 s overshooting continuous cycle is OVERSHOOT, not SHORT"):
            self.check("C2order_short")
        with self.subTest("p90 over margin and p50 under -3: OVERSHOOT wins"):
            self.check("C2order_underheat")

    def test_C3_continuous_underheat(self):
        self.check("C3")

    def test_C3cold_continuous_underheat_below_cold_setpoint_is_uncertain(self):
        self.check("C3cold")

    def test_C4a_continuous_good(self):
        self.check("C4a")

    def test_C4b_short_continuous_cycle_is_good(self):
        with self.subTest("40 s (HEAD: SHORT)"):
            self.check("C4b_40s")
        with self.subTest("90 s (HEAD: blanket < 120 s UNCERTAIN)"):
            self.check("C4b_90s")

    def test_task1409_off_gap_and_duty_come_from_the_previous_flame_off_edge(self):
        """Cycle 1: 300 s on, then 600 s off, cycle 2: 300 s on. The stored off gap of cycle 2 is 600 s and its
        duty is 300 / 900. HEAD zeroed the gap (flameOffStart was overwritten before use) and gave duty 1.0."""
        if self.exe is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")
        r = subprocess.run([str(self.exe), "twocycle"], capture_output=True, text=True, timeout=20)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr[-300:])
        v = dict(tok.split("=") for tok in r.stdout.split())
        alpha, duty0 = float(v["init_ema_alpha"]), float(v["init_duty"])
        # Cycle 1 is the first one after init: no previous flame-off edge exists, so the stored gap is 0
        # (not "time since init") and the duty is on/on = 1.0.
        self.assertEqual(int(v["c1_off_ms"]), 0, "cycle 1 (first after init) off gap")
        self.assertAlmostEqual(float(v["c1_duty"]), alpha * 1.0 + (1.0 - alpha) * duty0, places=5,
                               msg="cycle 1 EMA duty (no previous edge: duty 1.0)")
        # Cycle 2
        self.assertEqual(int(v["c2_on_ms"]), 300000, "cycle 2 on time")
        with self.subTest("stored off gap"):
            self.assertEqual(int(v["c2_off_ms"]), 600000, "cycle 2 stored off gap (ms)")
        with self.subTest("per-cycle duty (EMA)"):
            want = alpha * (300.0 / 900.0) + (1.0 - alpha) * float(v["c1_duty"])
            self.assertAlmostEqual(float(v["c2_duty"]), want, places=5,
                                   msg="cycle 2 EMA duty must use 300 s on / 900 s since the previous flame-off")
        # Rolling 4 h window over both cycles: off 0 + 600 s, on 300 + 300 s.
        self.assertEqual(int(v["c2_4h_n"]), 2)
        self.assertAlmostEqual(float(v["c2_4h_avg_on_s"]), 300.0, places=3)
        with self.subTest("4h average off time"):
            self.assertAlmostEqual(float(v["c2_4h_avg_off_s"]), (0 + 600) / 2.0, places=3, msg="4h avg off time")
        with self.subTest("4h duty ratio"):
            self.assertAlmostEqual(float(v["c2_4h_duty"]), 600.0 / (600.0 + 600.0), places=5, msg="4h duty ratio")

    def test_kind_mixed(self):
        _, kind = self.run_case("kind_mixed")
        self.assertEqual(KIND_NAME.get(kind, kind), "MIXED")


# --- thermo-nova oracle -----------------------------------------------------------------------------------
def _load_classifier():
    """Real cycles/classifier.py with a stub package (no Home Assistant). Returns (classifier module, types)."""
    pkg = "_tn_sat"
    for name in list(sys.modules):
        if name == pkg or name.startswith(pkg + "."):
            del sys.modules[name]
    root = pytypes.ModuleType(pkg); root.__path__ = [str(TN)]; sys.modules[pkg] = root
    const = pytypes.ModuleType(pkg + ".const"); const.COLD_SETPOINT = 28.2; sys.modules[pkg + ".const"] = const

    def load(modname, path):
        spec = importlib.util.spec_from_file_location(modname, path)
        mod = importlib.util.module_from_spec(spec)
        sys.modules[modname] = mod
        old = sys.dont_write_bytecode
        sys.dont_write_bytecode = True     # other-projects/ is read-only: no __pycache__ in the thermo-nova copy
        try:
            spec.loader.exec_module(mod)
        finally:
            sys.dont_write_bytecode = old
        return mod
    tntypes = load(pkg + ".types", TN / "types.py")
    cyc = pytypes.ModuleType(pkg + ".cycles"); cyc.__path__ = [str(TN / "cycles")]; sys.modules[pkg + ".cycles"] = cyc
    load(pkg + ".cycles.const", TN / "cycles" / "const.py")
    clf = load(pkg + ".cycles.classifier", TN / "cycles" / "classifier.py")
    clf.OVERSHOOT_MARGIN_CELSIUS = 2.0     # firmware margin (entry 15); `from .const import *` copied 3.0 in
    return clf, tntypes


def _pct(values, q):
    """percentile_interpolated (helpers.py:187-205)."""
    if not values:
        return None
    s = sorted(values)
    if len(s) == 1:
        return float(s[0])
    pos = (len(s) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (pos - lo)


def _kind(samples, tntypes):
    """tracker.py:101-108 + 314-324 (thresholds mirrored)."""
    n = len(samples)
    if n == 0:
        return tntypes.CycleKind.UNKNOWN
    dhw = sum(1 for _, f in samples if f & 0x04) / n
    ch = sum(1 for _, f in samples if not (f & 0x04) and (f & 0x0100)) / n
    if dhw > 0.8 and ch < 0.2:
        return tntypes.CycleKind.DOMESTIC_HOT_WATER
    if ch > 0.8 and dhw < 0.2:
        return tntypes.CycleKind.CENTRAL_HEATING
    if dhw > 0.1 and ch > 0.1:
        return tntypes.CycleKind.MIXED
    return tntypes.CycleKind.UNKNOWN


def oracle(c: Case):
    clf, T = _load_classifier()
    samples = [(flow, flags) for (n, flow, flags, _m, _o) in c.segs for _ in range(n)]
    # tracker.py:113-116, :237-241: sample k (1-based) at start + k s, the cycle ends at the last sample;
    # the tail is every sample at or after max(start + min(60, 0.25 * duration), end - 180). The firmware's
    # flow percentiles skip the warm-up exclusion (a recorded deviation); no case has a non-uniform flow in it.
    dur = float(len(samples))
    tail_start = max(min(60.0, 0.25 * dur), dur - 180.0)
    tail = [s_ for k, s_ in enumerate(samples, start=1) if k >= tail_start]
    flows = [f for f, _ in tail]
    ctrl_ok = c.ctrl > 0
    P = T.Percentiles
    ctrl_err = P(_pct([f - c.ctrl for f in flows], .5), _pct([f - c.ctrl for f in flows], .9)) if ctrl_ok else P()
    req_err = P(_pct([f - c.req for f in flows], .5), _pct([f - c.req for f in flows], .9))
    tm = pytypes.SimpleNamespace(flow_control_setpoint_error=ctrl_err, flow_requested_setpoint_error=req_err,
                                 requested_setpoint=P(c.req, c.req),
                                 hot_water_active_fraction=(sum(1 for _, f in tail if f & 0x04) / len(tail)) if tail else 0.0)
    last = c.segs[-1] if c.segs else (0, 0, 0, CONT, 0)
    enabled = last[3] == PWM
    status = T.PWMStatus.IDLE if c.idle else (T.PWMStatus.ON if last[4] else T.PWMStatus.OFF)
    pwm = pytypes.SimpleNamespace(enabled=enabled, status=status, on_time_seconds=float(c.on_time))
    dev = pytypes.SimpleNamespace(setpoint=c.ctrl)
    res = clf.CycleClassifier.classify(device_state=dev, duration_seconds=float(len(samples)), kind=_kind(samples, T),
                                       pwm_state=pwm, tail_metrics=tm)
    name = {"insufficient_data": "INSUFFICIENT", "short_cycling": "SHORT"}.get(res.value.lower(), res.name)
    if name == "UNDERHEAT" and enabled:
        name = "UNDERHEAT_PWM"
    return name


class ThermoNovaOracle(unittest.TestCase):
    """The expected class of each case equals what the real classifier.py says for the same inputs."""

    @classmethod
    def setUpClass(cls):
        if not (TN / "cycles" / "classifier.py").exists():
            raise unittest.SkipTest("thermo-nova reference not present")

    def test_expected_classes_match_classifier_py(self):
        for name, c in CASES.items():
            if not c.oracle:
                continue
            with self.subTest(name):
                self.assertEqual(oracle(c), c.expect)

    def test_kind_mixed_matches_tracker(self):
        _, T = _load_classifier()
        c = CASES["kind_mixed"]
        samples = [(flow, flags) for (n, flow, flags, _m, _o) in c.segs for _ in range(n)]
        self.assertEqual(_kind(samples, T), T.CycleKind.MIXED)


if __name__ == "__main__":
    unittest.main(verbosity=2)
