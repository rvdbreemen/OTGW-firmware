#!/usr/bin/env python3
"""TASK-1482 / TASK-1390: host test for the SAT restart and the outside-temperature rule.

Sergeant D entries 121, 126, 129 (restart), 56, 57 (outside temperature), with the hand-back of entry 47 and the
resume of entry 79 (docs/sat/sergeant-d-corrections.md).

Nothing is re-implemented. The test cuts the real code out of SATcontrol.ino and compiles it with a host C++ compiler:
  - the outside-temperature resolver (fix: "// --- satOutside begin" .. "end"; HEAD: satGetOutsideTemp());
  - the PID state persistence, "//=== PID State Persistence" up to "//=== Energy State Persistence" (satSavePidState,
    satLoadPidState and, on the fix, the PWM-flag file);
  - the SAT command senders, satDisable() and the OpenTherm-link block, as in test_sat_ot_loss.py;
  - fix only: the "// --- satRestart begin" .. "end" block (satRestartStep);
  - the part of satControlLoopRun() from "If safety tripped" up to "if (!DUE(timerSATControl)) return;";
  - for the PWM flag: the enabled branch of initSAT() that sets the control mode.
LittleFS is a thin wrapper over real files in a temp directory, so "reboot" = a second process on the same files.
The harness runs one pass per simulated second. When the control timer is due it stands in for the control tick
by sending CS = 40 - outside through the real satSendCS/MM/CH, so a tick computed with the 0.0 fallback
shows as CS=40.0 (the gruijter capture of TASK-1474: CS=34.1 with outside 0.0).

On HEAD (OTGW_SAT_INO=<copy of origin/dev SATcontrol.ino>) the boot sends CS=0, the first tick sends CS=40.0 with
no outside temperature, an hour-old weather value keeps being used and the PWM flag does not survive: the cases
fail on behaviour, not on a missing symbol.

Not executed here: initSAT() as a whole (BLE, PM chain, energy files), processOT() and the real control tick. The
MsgID 27 stamp in OTGW-Core.ino is checked at source level (SourceGuards).

Run: python tests/test_sat_restart_quiet.py
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

SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
typedef long long time_t_;
#define time_t time_t_
#define F(x) x
#define PSTR(x) x
#define PROGMEM
#define FPSTR(x) (x)
#define snprintf_P snprintf
#define strncpy_P strncpy
#define SATDebugTf(...) ((void)0)
#define SATDebugTln(x) ((void)0)
#define DebugTf(...) ((void)0)
enum SATControlMode { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
struct Weather { bool bValid = false; uint32_t iLastUpdateMs = 0; float fTemperature = 0.0f; };
struct SatSettings { bool bSimulation = false; bool bEnabled = true; bool bPushSetpoint = false; float fTargetTemp = 15.0f; };
struct Settings { SatSettings sat; };
struct SatState { bool bLastSentValid=false, bTCAccountedValid=false, bActive=false, bSafetyTripped=false, bFallbackActive=false,
                  bPushRefused=false, bRoomTempLost=false, bOtLinkLost=false, bOutsideTempMissing=false;
                  uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0, iTCPushMs=0;
                  float fLastSentCS=0, fFinalSetpoint=0; uint8_t iLastSentMM=0, iLastSentCH=0; int eControlMode=0; uint32_t iSimDhwExpiryMs=0;
                  float fSimOutdoorTemp = 0, fExternalOutdoor = 0; bool bExternalOutdoorValid = false; uint32_t iExternalOutdoorLastMs = 0;
                  bool bRestoredOutdoorValid = false; float fRestoredOutdoor = 0; uint32_t iRestoredOutdoorMs = 0;
                  float fPidD = 0, fError = 0, fLastSentTC = 0; Weather weather; };
struct OTSys { uint8_t SlaveStatus = 0; float Toutside = 0.0f; }; static OTSys OTcurrentSystemState;
struct OtBusState { uint32_t iBoilerLastSeenMs = 0; bool bPSmode = false; uint32_t iToutsideBoilerMs = 0; };
struct State { SatState sat; OtBusState otBus; };
static Settings settings; static State state;
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static long long g_now = 1800000000LL;   // wall clock (s); 0 = NTP not set
static long long fake_time() { return g_now; }
#define time(x) fake_time()
static bool isNTPtimeSet() { return g_now != 0; }
static void DebugTln(const char*) {}
static void satNarrate_P(const char*) {}
static void addCommandToQueue(const char* cmd, int, bool, int) { printf("%u %s\n", (unsigned)(g_ms / 1000), cmd); }
static bool isPICEnabled() { return true; }
static bool hasOTCommandInterface() { return true; }
static bool _sat_bootCS0sent = false;   // HEAD: the deferred boot CS=0 in the span stands in for initSAT()'s CS=0
static bool _sat_prevFlameState = false;
static bool satIsFlameOn() { return false; }
static void satCycleOnFlameChange(bool) {}
static void satPwmNoteDhwFlameEdge(bool, bool) {}
static void satCycleSample() {}
static void satGetWindow4hStats() {}
static void satGetWindow24hStats() {}
static void satPwmSuppressionRefresh() {}
static void satMaxTSetSendStep() {}   // TASK-1373: the user MaxTSet SH= step is tested in test_sat_max_tset.py
static void satPidService(uint32_t) {}
static void satDhwSetpointSync() {}   // TASK-1376: SW= is covered by test_sat_dhw_sw_sync.py
static void satMiStep() {}   // TASK-1371: MI=500 is tested in test_sat_no_return_ignition.py
static void satNoReturnFloodOnFlameEdge(bool, bool) {}
static void satNoReturnFloodStep() {}
static void satSetControlMode(SATControlMode m) { state.sat.eControlMode = m; }
static void satPwmReset() {}
static void satCycleSkipAutoSwitch() {}
static float _sat_lastControlCS = 0, _sat_requestedSetpoint = 0;
static bool _sat_thermalSafeActive = false, _sat_roomLostActive = false;
static bool g_roomSeen = false;
static bool satPidRoomSeen() { return g_roomSeen; }
static void satPidReset() { g_roomSeen = false; }
static void satResetControlState() {}   // TASK-1414: zone integrals, tested in test_sat_pid_resets.py
static int g_pidRestored = 0;
static void satPidRestoreState(float, float) { g_pidRestored = 1; }
static void satPidGetPersistState(float* i, float* d, float* rd, float* e) { *i = 1.5f; *d = 0; *rd = 0; *e = 0.2f; }
static void satSaveEnergyState() {}
static void satSaveEstimatedEnergy() {} static void satHCRReset() {}
// LittleFS over real files in the working directory (a "reboot" is a second process on the same files)
struct File {
  FILE* fp = nullptr;
  operator bool() const { return fp != nullptr; }
  void print(const char* s) { fputs(s, fp); }
  size_t readBytes(char* b, size_t n) { return fread(b, 1, n, fp); }
  void close() { if (fp) fclose(fp); fp = nullptr; }
};
struct FSStub {
  File open(const char* p, const char* mode) { File f; f.fp = fopen(p[0] == '/' ? p + 1 : p, mode[0] == 'w' ? "wb" : "rb"); return f; }
};
static FSStub LittleFS;
static uint32_t timerSATControl_due = 31000, timerSATControl_interval = 30000;
static uint32_t timerSAT4hStats_due = 0, timerSAT4hStats_interval = 60000;
static uint32_t timerSATPublishRetry_due = 0, timerSATPublishRetry_interval = 500;
static bool dueF(uint32_t& d, uint32_t iv) { if ((int32_t)(g_ms - d) >= 0) { d = g_ms + iv; return true; } return false; }
#define DUE(t) dueF(t##_due, t##_interval)
static bool _sat_publishRetryPending = false;
static void satPublishMQTT() {}
static const uint32_t SAT_STALE_OUTDOOR_MS = 600000UL;
@OUTSIDE@
@PERSIST@
@SENDERS@
@DISABLE@
@LINK@
@RESTART@
static bool prePass() {
@SPAN@
  return true;
}
static void bootInit() {
@BOOT@
}
static void initModeBranch() {
@INITMODE@
}
static void savePidFile(long long ts, long long otTs, bool withOutside) {
  FILE* f = fopen("sat_pid_state.json", "wb");
  if (withOutside) fprintf(f, "{\"i\":1.5000,\"d\":0.0000,\"rd\":0.0000,\"err\":0.20,\"ts\":%lld,\"ot\":7.50,\"ots\":%lld}", ts, otTs);
  else             fprintf(f, "{\"i\":1.5000,\"d\":0.0000,\"rd\":0.0000,\"err\":0.20,\"ts\":%lld}", ts);
  fclose(f);
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  const unsigned endS = (unsigned)atoi(argv[2]);
  const long long wall0 = 1800000000LL;
  // --- file round trips (no control passes) ---
  if (!strcmp(sc, "save")) {           // weather 7.5 C measured at t=1 s, PID state saved at t=100 s
    g_ms = 1000; state.sat.weather.bValid = true; state.sat.weather.iLastUpdateMs = g_ms; state.sat.weather.fTemperature = 7.5f;
    g_ms = 100000; g_now = wall0 + 100; satSavePidState();
    return 0;
  }
  if (!strcmp(sc, "load")) {           // 120 s later, 2 s after the new boot
    g_ms = 2000; g_now = wall0 + 220; satLoadPidState();
    float v = 0; uint32_t ms = 0;
    printf("RESTORED valid=%d val=%.2f age=%u pid=%d\n", (int)state.sat.bRestoredOutdoorValid, state.sat.fRestoredOutdoor,
           (unsigned)((uint32_t)(g_ms - state.sat.iRestoredOutdoorMs) / 1000), g_pidRestored);
    return 0;
  }
  if (!strcmp(sc, "load_old")) {       // a file from before TASK-1482: no outside fields
    savePidFile(wall0 - 60, 0, false);
    g_ms = 2000; g_now = wall0; satLoadPidState();
    printf("RESTORED valid=%d pid=%d\n", (int)state.sat.bRestoredOutdoorValid, g_pidRestored);
    return 0;
  }
  if (!strcmp(sc, "pwm_set")) {        // SAT ran in PWM: one loop pass after the switch
    state.sat.eControlMode = SAT_MODE_PWM;
    g_ms = 5000; state.otBus.iBoilerLastSeenMs = g_ms; state.sat.bExternalOutdoorValid = true;
    state.sat.iExternalOutdoorLastMs = g_ms; state.sat.fExternalOutdoor = 5.0f;
    prePass();
    return 0;
  }
  if (!strcmp(sc, "pwm_boot")) {       // the next boot: initSAT()'s enabled branch
    state.sat.eControlMode = SAT_MODE_OFF;
    initModeBranch();
    printf("MODE %d\n", state.sat.eControlMode);
    return 0;
  }
  // --- control passes, one per second ---
  if (!strncmp(sc, "push_", 5)) { settings.sat.bPushSetpoint = true; sc += 5; }   // TASK-1520: push setpoint on
  const bool valid = !strcmp(sc, "boot_valid") || !strcmp(sc, "boot_valid_late_ntp") || !strcmp(sc, "boiler_late");
  if (valid) savePidFile(wall0 - 120, wall0 - 600, true);                   // PID 2 min old, outside 10 min old
  if (!strcmp(sc, "ntp_never")) savePidFile(wall0 - 120, wall0 - 600, true);
  if (!strcmp(sc, "boot_valid_late_ntp") || !strcmp(sc, "ntp_never")) g_now = 0;
  g_ms = 1000;
  bootInit();
  bool ntpRestoreDone = g_now != 0;
  for (; g_ms / 1000 < endS; g_ms += 1000) {
    const unsigned t = g_ms / 1000;
    if (g_now) g_now = wall0 + t;
    if (!strcmp(sc, "boot_valid_late_ntp") && t == 20) g_now = wall0 + t;
    if (!ntpRestoreDone && g_now) {     // satControlLoopRun()'s deferred restore once NTP is set
      ntpRestoreDone = true;
@DEFERRED@
    }
    const bool boilerUp = strcmp(sc, "boiler_late") ? true : t >= 10;
    if (boilerUp) state.otBus.iBoilerLastSeenMs = g_ms;
    if (!strcmp(sc, "boot_nostate") && t >= 300 && (t - 300) % 900 == 0) {        // first weather fetch at 300 s
      state.sat.weather.bValid = true; state.sat.weather.iLastUpdateMs = g_ms; state.sat.weather.fTemperature = 8.0f;
    }
    if (!strcmp(sc, "weather_stale") && (t == 1 || t == 4000)) {                 // fetched at 1 s, fetches fail until 4000 s
      state.sat.weather.bValid = true; state.sat.weather.iLastUpdateMs = g_ms; state.sat.weather.fTemperature = 6.0f;
    }
    if (!strcmp(sc, "external") && t == 1) {                                      // one MQTT value, then nothing
      state.sat.bExternalOutdoorValid = true; state.sat.iExternalOutdoorLastMs = g_ms; state.sat.fExternalOutdoor = 4.0f;
    }
    if (!strcmp(sc, "ot27") || !strcmp(sc, "ot27_nostamp")) {
      OTcurrentSystemState.Toutside = 5.0f;                                       // the bus value (PS=1 summary or a live frame)
      if (!strcmp(sc, "ot27") && t % 60 == 1) state.otBus.iToutsideBoilerMs = g_ms;   // live boiler Read-Ack every minute
    }
    if (prePass()) {                                                              // control tick stand-in
      state.sat.bActive = true;
      float outside = satGetOutsideTemp();
      satSendCS(40.0f - outside);
      satSendMM(100);
      satSendCH(1);
    }
  }
  printf("END missing=%d\n", (int)state.sat.bOutsideTempMissing);
  return 0;
}
"""


