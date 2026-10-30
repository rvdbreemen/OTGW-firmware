#!/usr/bin/env python3
"""TASK-1414: the SAT PID resets its integral and raw derivative where thermo-nova does (Sergeant D entry 73, Q23).

thermo-nova (pinned 7beaaf5, custom_components/sat):
  PID.reset() zeroes _integral and _raw_derivative and nothing else (pid.py:134-137); reset_control_state() resets the
  main PID and every area PID (climate.py:461-464, area.py:335-338).
  Triggers: a target that really changes (climate.py:643-644, reset :660), so also a preset change or clear (:616, :624)
  and the window switch and restore (:819, :832); HVAC mode HEAT/OFF (:570-583); the reset_integral service
  (climate.py:217-220, services.py:31-39); a changed room-climate setpoint resets that area's PID (climate.py:785-795).

Nothing is re-implemented. The test compiles the REAL code with a host C++ compiler:
  - the whole SATpid.ino and the real safeTimers.h; the zone PID functions and satResetControlState / the target-change
    helper / satHandlePreset / satHandleWindow / _satCheckWindowTimer / satPresetClearForUserTarget /
    satHandleZoneSetpoint from SATcontrol.ino;
  - the SATtargettemp and SATenabled branches of updateSetting() and clampSettingFloat() from settingStuff.ino;
  - the MQTT reset_integral handler (_satResetIntegralCmd) from MQTTstuff.ino.
Stubs: the clock, the room sensor (a scripted ramp), MQTT/narration sinks and settings/state.

Per trigger the test lets the PID run for 130 s (so it is initialised and its 60 s timer runs), loads a non-zero integral,
raw derivative and zone integrals, fires the trigger and checks: integral, raw derivative (and the zone integrals) are 0
afterwards, while _pid_initialized, the start-up flags and the last error are untouched, and the PID steps keep the same
60 s schedule as a run without the trigger (AC#2, the TASK-1270 first-tick failure stays out). A target that does not
change must reset nothing.

Not reachable from the host: satDisable() (HVAC off) and the REST reset_integral handlers (checked at source level,
labelled structural).

OTGW_SATPID_INO / OTGW_SAT_INO / OTGW_SETTINGS_INO / OTGW_MQTT_INO point at other copies (`git show HEAD:<path>`) for the
old-vs-fix run.

Run: python tests/test_sat_pid_resets.py
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
import test_sat_pid_cadence as cad  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = REPO / "src" / "OTGW-firmware"
SETTINGS_INO = Path(os.environ.get("OTGW_SETTINGS_INO", SRC / "settingStuff.ino"))
MQTT_INO = Path(os.environ.get("OTGW_MQTT_INO", SRC / "MQTTstuff.ino"))
REST_INO = SRC / "restAPI.ino"

EXTRA_TYPES = r"""
enum SATPreset : uint8_t { SAT_PRESET_NONE = 0, SAT_PRESET_AWAY, SAT_PRESET_ECO, SAT_PRESET_COMFORT, SAT_PRESET_SLEEP,
                           SAT_PRESET_ACTIVITY, SAT_PRESET_HOME };
