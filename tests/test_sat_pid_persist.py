#!/usr/bin/env python3
"""TASK-1415: host test for the SAT PID state persistence (Sergeant D entry 74, Q24: follow thermo-nova).

thermo-nova stores five values per PID (pid.py:243-249: integral, last error, raw derivative, last temperature,
derivative time) and restores all of them with no age check (pid.py:141-157). Nothing is re-implemented here: the
test cuts the real code out of the sources and compiles it with a host C++ compiler:
  - SATcontrol.ino: "//=== PID State Persistence" up to "// --- satPwmFlag begin" (satSavePidState, satLoadPidState);
  - SATpid.ino: the held-state variables and "//=== PID rounding" up to "//=== Reset of the integral" (satPidRound3,
    satPidRestoreState, satPidGetPersistState and, on the fix, satPidGetPersistLast / satPidRestoreLast).
LittleFS is a thin wrapper over real files in a temp directory, so a "reboot" is a second process on the same file.

Old code: point OTGW_SAT_INO and OTGW_SATPID_INO at copies of the HEAD files
(git show HEAD:src/OTGW-firmware/SATcontrol.ino). The five-field and the older-than-30-minutes cases then fail on
behaviour (the restore returns NONE, the last temperature is not restored).

Run: python tests/test_sat_pid_persist.py
"""
import json
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
PID_INO = Path(os.environ.get("OTGW_SATPID_INO", SRC / "SATpid.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
typedef long long time_t_;
#define time_t time_t_
#define F(x) x
#define PSTR(x) x
#define PROGMEM
#define FPSTR(x) (x)
#define snprintf_P snprintf
#define strlcpy_P(d, s, n) (snprintf((d), (n), "%s", (s)))
#define SATDebugTf(...) ((void)0)
#define SATDebugTln(x) ((void)0)
struct Weather { bool bValid = false; };
struct SatSettings { bool bSimulation = false; };
struct Settings { SatSettings sat; };
struct SatState { float fKd = 0, fPidI = 0, fPidD = 0, fError = 0, fRawDerivative = 0;
                  bool bRestoredOutdoorValid = false; float fRestoredOutdoor = 0; uint32_t iRestoredOutdoorMs = 0; };
struct State { SatState sat; };
static Settings settings; static State state;
static const uint32_t SAT_OUTSIDE_VALID_MS = 3600000UL;
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static long long g_now = 1800000000LL;
static long long fake_time() { return g_now; }
#define time(x) fake_time()
static int satPidStateAppendOutside(char*, int n, size_t, time_t) { return n; }   // the outside temperature is tested in test_sat_restart_quiet.py
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
static bool _pid_roomSeen = false;
static bool _pid_firstOutputPending = false;
@PIDVARS@
@PIDFUNCS@
@PERSIST@
"""

MAIN = r"""
int main(int argc, char** argv) {
  const char* sc = argv[1];
  const long long delay = atoll(argv[2]);
  const long long wall0 = 1800000000LL;
  if (!strcmp(sc, "save") || !strcmp(sc, "save_nan")) {
    g_ms = 1000000; g_now = wall0;
    state.sat.fKd = 2.0f;
    _pid_integral = 1.25f; _pid_rawDerivative = 0.02f; _pid_lastRoomTemp = 20.7f;
    _pid_lastError = !strcmp(sc, "save_nan") ? NAN : 0.3f;
    _pid_lastDerivativeMs = g_ms - 90000; _pid_initialized = true;
    satSavePidState();
    return 0;
  }
  if (!strcmp(sc, "save_noclock")) {   // saved with no clock: the derivative time cannot be stored
    g_ms = 1000000; g_now = 0;
    _pid_integral = 1.25f; _pid_lastRoomTemp = 20.7f; _pid_lastError = 0.3f; _pid_lastDerivativeMs = 5; _pid_initialized = true;
    satSavePidState();
    return 0;
  }
  if (!strcmp(sc, "old_file")) {       // a file from before TASK-1415: no lt, dts; err is a number
    FILE* f = fopen("sat_pid_state.json", "wb");
    fprintf(f, "{\"i\":1.2500,\"d\":0.0400,\"rd\":0.0200,\"err\":0.30,\"ts\":%lld}", wall0);
    fclose(f);
  }
  if (!strcmp(sc, "load_noclock")) { g_ms = 5000; g_now = 0; }
  else if (!strcmp(sc, "load_past"))  { g_ms = 5000; g_now = wall0 - 500; }   // clock stepped back before the save
  else { g_ms = 5000; g_now = wall0 + delay; }
  const uint8_t ret = satLoadPidState();
  printf("RET=%d I=%.4f RD=%.4f HELD=%d ERR=%.3f LT=%.3f INIT=%d DAGE=%u\n", (int)ret, _pid_integral, _pid_rawDerivative,
         (int)!isnan(_pid_lastError), isnan(_pid_lastError) ? 0.0f : _pid_lastError, _pid_lastRoomTemp, (int)_pid_initialized,
         (unsigned)((uint32_t)(g_ms - _pid_lastDerivativeMs) / 1000));
  return 0;
}
"""


def _between(src, a, b):
    i = src.index(a)
    return src[i:src.index(b, i)]


def build_cpp():
    sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    pid = PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    persist = _between(sat, "//=== PID State Persistence", "// --- satPwmFlag begin")
    pidvars = _between(pid, "static float  _pid_lastError", "// The PID moves on its own 60 s timer")
    pidfuncs = _between(pid, "//=== PID rounding (entry 95, Q28a)", "//=== Reset of the integral and the raw derivative")
    return STUBS.replace("@PIDVARS@", pidvars).replace("@PIDFUNCS@", pidfuncs).replace("@PERSIST@", persist) + MAIN


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
        cpp.write_text(build_cpp(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")


def run(sc, delay=0, cwd=None):
    Harness.setup()
    if not Harness.exe:
        raise unittest.SkipTest("no host C++ compiler")
    out = subprocess.run([str(Harness.exe), sc, str(delay)], capture_output=True, text=True, check=True, cwd=cwd).stdout
    m = re.search(r"RET=(\d+) I=(\S+) RD=(\S+) HELD=(\d) ERR=(\S+) LT=(\S+) INIT=(\d) DAGE=(\d+)", out)
    if not m:
        return None
    k = ("ret", "i", "rd", "held", "err", "lt", "init", "dage")
    return {n: float(v) for n, v in zip(k, m.groups())}


def fresh_dir():
    Harness.setup()
    return tempfile.mkdtemp(dir=Harness.dir.name)


class PidPersist(unittest.TestCase):
    def test_round_trip_of_all_five_fields(self):
        # AC1: integral, last error, raw derivative, last temperature and the derivative time survive a reboot.
        d = fresh_dir()
        run("save", cwd=d)
        r = run("load", 120, cwd=d)
        self.assertEqual(r["ret"], 1, r)
        self.assertAlmostEqual(r["i"], 1.25, places=3)
        self.assertAlmostEqual(r["rd"], 0.02, places=3)
        self.assertEqual((r["held"], round(r["err"], 3)), (1, 0.3), r)
        self.assertAlmostEqual(r["lt"], 20.7, places=2)
        self.assertEqual(r["init"], 1, r)
        self.assertEqual(r["dage"], 90 + 120, "the derivative time is 90 s before the save plus the 120 s gap")

    def test_file_older_than_30_minutes_is_still_restored(self):
        # AC1: no age check (entry 74). Red on the old code: the 30 minute limit discards it.
        d = fresh_dir()
        run("save", cwd=d)
        for gap in (1801, 7200, 90 * 86400):
            r = run("load", gap, cwd=d)
            self.assertEqual(r["ret"], 1, (gap, r))
            self.assertAlmostEqual(r["i"], 1.25, places=3)
            self.assertAlmostEqual(r["lt"], 20.7, places=2)
        self.assertEqual(r["dage"], 4000000, "a very long gap is capped so the uptime stamp cannot wrap")

    def test_nan_last_error_is_saved_and_restored_without_corrupting_the_file(self):
        # AC2: NAN (no held error, satPidCurveOnly) is written as null, the file stays valid JSON, the restore keeps
        # the integral and leaves no error held.
        d = fresh_dir()
        run("save_nan", cwd=d)
        text = Path(d, "sat_pid_state.json").read_text()
        self.assertNotIn("nan", text.lower())
        data = json.loads(text)
        self.assertIsNone(data["err"])
        r = run("load", 60, cwd=d)
        self.assertEqual(r["ret"], 1, r)
        self.assertEqual(r["held"], 0, r)
        self.assertAlmostEqual(r["i"], 1.25, places=3)
        self.assertAlmostEqual(r["lt"], 20.7, places=2)

    def test_saved_file_is_valid_json_with_the_five_fields(self):
        d = fresh_dir()
        run("save", cwd=d)
        data = json.loads(Path(d, "sat_pid_state.json").read_text())
        for k in ("i", "rd", "err", "lt", "dts", "ts"):
            self.assertIn(k, data)
        self.assertEqual(data["ts"] - data["dts"], 90)

    def test_file_from_before_the_change_restores_integral_only(self):
        d = fresh_dir()
        r = run("old_file", 3600, cwd=d)
        self.assertEqual(r["ret"], 1, r)
        self.assertAlmostEqual(r["i"], 1.25, places=3)
        self.assertEqual(r["init"], 0, "no last temperature in the file: the first live step initialises as before")

    def test_no_clock_waits_for_ntp(self):
        d = fresh_dir()
        run("save", cwd=d)
        self.assertEqual(run("load_noclock", cwd=d)["ret"], 2)

    def test_save_without_clock_writes_no_derivative_time(self):
        d = fresh_dir()
        run("save_noclock", cwd=d)
        data = json.loads(Path(d, "sat_pid_state.json").read_text())
        self.assertNotIn("dts", data)
        self.assertNotIn("lt", data)

    def test_clock_before_the_save_time_still_restores(self):
        d = fresh_dir()
        run("save", cwd=d)
        r = run("load_past", cwd=d)
        self.assertEqual(r["ret"], 1, r)
        self.assertEqual(r["dage"], 0, r)


if __name__ == "__main__":
    unittest.main()
