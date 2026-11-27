#!/usr/bin/env python3
"""TASK-1259: host test for "SAT target follows the thermostat setpoint".

The code under test is NOT re-implemented here. The test cuts the real functions
satFollowThermostatDecide() (edge, debounce, synced and echo test), satAdoptThermostatSetpoint() (target change,
preset clear) and satOnThermostatSetpoint() (hook called by processOT() for a thermostat
MsgID 16 Write-Data) out of src/OTGW-firmware/SATcontrol.ino, wraps them in a stub header and
compiles them with a host C++ compiler (g++, clang++ or MSVC cl via vswhere). Without a
compiler the tests are skipped, they do not pass silently.

OTGW_SAT_INO points at another copy, e.g. the output of
`git show HEAD:src/OTGW-firmware/SATcontrol.ino`, for an old-vs-fix run. HEAD has none of the
three functions; the harness then supplies a no-op satOnThermostatSetpoint(), so every case that
expects an adoption fails and the "ignore" cases pass trivially (nothing ever happens on HEAD).

Not reachable from here: the call in processOT() (OTGW-Core.ino) and the readSettings() default
rule. Both are covered by source-level checks at the end of this file plus the bench run.

Run: python tests/test_sat_follow_thermostat.py
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
SRC = REPO / "src" / "OTGW-firmware"

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
// TASK-1521 log helpers
static void satNarrate_P(const char*) {}
static void satNarratef_P(const char*, ...) {}
static bool _sat_followNarrPending = false;
static bool _sat_narrAtMax = false;
static bool _sat_narrCold = false;
static bool _sat_bootBoilerLogged = false;
static bool _sat_bootOutsideLogged = false;
enum SATPreset : uint8_t { SAT_PRESET_NONE = 0, SAT_PRESET_AWAY, SAT_PRESET_ECO, SAT_PRESET_COMFORT, SAT_PRESET_SLEEP,
                           SAT_PRESET_ACTIVITY, SAT_PRESET_HOME };   // the order of SATtypes.h
static const float SAT_TARGET_CHANGE_MIN_C = 0.0495f;               // SATtypes.h
// Preset values kept away from every thermostat value the edge tests use (20.0, 21.0, 21.5, 22.0, 23.0, 23.5 ...).
struct SatSettings { bool bEnabled=true, bFollowThermostat=true, bPushSetpoint=false; float fTargetTemp=21.0f;
                     float fPresetComfort=22.5f, fPresetEco=18.5f, fPresetAway=15.5f, fPresetSleep=17.5f, fPresetActivity=14.5f, fPresetHome=24.5f; };
struct Settings { SatSettings sat; };
struct MqttState { bool bConnected=false; };
struct SatState { float fLastSentTC=0.0f, fThermostatTrSet=0.0f, fThermostatTrSetCand=0.0f, fPreCustomTemp=0.0f; uint32_t iTCPushMs=0; bool bPushRefused=false, bTCAccountedValid=false; bool bThermostatTrSetValid=false, bThermostatTrSetCandValid=false;
                  SATPreset eActivePreset=SAT_PRESET_NONE;
                  float fPreWindowTarget=0.0f, fPreActivityTemp=0.0f; uint32_t iWindowOpenSinceMs=0; uint8_t iPreWindowPreset=0; };
struct State { SatState sat; MqttState mqtt; };
static Settings settings; static State state;
static uint32_t millis() { return 0; }
static bool otOverrideOnThermostatSetpoint(float) { return false; }   // TASK-3033: called first in satOnThermostatSetpoint()
static bool satIsEnabled() { return settings.sat.bEnabled; }   // TASK-1345: the real one adds the MQTT-loss takeover flag
static int g_syncCalls = 0, g_adopt = 0, g_ctrlReset = 0, g_rt = -1;
#define snprintf_P snprintf
static void dtostrf(float v, int, int prec, char* out) { snprintf(out, 12, "%.*f", prec, v); }
#ifdef _MSC_VER
#define strcasecmp_P _stricmp
#else
#include <strings.h>
#define strcasecmp_P strcasecmp
#endif
static void sendMQTTData(const char*, const char*, bool) { }
static void satResetControlStateIfTargetChanged(float) { g_ctrlReset++; }
void satPresetClearForUserTarget();
// settingStuff.ino "SATtargettemp": a target that really changes clears an active preset and a window switch.
static void updateSetting(const char* field, const char* value) {
  if (!strcmp(field, "SATtargettemp")) {
    const float before = settings.sat.fTargetTemp;
    float v = (float)atof(value); settings.sat.fTargetTemp = v < 5 ? 5 : (v > 30 ? 30 : v); g_adopt++;
    if (fabsf(settings.sat.fTargetTemp - before) >= SAT_TARGET_CHANGE_MIN_C) satPresetClearForUserTarget();
  }
}
static void satSyncPresetTopic(SATPreset) { g_syncCalls++; }
static void satTCNoteAdopted(float) { }
"""