def _between(src, a, b):
    i = src.index(a)
    return src[i:src.index(b, i)]


def build_cpp(src: str) -> str:
    fix = "// --- satRestart begin" in src
    if "// --- satOutside begin" in src:
        outside = _between(src, "// --- satOutside begin", "// --- satOutside end")
    else:   # HEAD: satGetOutsideTemp() only
        i = src.index("static float satGetOutsideTemp()")
        outside = src[i:src.index("\n}\n", i) + 3]
    persist = _between(src, "//=== PID State Persistence", "//=== Energy State Persistence")
    a = src.index("static const uint32_t SAT_CMD_REFRESH_MS")
    senders = src[a:src.index("// TASK-1272, Sergeant D entry 14", a)]
    senders += _between(src, "// TASK-1272, Sergeant D entry 14", "// Push setpoint switched off")   # TC= (TASK-1520)
    disable = _between(src, "void satDisable(", "// --- satOtLink begin")
    decl = "void satDisable(bool persistPid = true);" if "bool persistPid" in disable else ""
    link = _between(src, "// --- satOtLink begin", "// --- satOtLink end")
    restart = _between(src, "// --- satRestart begin", "// --- satRestart end") if fix else ""
    s0 = src.index("  // If safety tripped, stay disabled until explicitly re-enabled")
    end = "  if (!DUE(timerSATControl)) return;"
    span = src[s0:src.index(end, s0) + len(end)].replace("return;", "return false;")
    if fix:
        boot = "  _sat_bootStartMs = millis();\n  satRestartNotePidLoad(satLoadPidState());"
        deferred = "      satRestartNotePidLoad(satLoadPidState());"
    else:
        boot = "  satLoadPidState();"
        deferred = "      satLoadPidState();"
    i = src.index("void initSAT()")
    j = src.index("  if (settings.sat.bEnabled) {\n", i) + len("  if (settings.sat.bEnabled) {\n")
    initmode = src[j:src.index("    Debug", j)]
    return (STUBS.replace("@OUTSIDE@", outside).replace("@PERSIST@", decl + "\n" + persist)
            .replace("@SENDERS@", senders).replace("@DISABLE@", disable).replace("@LINK@", link)
            .replace("@RESTART@", restart).replace("@SPAN@", span).replace("@BOOT@", boot)
            .replace("@DEFERRED@", deferred).replace("@INITMODE@", initmode))


