#!/usr/bin/env python3
"""TASK-1366: host test for the SAT PWM duty mapper with a heat pump (Sergeant D entries 40 and 50).

Heat pumps in PWM use the same 180 s minimum ON time as gas boilers (thermo-nova pwm.py:53) and a
PWM cycle length (period) of 1800 s (2 per hour, default) or 2400 s (1.5 per hour). The duty
mapping is thermo-nova's _calculate_duty_cycle (pwm.py:229-301) with that period
(pwm.py:54-61). Before the fix the firmware used 1800/2400 s as the minimum ON time, which made
dutyLower >= 1 and collapsed the mapper to continuous or ultra-low (audit #9).

The code under test is NOT re-implemented here. The test cuts the real getter block
(satGetMaxCyclesPerHour .. satAlwaysMaxModulation) and the real PWM block (the same cut as
tests/test_sat_pwm_override.py, which this file reuses) out of src/OTGW-firmware/SATcontrol.ino,
and the real right-hand side of the SAThpcycle setter out of settingStuff.ino, and compiles them
with a host C++ compiler. Without a compiler the tests are skipped, they do not pass silently.

Gas oracle: thermo-nova tests/test_pwm.py:108-133 (flow 50, 4 cycles per hour), which must stay
green on HEAD and on the fix.

Old-vs-fix: OTGW_SAT_INO / OTGW_SETTINGS_INO point at other copies, e.g. the output of
`git show HEAD:src/OTGW-firmware/SATcontrol.ino`.

Run: python tests/test_sat_hp_pwm.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (shared cut + compiler helpers)

REPO = base.REPO
SAT_INO = base.SAT_INO
SETTINGS_INO = Path(os.environ.get("OTGW_SETTINGS_INO", REPO / "src" / "OTGW-firmware" / "settingStuff.ino"))

SRC_GAS, SRC_HP, SRC_HYB = 1, 2, 3

MAIN = r"""
static void reset() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_csSent = 0; g_csLast = 0; g_hasOT = true; g_cold = 28.2f;
  _pwm_waitingForFlame = false; _pwm_flameOffHoldSetpoint = 0; _pwm_flameOnMs = 0; _pwm_lastOffTimeMs = 1; _pwm_lastOnTimeMs = 1;
  _pwm_effectiveBoilerTemp = 0; _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
}
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  if (!strcmp(argv[1], "setter")) {          // setter value: the real SAThpcycle right-hand side
    printf("stored=%d\n", (int)hpCycleSetter(argv[2]));
    return 0;
  }
  if (!strcmp(argv[1], "r3")) {              // r3 source hpSec cyclesPerHour ratio: ON start, flame not lit yet
    reset();
    g_source = atoi(argv[2]); settings.sat.iHpCycleSeconds = (uint16_t)atoi(argv[3]); settings.sat.iCyclesPerHour = atoi(argv[4]);
    const float pid = 27.2f + (float)atof(argv[5]) * (50.0f - 27.2f);
    g_flame = false; state.sat.bPwmFlameRequested = false;   // OFF phase
    g_flameOnStart = g_ms - 4000000UL; g_flameOffStart = g_ms - 3000000UL; _pwm_lastUpdateMs = g_ms - 3000000UL;  // previous burn: lit 4000 s ago, out 3000 s ago
    satApplyPWM(pid);                                         // tick N: OFF -> ON
    const int startReq = (int)state.sat.bPwmFlameRequested;
    g_ms += 30000UL; satApplyPWM(pid);                        // tick N+1: still no flame (slow ignition / compressor delay)
    printf("start=%d req=%d\n", startReq, (int)state.sat.bPwmFlameRequested);
    return 0;
  }
  // map source hpSec cyclesPerHour ratio flame [onAgoSec]
  reset();
  g_source = atoi(argv[2]); settings.sat.iHpCycleSeconds = (uint16_t)atoi(argv[3]); settings.sat.iCyclesPerHour = atoi(argv[4]);
  const float ratio = (float)atof(argv[5]);
  g_flame = atoi(argv[6]) != 0;
  const float pid = 27.2f + ratio * (50.0f - 27.2f);       // thermo-nova test_pwm.py:120-124
  state.sat.bPwmFlameRequested = true;
  if (argc > 7) {                                           // an ON phase whose flame lit onAgoSec ago
    const uint32_t ago = (uint32_t)atoi(argv[7]) * 1000UL;
    g_flameOnStart = g_ms - ago; _pwm_flameOnMs = g_ms - ago; _pwm_lastUpdateMs = g_ms - ago;   // the phase anchor (pwm.py:188, :205)
  } else {
    g_flameOnStart = g_ms; _pwm_lastUpdateMs = g_ms;        // flame edge now: the ON phase does not end in this call
  }
  satApplyPWM(pid);
  printf("on=%.3f off=%.3f req=%d duty=%.4f\n", _pwm_lastOnTimeMs / 1000.0, _pwm_lastOffTimeMs / 1000.0,
         (int)state.sat.bPwmFlameRequested, state.sat.fPwmDutyCycle);
  return 0;
}
"""


def extract_getters(src: str) -> str:
    a = src.index("// Returns max PWM cycles per hour")
    b = src.index("// Returns whether MM=100")
    return src[a:b]


def extract_setter() -> str:
    src = SETTINGS_INO.read_text(encoding="utf-8")
    m = re.search(r'PSTR\("SAThpcycle"\)\)\s*==\s*0\)\s*settings\.sat\.iHpCycleSeconds\s*=\s*(.*?);\s*//', src)
    if not m:
        raise AssertionError("SAThpcycle setter not found in settingStuff.ino")
    return ("static int hpCycleSetter(const char* newValue) { return (uint16_t)(" + m.group(1) + "); }\n")


def build(workdir: Path):
    src = SAT_INO.read_text(encoding="utf-8")
    body = base.extract(src)
    pre = ""
    for name, stub in (
        ("satComputeMM", "static uint8_t satComputeMM(bool,bool,bool,uint8_t,uint8_t){ return 255; }"),
        ("satPwmSuppressionRefresh", "static void satPwmSuppressionRefresh(){ }"),
        ("satPwmReset", "static void satPwmReset(){ }"),
        ("satSetControlMode", "static void satSetControlMode(int m){ state.sat.eControlMode = m; }"),
        ("_sat_lastControlCS", "static float _sat_lastControlCS = 0;"),
        ("_sat_thermalSafeActive", "static bool _sat_thermalSafeActive = false;"),
        ("satPwmDisableOnHeaterOff", "static void satPwmDisableOnHeaterOff(){ }"),
    ):
        if name not in body:
            pre += stub + "\n"
    cycles = "static uint32_t satPwmLastOffTimeMs() { return _pwm_lastOffTimeMs; }\n" + base.extract_cycles()
    flag = "#define HAS_REQSP 1\n" if "_sat_requestedSetpoint" in body else ""
    has_force = "bForcePWM" in body
    has_auto = "bPwmAutoSwitch" in cycles
    stubs = base.STUBS
    # the real getters replace the harness's fixed stubs
    for line in ("static uint32_t satGetMinOnTimeSec() { return 180; }\n",
                 "static uint8_t satGetMaxCyclesPerHour() { return 4; }\n"):
        if line not in stubs:
            raise AssertionError("getter stub not found in the shared harness: " + line)
        stubs = stubs.replace(line, "")
    stubs = re.sub(r"^static uint32_t satGetPwmPeriodSec\(\).*\n", "", stubs, flags=re.M)
    stubs = stubs.replace("float fOvershootMargin=2.0f;",
                          "uint8_t iCyclesPerHour=3; uint16_t iHpCycleSeconds=1800; float fOvershootMargin=2.0f;")
    stubs = (stubs.replace("@FORCEFIELD@", "bool bForcePWM=false;" if has_force else "")
                  .replace("@AUTOFIELD@", "bool bPwmAutoSwitch=true;" if has_auto else "")
                  .replace("@CHENABLE@", (REPO / "src" / "OTGW-firmware" / "SATchEnable.h").as_posix()))
    src_stubs = r"""
