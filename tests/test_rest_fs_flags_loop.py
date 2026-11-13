#!/usr/bin/env python3
"""TASK-1899 AC1: /api/v2/simulate and /api/v2/device/info read file-exists flags the loop task keeps.

Both handlers run on async_tcp and used to call LittleFS.exists() there (restAPI.ino sendSimulationStatus and
sendDeviceInfoV2). That cross-core flash read hung once and starved the loop task into the task watchdog (gruijter,
13 Nov 04:00:54, core dump). The fix: refreshLittleFSFileFlags() in helperStuff.ino checks both files on the loop task,
from loopLittleFSStatus() (which loop() runs only while not flashing), and the handlers read the two flags.

1. Host test: compiles the real "// --- fsFileFlags" block out of helperStuff.ino with a LittleFS stub and checks
   that the flags follow the files and the mount state.
2. Source test: the handlers read the flags and call no LittleFS; loopLittleFSStatus() refreshes the flags on its
   first pass and then on a timer; loop() runs it behind !isFlashing(); no REST file calls the refresh.

OTGW_RESTAPI_INO / OTGW_HELPER_INO point at older copies to see it red.

Run: python tests/test_rest_fs_flags_loop.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
REST_INO = Path(os.environ.get("OTGW_RESTAPI_INO", SRC / "restAPI.ino"))
HELPER_INO = Path(os.environ.get("OTGW_HELPER_INO", SRC / "helperStuff.ino"))
MAIN_INO = SRC / "OTGW-firmware.ino"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
from test_rest_no_littlefs_on_async_tcp import blank_comments_and_strings, function_body, littlefs_calls  # noqa: E402

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define FS_UPDATE_INCOMPLETE_FILE "/.fs_update_incomplete"
static bool g_sim = false, g_inc = false; static int g_calls = 0;
struct LittleFSStub { bool exists(const char* p) { g_calls++;
  if (!strcmp(p, "/otgw_simulation.log")) return g_sim;
  if (!strcmp(p, FS_UPDATE_INCOMPLETE_FILE)) return g_inc;
  return false; } } LittleFS;
bool LittleFSmounted = true, LittleFSsimFilePresent = false, LittleFSupdateIncomplete = false;
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: mounted sim incomplete
  LittleFSmounted = atoi(argv[1]); g_sim = atoi(argv[2]); g_inc = atoi(argv[3]);
  LittleFSsimFilePresent = !g_sim; LittleFSupdateIncomplete = !g_inc;   // stale values the refresh must replace
  refreshLittleFSFileFlags();
  printf("%d %d %d\n", (int)LittleFSsimFilePresent, (int)LittleFSupdateIncomplete, g_calls);
  return 0;
}
"""


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8").replace("\r\n", "\n")


class FlagsRefreshHost(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = read(HELPER_INO)
        m = re.search(r"// --- fsFileFlags begin\n(.*?)// --- fsFileFlags end", src, re.S)
        if not m:
            raise AssertionError("fsFileFlags block not found in " + str(HELPER_INO))
        cls._dir = tempfile.TemporaryDirectory()
        cpp = Path(cls._dir.name) / "t.cpp"
        cpp.write_text(STUBS + m.group(1) + MAIN, encoding="utf-8")
        cls._exe = compile_cpp(cpp, Path(cls._dir.name) / "t.exe")
        if cls._exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls._dir.cleanup()

    def run_it(self, mounted, sim, inc):
        out = subprocess.run([str(self._exe), str(mounted), str(sim), str(inc)], capture_output=True, text=True,
                             check=True).stdout.split()
        return tuple(int(x) for x in out)

    def test_flags_follow_the_files(self):
        self.assertEqual(self.run_it(1, 1, 0)[:2], (1, 0))
        self.assertEqual(self.run_it(1, 0, 1)[:2], (0, 1))
        self.assertEqual(self.run_it(1, 1, 1)[:2], (1, 1))
        self.assertEqual(self.run_it(1, 0, 0)[:2], (0, 0))

    def test_unmounted_reports_no_files_and_reads_nothing(self):
        self.assertEqual(self.run_it(0, 1, 1), (0, 0, 0))


class HandlersAndWiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rest = read(REST_INO)
        cls.helper = read(HELPER_INO)
        cls.main = read(MAIN_INO)

    def test_simulate_handler_reads_the_flag_not_littlefs(self):
        body = function_body(self.rest, "sendSimulationStatus")
        self.assertEqual([c for c in littlefs_calls(body)], [], "sendSimulationStatus calls LittleFS on async_tcp")
        self.assertIn("LittleFSsimFilePresent", body)

    def test_device_info_reads_the_flag_not_littlefs(self):
        body = function_body(self.rest, "sendDeviceInfoV2")
        self.assertEqual([c for c in littlefs_calls(body)], [], "sendDeviceInfoV2 calls LittleFS on async_tcp")
        self.assertRegex(body, r'je\.field\(F\("fsupdateincomplete"\),\s*LittleFSupdateIncomplete\)')

    def test_loop_task_refreshes_first_pass_then_on_a_timer(self):
        body = function_body(self.helper, "loopLittleFSStatus")
        self.assertIn("refreshLittleFSFileFlags();", body)
        self.assertRegex(body, r"if \(!fsFileFlagsRead \|\| DUE\(fsFileFlags\)\)")
        self.assertRegex(body, r"DECLARE_TIMER_SEC\(fsFileFlags,\s*\d+")

    def test_loop_runs_it_only_while_not_flashing(self):
        self.assertRegex(self.main, r"if \(!isFlashing\(\)\) loopLittleFSStatus\(\);")

    def test_no_rest_file_runs_the_refresh(self):
        # a call, not a mention in a comment
        self.assertNotRegex(blank_comments_and_strings(self.rest), r"\brefreshLittleFSFileFlags\s*\(\s*\)\s*;")


if __name__ == "__main__":
    unittest.main(verbosity=2)
