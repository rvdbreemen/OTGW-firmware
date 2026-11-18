#!/usr/bin/env python3
"""TASK-1865: /api/v2/health does no LittleFS flash operation on async_tcp; the loop task keeps the mount state.

Core dump on beta.182 (gruijter, 2026-11-12): async_tcp (prio 10, core 1) hung in a cross-core flash read inside
sendHealth() -> platformFSInfo() -> LittleFS.totalBytes() -> lfs_fs_size(), a full metadata traversal that the poll
ran every 15 s (twice: totalBytes and usedBytes) only to refresh the bool LittleFSmounted; loopTask on the same core
was starved for 30 s and the task watchdog fired. The 5-minute write probe (/.health) also ran there.

After the fix the mount check (60 s) and the write probe (5 min) run in loopLittleFSStatus() on the loop task, and
sendHealth() only emits LittleFSmounted.

The code under test is REAL: sendHealth() is read from restAPI.ino, loopLittleFSStatus() from helperStuff.ino, the
timers from safeTimers.h; only millis(), random(), isFlashing(), platformFSInfo() and updateLittleFSStatus() are stubs.

Old-vs-fix: OTGW_REST_INO / OTGW_HELPER_INO / OTGW_MAIN_INO point at the old files.
Run: python tests/test_health_no_fs_flash_ops.py
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

FW = REPO / "src" / "OTGW-firmware"
REST = Path(os.environ.get("OTGW_REST_INO", FW / "restAPI.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
HELPER = Path(os.environ.get("OTGW_HELPER_INO", FW / "helperStuff.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
MAIN = Path(os.environ.get("OTGW_MAIN_INO", FW / "OTGW-firmware.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
SAFE = (FW / "safeTimers.h").read_text(encoding="utf-8")


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        return None
    i, depth = src.index("{", m.end() - 1), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    return None


PROG = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <atomic>
typedef uint8_t byte;
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static long random(long lo, long hi) { return hi > lo ? lo : lo; }
struct FSInfo { size_t totalBytes, usedBytes, blockSize, pageSize, maxOpenFiles, maxPathLength; };
static FSInfo LittleFSinfo;
static bool LittleFSmounted = true;
static int g_reads = 0, g_probes = 0;
static uint32_t g_fsDelay = 0;   // TASK-1995: how long a traversal takes (ms)
static bool platformFSInfo(FSInfo &i) { g_reads++; g_now += g_fsDelay; i.totalBytes = 786432; return true; }
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
static bool g_probeOk = true;   // TASK-1995: the probe result; a failed probe clears the mount flag, as the real one
static bool updateLittleFSStatus(const __FlashStringHelper*) { g_probes++; LittleFSmounted = g_probeOk; return g_probeOk; }
static bool g_flashing = false, g_running = false;                 // TASK-1995: OTA in progress; the fsWriter task runs
static bool isFlashing() { return g_flashing; }
static char g_seq[64]; static int g_seqN = 0;               // TASK-1995: order of the loop-side FS steps and requests
static void seq(char c) { if (g_seqN < 63) { g_seq[g_seqN++] = c; g_seq[g_seqN] = 0; } }
static bool fsWriterRunning() { seq('R'); return g_running; }   // called by fsHealthRequest(): R = a request
static int g_fileFlags = 0;
static void refreshLittleFSFileFlags() { g_fileFlags++; seq('F'); }   // TASK-1899: file-exists flags for REST
static void loopDallasLabels() { seq('D'); }                         // TASK-1912: writes only a changed labels copy
static void loopSatMarkers() { seq('M'); }                           // TASK-1911: writes only a changed markers copy
typedef const char* PGM_P;
#define PSTR(s) (s)
static void fsSlowNote(PGM_P, uint32_t) { }                  // TASK-1995: the timing line, tested in test_fs_slow_note.py
#define DebugTf(...) printf(__VA_ARGS__)                     // TASK-1995: the loop task prints the writer's note
@SAFE@
@HEALTH@
@FN@
int main(int argc, char** argv) {
  if (argc > 1) return health_case(argv[1]);
  // 16 minutes at 1 s steps (the timers start at the first call)
  for (int s = 0; s < 16 * 60; s++) { g_now += 1000; loopLittleFSStatus(); }
  printf("run reads=%d probes=%d fileflags=%d\n", g_reads, g_probes, g_fileFlags);
  return 0;
}
"""