MAIN = r"""
// usage: t.exe enabled follow push lastTC preset(0|1) precustom target  v1 v2 ...   tokens: follow0 follow1 sat0 sat1
int main(int argc, char** argv) {
  if (argc < 8) return 2;
  settings.sat.bEnabled = atoi(argv[1]) != 0;
  settings.sat.bFollowThermostat = atoi(argv[2]) != 0;
  settings.sat.bPushSetpoint = atoi(argv[3]) != 0;
  state.sat.fLastSentTC = (float)atof(argv[4]);
  state.sat.eActivePreset = (SATPreset)atoi(argv[5]);   // 0 none, 1 away, 2 eco, 3 comfort, 4 sleep, 5 activity, 6 home
  state.sat.fPreCustomTemp = (float)atof(argv[6]);
  settings.sat.fTargetTemp = (float)atof(argv[7]);
  for (int i = 8; i < argc; i++) {
    if (!strcmp(argv[i], "follow0")) { settings.sat.bFollowThermostat = false; continue; }
    if (!strcmp(argv[i], "follow1")) { settings.sat.bFollowThermostat = true; continue; }
    if (!strcmp(argv[i], "sat0")) { settings.sat.bEnabled = false; continue; }
    if (!strcmp(argv[i], "sat1")) { settings.sat.bEnabled = true; continue; }
    if (!strncmp(argv[i], "tc=", 3)) { state.sat.fLastSentTC = (float)atof(argv[i] + 3); continue; }
    if (!strncmp(argv[i], "pw=", 3)) { state.sat.fPreWindowTarget = (float)atof(argv[i] + 3); state.sat.fPreActivityTemp = state.sat.fPreWindowTarget; state.sat.iWindowOpenSinceMs = 1; continue; }   // window switch holds the target
    if (!strncmp(argv[i], "sleep=", 6)) { settings.sat.fPresetSleep = (float)atof(argv[i] + 6); continue; }
    if (!strncmp(argv[i], "mqtt=", 5)) { satHandleTargetTemp(argv[i] + 5); continue; }   // the MQTT/web path
    if (!strcmp(argv[i], "off")) { satHandlePreset("none"); continue; }                  // "preset off"
    if (!strcmp(argv[i], "roundtrip")) {   // a reboot: write the preset file, forget the state, read the file back (entry 308)
      char b[96]; satPresetStateFormat(b, sizeof(b));
      settings.sat.fTargetTemp = 21.0f; state.sat.eActivePreset = SAT_PRESET_NONE; state.sat.fPreCustomTemp = 0.0f;
      g_rt = satPresetStateApply(b) ? 1 : 0; continue;
    }
    satOnThermostatSetpoint((float)atof(argv[i]));
  }
  printf("target=%.1f preset=%d pre=%.1f adopted=%d sync=%d valid=%d pw=%.1f since=%u rt=%d user=%.1f\n", settings.sat.fTargetTemp,
         (int)state.sat.eActivePreset, state.sat.fPreCustomTemp, g_adopt, g_syncCalls, (int)state.sat.bThermostatTrSetValid,
         state.sat.fPreWindowTarget, (unsigned)state.sat.iWindowOpenSinceMs, g_rt, satUserTargetTemp());
  return 0;
}
"""