class Harness:
    exe = None
    dir = None

    @classmethod
    def setup(cls):
        if cls.exe is not None or cls.dir is not None:
            return
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")


def run(sc, end_s=1, workdir=None):
    Harness.setup()
    if not Harness.exe:
        raise unittest.SkipTest("no host C++ compiler")
    cwd = workdir or tempfile.mkdtemp(dir=Harness.dir.name)
    out = subprocess.run([str(Harness.exe), sc, str(end_s)], capture_output=True, text=True, check=True, cwd=cwd).stdout
    cmds = []
    for line in out.splitlines():
        m = re.match(r"^(\d+) (\S+)$", line)
        if m:
            cmds.append((int(m.group(1)), m.group(2)))
    return cmds, out


HANDBACK = ["CS=0", "CH=1", "MM=T"]


def sat_cs(cmds):
    """SAT's own computed setpoints (CS=<x> with x != 0)."""
    return [(t, c) for t, c in cmds if c.startswith("CS=") and c not in ("CS=0",)]


class Restart(unittest.TestCase):
    def test_push_on_boot_without_state_sends_tc_at_the_decision_not_after_the_outside_wait(self):
        # TASK-1520 (gruijter alpha.567): with push setpoint on, SAT's TC= goes out at the boot decision, so follow
        # thermostat can adopt a dial change made while SAT waits for an outside temperature. Red on the old code: the
        # first TC= comes with the takeover at 300 s.
        cmds, _ = run("push_boot_nostate", 320)
        tc = [(t, c) for t, c in cmds if c.startswith("TC=")]
        self.assertEqual(tc, [(1, "TC=15.0")], cmds)
        self.assertEqual([c for t, c in cmds if t < 300 and not c.startswith("TC=")], HANDBACK, cmds)

    def test_push_on_boot_with_valid_state_sends_tc_once(self):
        cmds, _ = run("push_boot_valid", 40)
        self.assertEqual([(t, c) for t, c in cmds if c.startswith("TC=")], [(1, "TC=15.0")], cmds)

    def test_push_off_boot_sends_no_tc(self):
        cmds, _ = run("boot_nostate", 320)
        self.assertEqual([c for _, c in cmds if c.startswith("TC=")], [], cmds)

    def test_boot_without_state_hands_back_and_stays_quiet_until_outside_valid(self):
        # Entry 121: no valid restored state -> hand-back once, then nothing of SAT's own until a boiler reply AND a
        # valid outside temperature; the first weather fetch at 300 s lets SAT take over. Red on HEAD: CS=40.0 at 31 s
        # (curve with outside 0.0) long before any outside temperature exists.
        cmds, _ = run("boot_nostate", 320)
        early = [c for t, c in cmds if t < 300]
        self.assertEqual(early, HANDBACK, cmds)
        self.assertEqual(sat_cs(cmds)[:1], [(300, "CS=32.0")], cmds)   # takeover in the pass the weather arrives

    def test_boot_with_valid_state_sends_nothing_and_takes_over_at_the_first_pass(self):
        # Entries 126, 129: PID state and outside temperature (10 min old) restored -> no CS=0 / CH / MM at boot; SAT
        # takes over in the first pass (not one interval later), from the restored 7.5 C. Red on HEAD: CS=0 at boot,
        # first tick at 31 s with outside 0.0 (CS=40.0).
        cmds, _ = run("boot_valid", 40)
        self.assertNotIn("CS=0", [c for _, c in cmds], cmds)
        self.assertEqual(cmds[0], (1, "CS=32.5"), cmds)

    def test_boot_waits_for_ntp_before_it_decides(self):
        # The PID file needs the clock: SAT sends nothing until NTP (20 s), then takes over at once.
        cmds, _ = run("boot_valid_late_ntp", 40)
        self.assertTrue(cmds, "SAT never took over")
        self.assertEqual(cmds[0], (20, "CS=32.5"), cmds)

    def test_no_ntp_hands_back_after_the_decide_window(self):
        # Our choice (SAT_BOOT_DECIDE_MS 60 s, interpretation, not confirmed by Sergeant D): no clock -> the restored
        # state cannot be checked; after 60 s it counts as not valid -> hand-back, then quiet (no outside temperature).
        cmds, _ = run("ntp_never", 90)
        self.assertEqual(cmds, [(61, c) for c in HANDBACK], cmds)

    def test_boot_waits_for_a_boiler_reply(self):
        # Entry 121: also with a valid restored state the first CS waits for a boiler reply after boot (10 s here).
        cmds, _ = run("boiler_late", 30)
        self.assertEqual(cmds[0], (10, "CS=32.5"), cmds)