# TASK-1995 Step B: cases for the probe and mount check on the fsWriter task (g_running = true). The writer task is
# played by calling the REAL fsHealthService() where the writer would take its pass.
HEALTH_CASES = r"""
static void minutes(int m) { for (int s = 0; s < m * 60; s++) { g_now += 1000; loopLittleFSStatus(); } }
static int health_case(const char* c) {
  g_running = true;
  if (!strcmp(c, "deferred")) {
    minutes(6);                                    // the probe falls due at 5 min, the mount check every minute
    printf("loop reads=%d probes=%d\n", g_reads, g_probes);
    int ran = 0; while (fsHealthService()) ran++;  // the writer's passes: one probe, one mount check (coalesced)
    printf("writer ran=%d reads=%d probes=%d\n", ran, g_reads, g_probes);
  } else if (!strcmp(c, "recover")) {
    g_probeOk = false; minutes(6); fsHealthService();   // the probe fails: unmounted
    printf("after_fail mounted=%d\n", (int)LittleFSmounted);
    g_probeOk = true; minutes(5); while (fsHealthService()) { }
    printf("after_next mounted=%d probes=%d reads=%d\n", (int)LittleFSmounted, g_probes, g_reads);
  } else if (!strcmp(c, "flashing")) {
    minutes(6);
    g_flashing = true;
    int ran = (int)fsHealthService();   // before the printf: its argument order is unspecified
    printf("during ran=%d probes=%d\n", ran, g_probes);
    g_flashing = false;
    ran = (int)fsHealthService();
    printf("after ran=%d probes=%d\n", ran, g_probes);
  } else if (!strcmp(c, "order")) {
    // the 5-min pass: the probe, the file flags and the mount check fall due together (the timers start together)
    for (int s = 0; s < 6 * 60; s++) {
      g_seqN = 0; g_seq[0] = 0;
      g_now += 1000; loopLittleFSStatus();
      if (s_fsProbeRequest) { printf("probe_pass seq=%s\n", g_seq); break; }
    }
  } else if (!strcmp(c, "note")) {
    // a slow mount check on the writer: the writer pass prints nothing, the next loop pass prints the note once
    minutes(2);
    g_fsDelay = 150;
    printf("writer>"); fflush(stdout);
    fsHealthService();
    printf("<writer\n");
    g_now += 1000; loopLittleFSStatus();
    g_now += 1000; loopLittleFSStatus();
    printf("end\n");
  }
  return 0;
}
"""


def health_block(src):
    a, b = src.find("// --- fsHealth begin"), src.find("// --- fsHealth end")
    return src[a:b] if a >= 0 and b > a else None


def build_prog(helper, tmp, name):
    fn = braced(helper, r"^void loopLittleFSStatus\(\)\s*\{")
    hb = health_block(helper)
    if fn is None:
        return None
    cpp = Path(tmp) / (name + ".cpp")
    cases = HEALTH_CASES if hb else "static int health_case(const char*) { return 2; }\n"
    cpp.write_text(PROG.replace("@SAFE@", SAFE).replace("@HEALTH@", hb or "").replace("@FN@", fn + cases),
                   encoding="utf-8")
    exe = compile_cpp(cpp, Path(tmp) / (name + (".exe" if os.name == "nt" else "")))
    if exe is None:
        raise unittest.SkipTest("no host compiler")
    return exe


class HealthDoesNoFlashOp(unittest.TestCase):
    def test_sendHealth_calls_no_fs_info_and_no_write_probe(self):
        a = REST.find("\nvoid sendHealth()\n")
        self.assertNotEqual(a, -1, "sendHealth() not found")
        body = REST[a:REST.index("} // sendHealth()", a)]   # its comments hold unbalanced braces ('// root {')
        self.assertNotIn("platformFSInfo(", body, "sendHealth() still runs a LittleFS traversal on async_tcp")
        self.assertNotIn("updateLittleFSStatus(", body, "sendHealth() still runs the flash write probe on async_tcp")
        self.assertIn("LittleFSmounted", body, "the health answer still reports the mount state")

    def test_sendHealth_carries_the_simulation_state_from_loop_task_state(self):
        # TASK-1915: the SIMULATION badge reads health instead of /api/v2/simulate; the same values as
        # sendSimulationStatus(), the file flag from the loop task (TASK-1899), no LittleFS call
        a = REST.find("\nvoid sendHealth()\n")
        body = REST[a:REST.index("} // sendHealth()", a)]
        self.assertRegex(body, r'je\.field\(F\("simulation_active"\),\s*state\.debug\.bOTGWSimulation\);')
        self.assertRegex(body, r'je\.field\(F\("simulation_source"\),\s*LittleFSsimFilePresent \? "file" : "builtin"\);')
        self.assertNotIn("LittleFS.", body)

    def test_loop_task_runs_the_check_outside_flashing(self):   # the flash guard sits in the caller
        body = braced(MAIN, r"^void doBackgroundTasks\(\)\s*\{")
        self.assertIsNotNone(body)
        self.assertRegex(body, r"if \(!isFlashing\(\)\) loopLittleFSStatus\(\);")