FUNCS = (
    ("satGetPresetName", r"^static const char\* satGetPresetName\(.*?^\}\s*$"),
    ("satUserTargetTemp", r"^float satUserTargetTemp\(\).*?^\}\s*$"),
    ("satPresetStateFormat", r"^static size_t satPresetStateFormat\(.*?^\}\s*$"),
    ("satPresetTempOrZero", r"^static float satPresetTempOrZero\([^\n]*$"),
    ("satPresetStateApply", r"^static bool satPresetStateApply\(.*?^\}\s*$"),
    ("satPresetClearForUserTarget", r"^void satPresetClearForUserTarget\(\).*?^\}\s*$"),
    ("satPresetForValue", r"^static SATPreset satPresetForValue\(.*?^\}\s*$"),   # TASK-3032 (HEAD: not there yet)
    ("satHandlePreset", r"^void satHandlePreset\(.*?^\}\s*$"),
    ("satHandleTargetTemp", r"^bool satHandleTargetTemp\(.*?^\}\s*$"),
    ("satPushRefusedDecide", r"^static bool satPushRefusedDecide\(.*?^\}\s*$"),
    ("satFollowThermostatDecide", r"^static bool satFollowThermostatDecide\(.*?^\}\s*$"),
    ("satAdoptThermostatSetpoint", r"^static void satAdoptThermostatSetpoint\(.*?^\}\s*$"),
    ("satOnThermostatSetpoint", r"^void satOnThermostatSetpoint\(.*?^\}\s*$"),
)


def extract(src: str) -> str:
    out = []
    for name, rx in FUNCS:
        m = re.search(rx, src, re.S | re.M)
        if m:
            out.append(m.group(0))
        elif name == "satPushRefusedDecide":
            continue   # HEAD: not there yet
        elif name == "satOnThermostatSetpoint":
            out.append("void satOnThermostatSetpoint(float) { }   // HEAD: no follow-thermostat code")
    return "\n\n".join(out)


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


def build(workdir: Path):
    body = extract(SAT_INO.read_text(encoding="utf-8"))
    cpp = workdir / "t.cpp"
    cpp.write_text(STUBS + body + MAIN, encoding="utf-8")
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