class OutsideTemperature(unittest.TestCase):
    def test_weather_older_than_one_hour_hands_back_once_and_resumes(self):
        # Entries 56, 57, 79: the weather value of 1 s expires at 3601 s -> one hand-back, then silence, then SAT
        # resumes when the fetch at 4000 s succeeds. Red on HEAD: the hour-old value keeps being used.
        # TASK-1370: the 5 s satCmdResend() is armed by the ticks before 3601 s and sits behind the same gate, so the
        # exact-equality check on `mid` also proves it stays silent while the outside temperature is missing.
        cmds, out = run("weather_stale", 4010)
        mid = [(t, c) for t, c in cmds if 3601 <= t < 4000]
        self.assertEqual(mid, [(3601, c) for c in HANDBACK], cmds[-12:])
        back = [t for t, c in sat_cs(cmds) if t >= 4000]
        self.assertTrue(back and back[0] <= 4031, cmds[-12:])   # the next control tick after the fetch

    def test_external_value_stays_valid_for_one_hour(self):
        # Entries 57, 126 ("valid < 1 h whatever its source"); after 10 min it only loses priority (our reading,
        # not confirmed). One MQTT value at 1 s (boot without a PID file: the boot hand-back at 1 s), then no
        # hand-back at 601 s, one at 3601 s.
        cmds, _ = run("external", 3620)
        backs = [t for t, c in cmds if c == "MM=T"]
        self.assertEqual(backs, [1, 3601], cmds[-10:])

    def test_msgid27_counts_only_with_a_live_boiler_read_ack(self):
        cmds, _ = run("ot27", 40)
        self.assertEqual(sat_cs(cmds)[:1], [(1, "CS=35.0")], cmds)

    def test_msgid27_without_a_read_ack_is_no_outside_temperature(self):
        # A PS=1 summary repeats a stored value with no age (ADR-181): it never makes the outside temperature valid.
        cmds, _ = run("ot27_nostamp", 120)
        self.assertEqual(sat_cs(cmds), [], cmds)


