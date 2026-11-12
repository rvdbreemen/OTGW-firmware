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
typedef uint8_t byte;
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static long random(long lo, long hi) { return hi > lo ? lo : lo; }
struct FSInfo { size_t totalBytes, usedBytes, blockSize, pageSize, maxOpenFiles, maxPathLength; };
static FSInfo LittleFSinfo;
static bool LittleFSmounted = true;
static int g_reads = 0, g_probes = 0;
static bool platformFSInfo(FSInfo &i) { g_reads++; i.totalBytes = 786432; return true; }
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
static bool updateLittleFSStatus(const __FlashStringHelper*) { g_probes++; return true; }
@SAFE@
@FN@
int main() {
  // 16 minutes at 1 s steps (the timers start at the first call)
  for (int s = 0; s < 16 * 60; s++) { g_now += 1000; loopLittleFSStatus(); }
  printf("run reads=%d probes=%d\n", g_reads, g_probes);
  return 0;
}
"""


class HealthDoesNoFlashOp(unittest.TestCase):
    def test_sendHealth_calls_no_fs_info_and_no_write_probe(self):
        a = REST.find("\nvoid sendHealth()\n")
        self.assertNotEqual(a, -1, "sendHealth() not found")
        body = REST[a:REST.index("} // sendHealth()", a)]   # its comments hold unbalanced braces ('// root {')
        self.assertNotIn("platformFSInfo(", body, "sendHealth() still runs a LittleFS traversal on async_tcp")
        self.assertNotIn("updateLittleFSStatus(", body, "sendHealth() still runs the flash write probe on async_tcp")
        self.assertIn("LittleFSmounted", body, "the health answer still reports the mount state")

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
        fn = braced(HELPER, r"^void loopLittleFSStatus\(\)\s*\{")
        if fn is None:
            cls.out = None
            return
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(PROG.replace("@SAFE@", SAFE).replace("@FN@", fn), encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                raise unittest.SkipTest("no host compiler")
            cls.out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout

    def test_mount_check_every_60_s_and_write_probe_every_5_min(self):
        self.assertIsNotNone(self.out, "loopLittleFSStatus() not found in helperStuff.ino")
        m = re.search(r"run reads=(\d+) probes=(\d+)", self.out)
        reads, probes = int(m.group(1)), int(m.group(2))
        self.assertEqual(probes, 3, self.out)                # at 5, 10 and 15 min
        self.assertIn(reads, (13, 14, 15, 16), self.out)      # every 60 s, minus the passes a probe takes
        # the old poll ran 2 traversals per 15 s health call: 128 in 16 min with one browser tab


if __name__ == "__main__":
    unittest.main(verbosity=2)