class SatFollowThermostat(unittest.TestCase):
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

    def run_sc(self, *args, enabled=1, follow=1, push=0, last_tc=0.0, preset=0, pre=0.0, target=21.0):
        argv = [str(self.exe), *map(str, (enabled, follow, push, last_tc, preset, pre, target, *args))]
        out = subprocess.run(argv, capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    # --- edge detection (push off: every confirmed edge) ----------------------------------
    def test_first_value_only_records(self):
        r = self.run_sc(22.0)
        self.assertEqual((r["target"], r["adopted"], r["valid"]), ("21.0", "0", "1"))

    def test_change_needs_two_consecutive_frames(self):
        self.assertEqual(self.run_sc(20.0, 21.5)["adopted"], "0")
        r = self.run_sc(20.0, 21.5, 21.5)
        self.assertEqual((r["target"], r["adopted"]), ("21.5", "1"))

    def test_exactly_one_tenth_is_adopted(self):
        r = self.run_sc(20.0, 20.1, 20.1)
        self.assertEqual((r["target"], r["adopted"]), ("20.1", "1"))

    def test_change_below_one_tenth_is_ignored(self):
        r = self.run_sc(20.0, 20.05, 20.05)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "0"))

    def test_repeated_same_value_adopts_once(self):
        r = self.run_sc(20.0, 21.5, 21.5, 21.5, 21.5)
        self.assertEqual((r["target"], r["adopted"]), ("21.5", "1"))

    def test_slow_drift_below_threshold_never_adds_up(self):
        r = self.run_sc(20.0, 20.05, 20.08, 20.04)
        self.assertEqual(r["adopted"], "0")

    def test_two_changes_in_a_row_both_adopted_no_minimum_time(self):
        r = self.run_sc(20.0, 21.0, 21.0, 22.0, 22.0)
        self.assertEqual((r["target"], r["adopted"]), ("22.0", "2"))

    def test_out_of_range_values_are_not_a_setpoint(self):
        r = self.run_sc(20.0, 0.0, 35.0, 4.0, 35.0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "0"))

    def test_scenario6_push_off_20_then_21_twice_adopted(self):
        r = self.run_sc(20.0, 21.0, 21.0, push=0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "1"))

    # --- debounce -----------------------------------------------------------------------
    def test_scenario5_single_frame_flicker_adopts_nothing(self):
        for push, tc in ((0, 0.0), (1, 21.0)):
            with self.subTest(push=push):
                r = self.run_sc(21.0, 19.0, 21.0, push=push, last_tc=tc, target=21.0)
                self.assertEqual(r["adopted"], "0")

    def test_different_value_in_between_restarts_the_candidate(self):
        r = self.run_sc(20.0, 21.0, 22.0, 21.0, push=0)
        self.assertEqual(r["adopted"], "0")
        r = self.run_sc(20.0, 21.0, 22.0, 22.0, push=0)
        self.assertEqual((r["target"], r["adopted"]), ("22.0", "1"))

    def test_candidate_frames_within_a_twentieth_confirm(self):
        r = self.run_sc(20.0, 21.0, 21.04, push=0)
        self.assertEqual(r["adopted"], "1")

    # --- preset ---------------------------------------------------------------------
    def test_adoption_clears_preset_without_restoring_pre_custom_temp(self):
        r = self.run_sc(20.0, 23.0, 23.0, preset=1, pre=19.0, target=16.0)
        self.assertEqual((r["target"], r["preset"], r["pre"], r["sync"]), ("23.0", "0", "0.0", "1"))

    def test_no_preset_sync_when_no_preset_was_active(self):
        r = self.run_sc(20.0, 23.0, 23.0, preset=0)
        self.assertEqual(r["sync"], "0")

    def test_ignored_change_keeps_the_preset(self):
        r = self.run_sc(20.0, 20.05, 20.05, preset=1, pre=19.0, target=16.0)
        self.assertEqual((r["target"], r["preset"], r["pre"]), ("16.0", "1", "19.0"))

    # --- TASK-3032, entry 465: a thermostat change that equals a preset value selects that preset ------
    # Test presets: comfort 22.5, eco 18.5, away 15.5, sleep 17.5, activity 14.5, home 24.5 (enum: away 1, eco 2, comfort 3,
    # sleep 4, activity 5, home 6).
    def test_ac1_thermostat_change_to_a_preset_value_selects_that_preset(self):
        r = self.run_sc(20.0, 22.5, 22.5, preset=0, target=21.0)
        self.assertEqual((r["target"], r["preset"], r["pre"]), ("22.5", "3", "0.0"))
        r = self.run_sc(20.0, 18.5, 18.5, preset=0, target=21.0)
        self.assertEqual((r["target"], r["preset"]), ("18.5", "2"))

    def test_ac1_switching_from_one_preset_to_another_publishes_the_final_state_once(self):
        r = self.run_sc(20.0, 22.5, 22.5, preset=1, pre=19.0, target=15.5)
        self.assertEqual((r["preset"], r["pre"], r["sync"]), ("3", "0.0", "1"))   # one publish, no NONE-then-preset flap

    def test_ac1_a_matched_preset_without_a_previous_preset_is_published_once(self):
        self.assertEqual(self.run_sc(20.0, 22.5, 22.5, preset=0)["sync"], "1")

    def test_ac2_non_preset_value_still_clears_the_preset(self):
        r = self.run_sc(20.0, 23.0, 23.0, preset=3, pre=19.0, target=22.5)
        self.assertEqual((r["target"], r["preset"], r["pre"], r["sync"]), ("23.0", "0", "0.0", "1"))

    def test_ac3_preset_off_after_a_thermostat_match_keeps_the_thermostat_value(self):
        r = self.run_sc(20.0, 22.5, 22.5, "off", preset=2, pre=19.0, target=18.5)   # was eco over a pre-preset 19.0
        self.assertEqual((r["target"], r["preset"], r["pre"]), ("22.5", "0", "0.0"))

    def test_ac3_old_pre_preset_value_is_not_restored_after_the_match(self):
        r = self.run_sc(20.0, 18.5, 18.5, "off", preset=3, pre=21.0, target=22.5)   # comfort over 21.0, thermostat to eco
        self.assertEqual(r["target"], "18.5")

    def test_tolerance_is_strictly_below_a_twentieth_of_a_degree(self):
        r = self.run_sc(20.0, 22.54, 22.54)
        self.assertEqual(r["preset"], "3")                      # 0.04 off: matches (same rule as satHandleTargetTemp)
        r = self.run_sc(20.0, 22.56, 22.56)
        self.assertEqual((r["preset"], r["target"]), ("0", "22.6"))   # 0.06 off: no match, plain adoption

    def test_equal_valued_presets_pick_the_first_in_table_order_like_the_mqtt_path(self):
        r = self.run_sc("sleep=18.5", 20.0, 18.5, 18.5)         # sleep == eco: table order comfort, eco, away, sleep ...
        self.assertEqual(r["preset"], "2")
        r = self.run_sc("sleep=18.5", "mqtt=18.5")
        self.assertEqual(r["preset"], "2")                      # the MQTT/web path picks the same one

    def test_mqtt_and_thermostat_paths_agree_for_every_preset_value(self):
        for v, want in (("22.5", "3"), ("18.5", "2"), ("15.5", "1"), ("17.5", "4"), ("14.5", "5"), ("24.5", "6")):
            with self.subTest(v=v):
                self.assertEqual(self.run_sc("mqtt=" + v)["preset"], want)
                self.assertEqual(self.run_sc(20.0, float(v), float(v))["preset"], want)

    def test_window_switch_ended_by_a_thermostat_match_to_another_preset(self):
        # activity held by the window switch (pre-window target 16.0), thermostat dialled to eco: eco stays, nothing is restored on close
        r = self.run_sc("pw=16.0", 20.0, 18.5, 18.5, preset=5, target=14.5)
        self.assertEqual((r["preset"], r["target"], r["pw"], r["since"]), ("2", "18.5", "0.0", "0"))

    def test_window_switch_ended_even_when_the_value_equals_the_current_target(self):
        r = self.run_sc("pw=16.0", 20.0, 18.5, 18.5, preset=5, target=18.5)   # no real target change: updateSetting() clears nothing
        self.assertEqual((r["preset"], r["pw"], r["since"]), ("2", "0.0", "0"))

    def test_window_switch_kept_when_the_thermostat_value_is_the_activity_preset(self):
        r = self.run_sc("pw=16.0", 20.0, 14.5, 14.5, preset=5, target=14.5)
        self.assertEqual((r["preset"], r["pw"]), ("5", "16.0"))

    def test_matched_preset_survives_a_reboot_with_the_thermostat_value_as_user_target(self):
        r = self.run_sc(20.0, 22.5, 22.5, "roundtrip")
        self.assertEqual((r["rt"], r["preset"], r["target"], r["user"]), ("1", "3", "22.5", "22.5"))

    # --- push on: synced rule and echo guard -----------------------------------------------
    def test_scenario1_boot_before_first_push_adopts_nothing(self):
        r = self.run_sc(21.0, 19.0, 19.0, push=1, last_tc=0.0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "0"))

    def test_scenario2_synced_thermostat_user_change_twice_adopted(self):
        r = self.run_sc(21.0, 22.0, 22.0, push=1, last_tc=21.0, target=21.0)
        self.assertEqual((r["target"], r["adopted"]), ("22.0", "1"))

    def test_scenario3_late_reply_after_newer_push_not_adopted(self):
        # thermostat was synced to 21, SAT pushed 22 meanwhile: the thermostat still shows 21, then 22 (echo)
        r = self.run_sc(21.0, "tc=22.0", 21.0, 22.0, 22.0, push=1, last_tc=21.0, target=22.0)
        self.assertEqual((r["target"], r["adopted"]), ("22.0", "0"))

    def test_scenario3b_previous_value_not_synced_is_adopted_since_task_1298(self):
        # TASK-1298: the thermostat never mirrored the push (prev 22.0 vs TC 23.0); a real dial change
        # to 21.0 is neither echo of 23.0 nor of the push before it (22.0 is 1.0 away), so it is adopted.
        r = self.run_sc(22.0, "tc=23.0", 21.0, 21.0, push=1, last_tc=22.0, target=23.0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "1"))

    def test_gruijter_log_honeywell_round_never_mirrors_the_override(self):
        # mqtt.log 2026-10-22 (TASK-1298): TC=20.5 pushed; dial 15 -> 19.5 -> 20.0 -> 19.5, each seen twice;
        # the Round keeps reporting its own setpoint and never the override. The control loop pushes every
        # adopted value as the next TC= (SATcontrol.ino satControlLoop), so the tc= tokens model that.
        # HEAD adopts nothing; the review of the first fix showed a previous-push echo guard swallowing
        # the final 19.5, so the pushes must be in the replay.
        r = self.run_sc(15.0, 15.0, "tc=20.5", 19.5, 19.5, "tc=19.5", 20.0, 20.0, "tc=20.0", 19.5, 19.5,
                        push=1, last_tc=0.0, target=20.5)
        self.assertEqual((r["target"], r["adopted"]), ("19.5", "3"))
        # then the user sets 20.0 in the SAT GUI (TC=20.0) and the dial shows 20.0: that is an echo
        r = self.run_sc(19.5, 19.5, "tc=20.0", 20.0, 20.0, push=1, last_tc=19.5, target=20.0)
        self.assertEqual((r["target"], r["adopted"]), ("20.0", "0"))

    def test_dial_back_to_the_previous_value_is_adopted(self):
        # A -> B -> A on any thermostat, with the loop's push after each adoption: the return to A is a
        # real change and must not be mistaken for an echo of the push before the last one.
        r = self.run_sc(21.0, 21.0, "tc=21.0", 22.0, 22.0, "tc=22.0", 21.0, 21.0, push=1, last_tc=0.0, target=21.0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "2"))

    def test_mirroring_thermostat_late_reply_is_no_edge(self):
        # Remeha iSense style: the thermostat mirrored TC=22 (echo, recorded). SAT then pushes 23; the
        # thermostat still reports 22 for a moment, which is the recorded value and no edge, then mirrors 23
        # (echo). Nothing is adopted and the target stays 23.
        r = self.run_sc(21.0, 21.0, "tc=22.0", 22.0, 22.0, "tc=23.0", 22.0, 22.0, 23.0, 23.0, push=1, last_tc=21.0, target=23.0)
        self.assertEqual((r["target"], r["adopted"]), ("23.0", "0"))

    def test_scenario4_thermostat_rounds_tc_to_half_degree(self):
        # SAT pushed 20.3, the thermostat shows 20.5 (echo), then the user sets 21.0
        r = self.run_sc(20.0, "tc=20.3", 20.5, 20.5, 21.0, 21.0, push=1, last_tc=20.0, target=20.3)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "1"))

    def test_rounded_echo_alone_is_not_adopted(self):
        r = self.run_sc(20.0, "tc=20.3", 20.5, 20.5, push=1, last_tc=20.0, target=20.3)
        self.assertEqual(r["adopted"], "0")

    def test_echo_within_a_quarter_degree_not_adopted_exact_echo_too(self):
        for echo in (22.0, 22.2):
            with self.subTest(echo=echo):
                r = self.run_sc(21.0, "tc=22.0", echo, echo, push=1, last_tc=21.0, target=22.0)
                self.assertEqual(r["adopted"], "0")

    def test_same_value_without_push_is_not_an_echo(self):
        r = self.run_sc(21.0, 22.0, 22.0, push=0, last_tc=22.0)
        self.assertEqual((r["target"], r["adopted"]), ("22.0", "1"))

    def test_echo_is_recorded_so_the_next_real_change_is_an_edge(self):
        r = self.run_sc(21.0, 22.0, 22.0, 21.0, 21.0, push=1, last_tc=22.0, target=22.0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "1"))   # 22.0 echo recorded, prev synced, user 21.0

    # --- option and SAT state -------------------------------------------------------
    def test_option_off_ignores(self):
        r = self.run_sc(20.0, 21.5, 21.5, follow=0)
        self.assertEqual((r["target"], r["adopted"], r["valid"]), ("21.0", "0", "0"))

    def test_sat_disabled_ignores(self):
        r = self.run_sc(20.0, 21.5, 21.5, enabled=0)
        self.assertEqual((r["target"], r["adopted"], r["valid"]), ("21.0", "0", "0"))

    def test_first_value_after_enabling_the_option_only_records(self):
        r = self.run_sc(20.0, "follow0", 21.5, "follow1", 23.0, 23.5, 23.5)
        self.assertEqual((r["target"], r["adopted"]), ("23.5", "1"))   # 21.5 ignored, 23.0 recorded, 23.5 twice adopted

    def test_first_value_after_enabling_sat_only_records(self):
        r = self.run_sc(20.0, "sat0", 21.5, "sat1", 23.0)
        self.assertEqual((r["target"], r["adopted"]), ("21.0", "0"))

    def test_disabling_drops_a_pending_candidate(self):
        r = self.run_sc(20.0, 21.5, "follow0", 21.5, "follow1", 21.5, 21.5)
        self.assertEqual(r["adopted"], "0")