class Persistence(unittest.TestCase):
    def test_outside_temperature_survives_a_restart_with_its_age(self):
        # Entry 126: saved with the PID state, restored at boot with the age of the measurement (99 s at the save
        # plus 120 s offline). Red on HEAD: no outside temperature in the file.
        Harness.setup()
        if not Harness.exe:
            self.skipTest("no host C++ compiler")
        wd = tempfile.mkdtemp(dir=Harness.dir.name)
        run("save", workdir=wd)
        _, out = run("load", workdir=wd)
        m = re.search(r"RESTORED valid=(\d) val=([-\d.]+) age=(\d+) pid=(\d)", out)
        self.assertTrue(m, out)
        self.assertEqual(m.group(1), "1", out)
        self.assertAlmostEqual(float(m.group(2)), 7.5, places=2)
        self.assertTrue(219 <= int(m.group(3)) <= 221, out)
        self.assertEqual(m.group(4), "1", out)

    def test_file_without_outside_fields_restores_no_outside_temperature(self):
        _, out = run("load_old")
        self.assertIn("RESTORED valid=0 pid=1", out)

    def test_pwm_enabled_survives_a_restart(self):
        # Entry 121: PWM enabled before the restart comes back as PWM (thermo-nova pwm.py:111-114). Red on HEAD:
        # continuous after every boot.
        Harness.setup()
        if not Harness.exe:
            self.skipTest("no host C++ compiler")
        wd = tempfile.mkdtemp(dir=Harness.dir.name)
        run("pwm_set", workdir=wd)
        _, out = run("pwm_boot", workdir=wd)
        self.assertIn("MODE 2", out)

    def test_no_pwm_file_boots_continuous(self):
        _, out = run("pwm_boot")
        self.assertIn("MODE 1", out)