enum { SAT_HSYS_AUTO = 0, SAT_HSYS_RADIATORS = 1, SAT_HSYS_UNDERFLOOR = 2 };
enum { SAT_SRC_AUTO = 0, SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2, SAT_SRC_HYBRID = 3 };
static int g_source = SAT_SRC_GAS_BOILER;
static uint8_t satGetEffectiveHeatingSource() { return (uint8_t)g_source; }
static uint8_t satGetEffectiveHeatingSystem() { return SAT_HSYS_RADIATORS; }
template <typename T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
"""
    cpp = workdir / "hp.cpp"
    cpp.write_text(flag + stubs + src_stubs + extract_getters(src) + pre + body + cycles + extract_setter() + MAIN,
                   encoding="utf-8")
    return base.compile_cpp(cpp, workdir / "hp.exe")


class SatHeatPumpPwm(unittest.TestCase):
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

    def assertMap(self, source, hp_sec, cph, ratio, flame, expected, tol=1):
        r = self.run_sc("map", source, hp_sec, cph, ratio, int(flame))
        on, off = float(r["on"]), float(r["off"])
        msg = (f"source={source} hp={hp_sec} cph={cph} ratio={ratio} flame={int(flame)}: "
               f"got on/off {on:.0f}/{off:.0f} s (duty {r['duty']}), expected {expected[0]}/{expected[1]} s")
        self.assertLessEqual(abs(on - expected[0]), tol, msg)
        self.assertLessEqual(abs(off - expected[1]), tol, msg)

    # --- Heat pump, 1800 s period: lower 0.10, upper 0.90, min 0.05, max 0.95 (pwm.py:53-61) ---
    def test_hp1800_mid_range_duty_060(self):
        self.assertMap(SRC_HP, 1800, 3, 0.60, True, (1080, 720))        # pwm.py:274-283: P*d, P*(1-d)

    def test_hp1800_mid_range_duty_030(self):
        self.assertMap(SRC_HP, 1800, 3, 0.30, True, (540, 1260))

    def test_hp1800_just_below_half(self):
        self.assertMap(SRC_HP, 1800, 3, 0.4999, True, (900, 900))

    def test_hp1800_low_range(self):
        self.assertMap(SRC_HP, 1800, 3, 0.08, False, (180, 2070))      # pwm.py:263-272: 180, 180/d - 180

    def test_hp1800_high_range(self):
        self.assertMap(SRC_HP, 1800, 3, 0.92, False, (2070, 180))      # pwm.py:285-294: 180/(1-d) - 180, 180

    def test_hp1800_ultra_low_with_flame(self):
        self.assertMap(SRC_HP, 1800, 3, 0.03, True, (180, 3420))       # pwm.py:247-256: 180, 2P - 180

    def test_hp1800_ultra_low_without_flame(self):
        self.assertMap(SRC_HP, 1800, 3, 0.03, False, (0, 3600))        # pwm.py:258-260: 0, 2P

    def test_hp1800_over_max(self):
        self.assertMap(SRC_HP, 1800, 3, 0.97, False, (3600, 0))        # pwm.py:296-301: 2P, 0

    # --- Heat pump, 2400 s period: lower 0.075, upper 0.925, min 0.0375, max 0.9625 ---
    def test_hp2400_mid_range_duty_050(self):
        self.assertMap(SRC_HP, 2400, 3, 0.50, True, (1200, 1200))

    def test_hp2400_mid_range_duty_030(self):
        self.assertMap(SRC_HP, 2400, 3, 0.30, True, (720, 1680))

    def test_hp2400_low_range(self):
        self.assertMap(SRC_HP, 2400, 3, 0.07, True, (180, 2391))

    def test_hp2400_high_range(self):
        self.assertMap(SRC_HP, 2400, 3, 0.95, False, (3420, 180))

    def test_hp2400_over_max(self):
        self.assertMap(SRC_HP, 2400, 3, 0.98, False, (4800, 0))

    # --- ON phase end: min ON is 180 s, not the cycle length ---
    def test_hp1800_on_phase_ends_after_on_time(self):
        # duty 0.30 -> on 540 s; flame lit 600 s ago: the ON phase ends (HEAD held it to 1800 s)
        r = self.run_sc("map", SRC_HP, 1800, 3, 0.30, 1, 600)
        self.assertEqual(r["req"], "0", f"ON phase still requested after 600 s with on={r['on']} s")

    def test_hp1800_on_phase_holds_before_on_time(self):
        r = self.run_sc("map", SRC_HP, 1800, 3, 0.30, 1, 300)
        self.assertEqual(r["req"], "1")

    # --- Gas boiler: thermo-nova oracle tests/test_pwm.py:108-117 (4 per hour), unchanged ---
    def test_gas_oracle_4ph(self):
        for ratio, flame, exp in ((0.05, False, (0, 1800)), (0.05, True, (180, 1620)), (0.15, False, (180, 1020)),
                                  (0.50, False, (450, 450)), (0.85, False, (1020, 180)), (0.95, False, (1800, 0))):
            with self.subTest(ratio=ratio, flame=flame):
                self.assertMap(SRC_GAS, 1800, 4, ratio, flame, exp)

    def test_gas_3ph_default(self):
        self.assertMap(SRC_GAS, 2400, 3, 0.50, False, (600, 600))     # the HP setting does not touch gas

    def test_gas_on_phase_min_on_180(self):
        r = self.run_sc("map", SRC_GAS, 1800, 4, 0.10, 1, 170)       # range 2, on 180 s: still ON at 170 s
        self.assertEqual(r["req"], "1")

    # --- R3, pre-existing and not fixed by TASK-1366 (needs Sergeant D's OK) ---
    # The ON end is timed from the cycle tracker's previous flame-on edge (SATcontrol.ino:949-958,
    # SATcycles.ino:722, :1045), so an ON phase whose flame has not lit yet ends on the next tick and the
    # 180 s ignition wait never holds. thermo-nova times ON from the phase start and waits up to 180 s
    # for the flame (pwm.py:181-199). Built on the gas path, which is the same on HEAD and on the fix.
    # Since TASK-1366 a heat pump in PWM at duty 0.05-0.95 takes this same path.
    def test_r3_on_start_without_flame_starts(self):
        r = self.run_sc("r3", SRC_GAS, 1800, 4, 0.50)
        self.assertEqual(r["start"], "1", "OFF phase with off time passed did not start ON")

    def test_r3_on_phase_waits_for_flame(self):
        r = self.run_sc("r3", SRC_GAS, 1800, 4, 0.50)
        self.assertEqual(r["req"], "1", "ON phase ended 30 s after its start, before the flame lit (R3)")

    # --- Hybrid: unchanged by this task (entries 50/52 leave hybrids open) ---
    def test_hybrid_unchanged(self):
        self.assertMap(SRC_HYB, 2400, 3, 0.50, False, (600, 600))
        self.assertMap(SRC_HYB, 2400, 3, 0.10, False, (180, 1620))

    # --- Setter: two options only (entry 30 "just 2 options"); old values snap to the nearer one ---
    def test_setter_snaps_to_two_options(self):
        for given, stored in (("1800", 1800), ("2400", 2400), ("2100", 2400), ("2099", 1800), ("0", 1800), ("9999", 2400)):
            with self.subTest(given=given):
                self.assertEqual(int(self.run_sc("setter", given)["stored"]), stored)


if __name__ == "__main__":
    unittest.main(verbosity=2)