class SourceWiring(unittest.TestCase):
    """Source-level checks for what the host harness cannot reach."""

    def test_processot_feeds_the_thermostat_t_frame(self):
        core = (SRC / "OTGW-Core.ino").read_text(encoding="utf-8")
        m = re.search(r"if \(OTdata\.id == OT_TrSet[^{]*\{\s*satOnThermostatSetpoint\(", core, re.S)
        self.assertTrue(m, "processOT() must call satOnThermostatSetpoint() for the thermostat MsgID 16 Write-Data")
        self.assertIn("OTGW_THERMOSTAT", m.group(0))
        self.assertIn("OT_WRITE_DATA", m.group(0))

    def test_readsettings_defaults_follow_to_push_when_key_absent(self):
        s = (SRC / "settingStuff.ino").read_text(encoding="utf-8")
        self.assertRegex(s, r"if \(!g_sawSatFollowKey\)\s*\{\s*settings\.sat\.bFollowThermostat = settings\.sat\.bPushSetpoint;")
        # the default is applied after the whole file is read (after the parse loop's file.close())
        self.assertLess(s.index("file.close();\n" if "file.close();\n" in s else "file.close();"),
                        s.index("if (!g_sawSatFollowKey)"))
        self.assertIn('F("SATfollowthermostat")', s)

    def test_setting_is_wired_everywhere(self):
        for rel, needle in (("restAPI.ino", 'F("satfollowthermostat")'), ("restAPI.ino", '"satfollowthermostat"'),
                            ("restAPI.ino", 'PSTR("follow_thermostat")'), ("MQTTstuff.ino", '"follow_thermostat"'),
                            ("MQTTHaDiscovery.cpp", "/sat/follow_thermostat_enable"),
                            ("SATcontrol.ino", 'F("sat/follow_thermostat_enable")'),
                            ("SATcontrol.ino", 'F("follow_thermostat")'),
                            ("data/v2.js", "satfollowthermostat")):
            with self.subTest(rel + " " + needle):
                self.assertIn(needle, (SRC / rel).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