class IpcStackLowPointsInDeviceInfo(unittest.TestCase):
    """C: device/info reports the ipc0/ipc1 stack low points (ADR-225 hypothesis check), frozen in the snapshot."""

    def test_snapshot_reads_both_ipc_tasks_and_emits_them(self):
        a = REST.find("\nvoid sendDeviceInfoV2()")
        self.assertNotEqual(a, -1)
        fn = REST[a:REST.index("} // sendDeviceInfoV2()", a)]
        self.assertIn('snap->ipc0StackMin    = platformIpcStackHighWater("ipc0");', fn)
        self.assertIn('snap->ipc1StackMin    = platformIpcStackHighWater("ipc1");', fn)
        self.assertIn('je.field(F("hd_ipc0_stack_min"),       snap->ipc0StackMin);', fn)
        self.assertIn('je.field(F("hd_ipc1_stack_min"),       snap->ipc1StackMin);', fn)


class LoopCheckTiming(unittest.TestCase):
    out = None

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as d:
            exe = build_prog(HELPER, d, "t")
            cls.out = None if exe is None else subprocess.run([str(exe)], capture_output=True, text=True,
                                                               check=True).stdout

    def test_mount_check_every_60_s_and_write_probe_every_5_min(self):
        self.assertIsNotNone(self.out, "loopLittleFSStatus() not found in helperStuff.ino")
        m = re.search(r"run reads=(\d+) probes=(\d+)", self.out)
        reads, probes = int(m.group(1)), int(m.group(2))
        self.assertEqual(probes, 3, self.out)                # at 5, 10 and 15 min
        self.assertIn(reads, (13, 14, 15, 16), self.out)      # every 60 s (no writer task: run inline, as before)
        # TASK-1899: the REST file flags refresh on the first pass, then every 10 s (96 ticks in 16 min, plus the
        # first pass; a pass the write probe takes skips it, the tick stays due for the next one)
        flags = int(re.search(r"fileflags=(\d+)", self.out).group(1))
        self.assertIn(flags, range(94, 99), self.out)
        # the old poll ran 2 traversals per 15 s health call: 128 in 16 min with one browser tab


class ProbeOnTheWriterTask(unittest.TestCase):
    """TASK-1995 Step B: with the fsWriter task running, the loop task only raises requests; the REAL
    fsHealthService() runs them. On the code before Step B the loop runs the probe itself and the cases fail."""
    tmp = exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build_prog(HELPER, cls.tmp.name, "h")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, c):
        self.assertIsNotNone(self.exe, "loopLittleFSStatus() not found")
        p = subprocess.run([str(self.exe), c], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, "no fsHealthService() block in helperStuff.ino (code before Step B)")
        return p.stdout

    def test_the_loop_task_runs_no_traversal_and_no_probe(self):
        out = self.run_case("deferred")
        self.assertIn("loop reads=0 probes=0", out)
        self.assertIn("writer ran=2 reads=1 probes=1", out)   # requests coalesce: one probe, one mount check

    def test_a_failed_probe_is_retried_on_the_writer_and_mounts_again(self):
        out = self.run_case("recover")
        self.assertIn("after_fail mounted=0", out)
        self.assertIn("after_next mounted=1 probes=2", out)

    def test_a_slow_check_is_measured_on_the_writer_and_printed_by_the_loop_task(self):
        # TASK-1995 respin: the first 370 build printed from fsHealthService() on the writer task (here: nothing at
        # all, fsSlowNote() is a stub), so the line below is missing on that code
        out = self.run_case("note").replace("\r", "")
        self.assertIn("writer><writer\n", out, "the writer pass printed something")
        self.assertEqual(out.count("[fs-slow] mount-check 150 ms (writer)\n"), 1, out)
        self.assertTrue(out.endswith("end\n"), out)

    def test_both_requests_come_after_every_loop_side_fs_step_of_the_pass(self):
        # TASK-1995 respin #2: on 6144353 the probe request came first in the 5-min pass, so the writer held the
        # LittleFS lock while the loop's exists() for the file flags waited (09, .61: file-flags 191-394 ms at the
        # probe ticks, a 404 ms [loop-stall]). F = file flags, D = Dallas labels, M = SAT markers, R = a request.
        m = re.search(r"probe_pass seq=(\w+)", self.run_case("order"))
        self.assertIsNotNone(m, "no pass raised the probe request")
        order = m.group(1)
        self.assertEqual(order.count("R"), 2, "the probe and the mount check both fall due in this pass: " + order)
        self.assertIn("F", order, order)
        last_fs = max(order.rfind(c) for c in "FDM")
        self.assertLess(last_fs, order.index("R"), "a request comes before a loop-side FS step: " + order)

    def test_a_request_waits_while_flashing(self):
        out = self.run_case("flashing")
        self.assertIn("during ran=0 probes=0", out)
        self.assertIn("after ran=1 probes=1", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