typedef const char* PGM_P;
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define DebugTf(...) ((void)0)
#define strcasecmp_P strcasecmp
#define strcmp_P strcmp
#define snprintf_P snprintf
#define EVALBOOLEAN(x) (!strcmp((x), "1") || !strcasecmp((x), "true"))
template <class T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
"""


def _fn(src: str, sig: str, path: Path) -> str:
    m = re.search(r"^" + sig + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(f"{sig} not found in {path}")
    return m.group(0)


def _branch(st: str, key: str, nxt: str) -> str:
    b = re.search(r"\n(  else if \(strcasecmp_P\(field, PSTR\(\"" + key + r"\"\)\) == 0\).*?)\n  else if \(strcasecmp_P\(field, PSTR\(\""
                  + nxt + r"\"\)", st, re.S)
    if not b:
        raise AssertionError(f"{key} branch of updateSetting() not found")
    return b.group(1)


STUBS = r"""
static void satNarrate_P(PGM_P) {}
static void sendMQTTData(const char*, const char*, bool) {}
static char* dtostrf(double v, signed char w, unsigned char p, char* b) { sprintf(b, "%*.*f", w, p, v); return b; }
static void satSyncPresetTopic(SATPreset) {}
static void satClearSafetyTrip() {}
static int g_disable = 0;
static void satDisable(bool = true) { g_disable++; }
void satResetControlState();   // the source declares it before satHandlePreset (defined after satZones there)
"""

MAIN = r"""
static void upd(const char* field, const char* newValue) {
  if (false) {}
@ENABLED@
@TARGET@
}
static void prime() {                         // a PID that has built up an integral, a raw derivative and zone integrals
  _pid_integral = 0.7f; _pid_rawDerivative = 0.02f;
  state.sat.fPidI = 0.7f; state.sat.fRawDerivative = 0.02f; state.sat.fPidD = 0.3f;
  satZones[0].fPidIntegral = 0.3f; satZones[1].fPidIntegral = 0.4f;
}
static void pass(int t) {
  g_ms = g_base + (uint32_t)t * 1000u;
  for (int z = 0; z < 2; z++) { satZones[z].iLastUpdateMs = g_ms; }
  satPidService(g_ms);
}
static void windowSwitch() { satHandleWindow(true); g_ms += 61000; _satCheckWindowTimer(); }
static void snapshot(const char* tag) {
  printf("%s I=%.6f D=%.6f rawD=%.6f shownI=%.6f shownRawD=%.6f z0=%.6f z1=%.6f init=%d startup=%d first=%d lastErr=%.6f seen=%d\n", tag,
         _pid_integral, state.sat.fKd * _pid_rawDerivative, _pid_rawDerivative, state.sat.fPidI, state.sat.fRawDerivative,
         satZones[0].fPidIntegral, satZones[1].fPidIntegral, (int)_pid_initialized, (int)_pid_startupPending,
         (int)_pid_firstOutputPending, _pid_lastError, (int)satPidRoomSeen());
}
int main(int argc, char** argv) {
  const char* sc = argv[1]; g_base = 1000000; g_ms = g_base; satPidReset(); g_trace = 1;
  settings.sat.iZoneCount = 2;
  satZones[0].fRoomTemp = 20.0f; satZones[0].fSetpoint = 21.0f; satZones[0].bRoomValid = satZones[0].bSpValid = true;
  settings.sat.fTargetTemp = 21.0f;
  for (int t = 0; t <= 400; t++) {
    pass(t);
    if (t == 130) {
      prime();
      snapshot("before");
      if      (!strcmp(sc, "none")) {}
      else if (!strcmp(sc, "target"))      upd("SATtargettemp", "23.0");
      else if (!strcmp(sc, "target_same")) upd("SATtargettemp", "21.0");
      else if (!strcmp(sc, "preset"))      satHandlePreset("comfort");        // 21.5 against 21.0
      else if (!strcmp(sc, "preset_same")) { settings.sat.fPresetComfort = 21.0f; satHandlePreset("comfort"); }
      else if (!strcmp(sc, "preset_clear")) {
        satHandlePreset("comfort"); prime(); satHandlePreset("none");          // back to 21.0
      }
      else if (!strcmp(sc, "window_open")) windowSwitch();                    // activity preset 16.0
      else if (!strcmp(sc, "window_close")) { windowSwitch(); prime(); satHandleWindow(false); }
      else if (!strcmp(sc, "enable"))      { settings.sat.bEnabled = false; upd("SATenabled", "1"); }
      else if (!strcmp(sc, "enable_same")) { settings.sat.bEnabled = true;  upd("SATenabled", "1"); }
      else if (!strcmp(sc, "reset_integral")) _satResetIntegralCmd("");
      else if (!strcmp(sc, "rest_pending")) satResetControlState();           // what a REST handler does
      else if (!strcmp(sc, "zone_sp"))     satHandleZoneSetpoint(1, "22.0");
      else if (!strcmp(sc, "zone_sp_same")) satHandleZoneSetpoint(1, "21.0");
      else if (!strcmp(sc, "zone_sp_first")) { satZones[0].bSpValid = false; satHandleZoneSetpoint(1, "22.0"); }
      snapshot("mid");                       // the trigger only raised the flag (REST / async_tcp task): nothing zeroed yet
      satApplyControlStateReset();           // the loop task's next pass
      snapshot("after");
      if (!strcmp(sc, "rest_pending")) { prime(); satApplyControlStateReset(); snapshot("again"); }   // exactly once
      printf("out=%.5f\n", satPidOutput(g_curveNow));
    }
  }
  printf("steps=%d log=", g_gathers);
  for (int i = 0; i < g_stepN; i++) printf("%d,", g_stepLog[i]);
  printf("\n");
  return 0;
}
"""


def build_cpp(sat: str, pid: str, st: str, mq: str) -> str:
    pid = pid[pid.index("// --- PID Constants ---"):]
    pre = cad.PRELUDE
    pre = pre.replace("struct SatSettings { ", "struct SatSettings { float fPresetAway = 15, fPresetEco = 18, fPresetComfort = 21.5f, "
                      "fPresetSleep = 17, fPresetActivity = 16, fPresetHome = 20.5f; bool bWindowDetection = true; "
                      "uint16_t iWindowMinOpenSec = 60; bool bEnabled = true; ", 1)
    pre = pre.replace("struct SatSettings {", EXTRA_TYPES + "struct SatSettings {", 1)
    pre = pre.replace("struct SatState { ", "struct SatState { SATPreset eActivePreset = SAT_PRESET_NONE; float fPreCustomTemp = 0, "
                      "fPreWindowTarget = 0, fPreActivityTemp = 0; uint8_t iPreWindowPreset = 0; bool bWindowOpen = false; "
                      "uint32_t iWindowOpenSinceMs = 0; bool bFallbackSuppressed = false; ", 1)
    pre = pre.replace("struct State { SatState sat; };", "struct MqttS { bool bConnected = false; };\n"
                      "struct State { SatState sat; MqttS mqtt; bool bSetupComplete = true; };", 1)
    assert "bSetupComplete" in pre and "fPresetAway" in pre and "eActivePreset" in pre
    zone = cad.zone_source(sat) + "\n" + _fn(sat, r"bool satHandleZoneSetpoint\(", SAT_INO)
    ctrl = "\n".join([
        _fn(sat, r"static const char\* satGetPresetName\(", SAT_INO),
        _fn(sat, r"void satPresetClearForUserTarget\(", SAT_INO),
        _fn(sat, r"static void satResetControlStateIfTargetChanged\(", SAT_INO),
        re.search(r"^static volatile bool _sat_resetControlStatePending.*$", sat, re.M).group(0),
        _fn(sat, r"void satResetControlState\(\)\n\{", SAT_INO),
        _fn(sat, r"static void satApplyControlStateReset\(\)\n\{", SAT_INO),
        _fn(sat, r"void satHandlePreset\(", SAT_INO), _fn(sat, r"void satHandleWindow\(", SAT_INO),
        _fn(sat, r"static void _satCheckWindowTimer\(", SAT_INO),
        _fn(st, r"static float clampSettingFloat\(", SETTINGS_INO),
        re.search(r"^static void _satResetIntegralCmd\(.*$", mq, re.M).group(0),
    ])
    main = (MAIN.replace("@ENABLED@", _branch(st, "SATenabled", "SATsystem"))
            .replace("@TARGET@", _branch(st, "SATtargettemp", "SATcoefficient")))
    # the zone functions need the control helpers declared first (satZones lives in the prelude here)
    text = (pre.replace("@TIMERS@", cad.TIMERS).replace("@ZONE@", zone).replace("@PID@", pid)
            .replace("@GATHER@", cad.GATHER_STUB).replace("@SAVE@", cad.save_source(sat)))
    return cad.adapt(sat, text + STUBS + ctrl + "\n#define HAVE_SERVICE 1\n" + main)


SAT_INO = cad.SAT_INO


def parse(line):
    return {k: v for k, v in (kv.split("=", 1) for kv in line.split()[0:] if "=" in kv)}


class Resets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = cad.PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        st = SETTINGS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        mq = MQTT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(sat, pid, st, mq), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")
        cls.cache = {}

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        if sc not in self.cache:
            out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout.strip().split("\n")
            lines = {l.split()[0]: parse(l) for l in out if l.startswith(("before", "mid", "after", "again"))}
            lines["out"] = float([l for l in out if l.startswith("out=")][0][4:])
            lines["log"] = out[-1].split("log=")[1]
            self.cache[sc] = lines
        return self.cache[sc]

    def assertTermsCleared(self, sc, zones=True):
        r = self.run_sc(sc)
        b, a = r["before"], r["after"]
        self.assertNotEqual(float(b["I"]), 0.0)                 # the precondition: there was something to reset
        for k in ("I", "rawD", "shownI", "shownRawD"):
            self.assertEqual(float(a[k]), 0.0, f"{sc}: {k} after = {a[k]}")
        if zones:
            self.assertEqual(float(a["z0"]), 0.0, f"{sc}: zone 1 integral {a['z0']}")
            self.assertEqual(float(a["z1"]), 0.0, f"{sc}: zone 2 integral {a['z1']}")
        # AC#2: nothing but the two terms is touched, and the 60 s schedule is the one of a run without the trigger
        for k in ("init", "startup", "first", "lastErr", "seen"):
            self.assertEqual(a[k], b[k], f"{sc}: {k} changed {b[k]} -> {a[k]}")
        self.assertEqual(r["log"], self.run_sc("none")["log"], f"{sc}: PID step schedule changed")

    def assertUntouched(self, sc, keys=("I", "rawD", "z0", "z1")):
        r = self.run_sc(sc)
        for k in keys:
            self.assertEqual(r["after"][k], r["before"][k], f"{sc}: {k} {r['before'][k]} -> {r['after'][k]}")

    def test_a_preconditions(self):
        r = self.run_sc("none")
        self.assertNotEqual(float(r["before"]["I"]), 0.0)
        self.assertEqual(r["before"]["init"], "1")             # the PID was initialised and running at the trigger
        self.assertEqual(r["before"]["startup"], "0")
        self.assertEqual(r["log"].split(",")[:3], ["0", "60", "120"])   # the 60 s schedule

    def test_target_change(self):
        self.assertTermsCleared("target")

    def test_target_unchanged_does_not_reset(self):
        self.assertUntouched("target_same")

    def test_preset_change(self):
        self.assertTermsCleared("preset")

    def test_preset_with_same_target_does_not_reset(self):
        self.assertUntouched("preset_same")     # climate.py:643-644: the target did not change

    def test_preset_clear(self):
        self.assertTermsCleared("preset_clear")

    def test_window_open_switch(self):
        self.assertTermsCleared("window_open")

    def test_window_close_restore(self):
        self.assertTermsCleared("window_close")

    def test_hvac_on(self):
        self.assertTermsCleared("enable")

    def test_hvac_already_on_does_not_reset(self):
        self.assertUntouched("enable_same")     # another client or a retained MQTT message can repeat SATenabled=1

    def test_reset_integral_command(self):
        self.assertTermsCleared("reset_integral")

    def test_zone_setpoint_change_resets_that_zone_only(self):
        r = self.run_sc("zone_sp")
        self.assertEqual(float(r["after"]["z0"]), 0.0)
        self.assertEqual(r["after"]["z1"], r["before"]["z1"])
        self.assertEqual(r["after"]["I"], r["before"]["I"])     # the main PID is not touched by a room setpoint

    def test_zone_setpoint_same_or_first_does_not_reset(self):
        self.assertUntouched("zone_sp_same")
        self.assertUntouched("zone_sp_first")

    def test_first_tick_after_reset_is_a_computed_output(self):
        """TASK-1270: after a reset the very next output is computed (curve + P + I), never the uninitialised 0."""
        if not self.exe:
            self.skipTest("no host C++ compiler")
        self.assertTermsCleared("target")   # init/startup/first/lastErr unchanged and the schedule intact
        out = self.run_sc("target")["out"]
        self.assertGreater(out, 40.0)   # curve 40 + P (room below target) + I 0 + D 0: not the stored 0, not the 10 floor

    def test_rest_request_applied_once_on_next_loop_pass(self):
        """A reset requested from another task only raises a flag; the loop task zeroes the terms once, nothing else."""
        r = self.run_sc("rest_pending")
        self.assertEqual(r["mid"]["I"], r["before"]["I"], "the request itself must not write the PID terms")
        self.assertEqual(r["mid"]["z0"], r["before"]["z0"])
        self.assertTermsCleared("rest_pending")        # applied on the pass: terms 0; timers, flags, last error, schedule kept
        self.assertEqual(r["again"]["I"], r["before"]["I"], "a second pass without a new request must not reset again")
        self.assertEqual(r["again"]["z1"], r["before"]["z1"])

    def test_structural_disable_and_rest_paths(self):
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = _fn(sat, r"void satDisable\(", SAT_INO)
        self.assertRegex(body, r"satResetControlState\(\);[^\n]*\n\s*satPidReset\(\);", "satDisable resets the zone integrals too (structural)")
        run = _fn(sat, r"static void satControlLoopRun\(\)\n\{", SAT_INO)
        self.assertEqual(run.split("\n")[2].strip().split(";")[0], "satApplyControlStateReset()", "the loop pass applies the request first (structural)")
        rest = REST_INO.read_text(encoding="utf-8")
        self.assertNotIn("satResetIntegral(", rest)
        self.assertEqual(rest.count("satResetControlState();"), 2, "both REST reset_integral paths call the control-state reset (structural)")


if __name__ == "__main__":
    unittest.main()