class SourceGuards(unittest.TestCase):
    """STRUCTURAL: what the harness cannot execute."""

    @classmethod
    def setUpClass(cls):
        cls.sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.core = (SRC / "OTGW-Core.ino").read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_msgid27_stamp_only_on_a_live_boiler_read_ack(self):
        i = self.core.index("state.otBus.iToutsideBoilerMs = millis();")
        head = self.core[i - 260:i]
        for cond in ("OTdata.id == 27", "OTdata.rsptype == OTGW_BOILER", "OTdata.type == OT_READ_ACK",
                     "!OTdata.skipthis", "!OTdata.bGatewaySubstituted"):
            self.assertIn(cond, head, cond)
        a = self.core.index("static void updatePSSummaryFloatState(")
        self.assertNotIn("iToutsideBoilerMs", self.core[a:self.core.index("\n}\n", a)], "the PS=1 summary must not stamp")

    def test_init_sends_no_sat_command(self):
        i = self.sat.index("void initSAT()")
        body = self.sat[i:self.sat.index("\n}\n", i)]
        self.assertNotIn("satEnqueueOwnCS(", body)
        self.assertNotIn('"CH=', body)
        self.assertNotIn('"MM=', body)

    def test_restart_gate_precedes_every_sender(self):
        i = self.sat.index("static void satControlLoopRun()\n{")
        run_ = self.sat[i:self.sat.index("\n}\n", i)]
        g = run_.index("if (satRestartStep()) return;")
        self.assertGreater(g, run_.index("if (satOtLinkStep()) return;"))
        for later in ("  satPicCSKeepalive();", "  satPwmSuppressionRefresh();", "if (!DUE(timerSATControl)) return;"):
            self.assertLess(g, run_.index(later), later)

    def test_status_published(self):
        self.assertIn('publishIfChangedB(F("sat/outside_temp_missing")', self.sat)
        self.assertIn('je.field(F("outside_temp_missing")', self.sat)


if __name__ == "__main__":
    unittest.main(verbosity=2)
