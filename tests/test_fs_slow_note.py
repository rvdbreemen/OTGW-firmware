#!/usr/bin/env python3
"""TASK-1995 Step A: the LittleFS steps of the loop name themselves when they are slow.

09's capture on .61 (01d24c5, 40 min) put every [loop-stall] in step CPROBE_APP_END -> CPROBE_PRE_MQTT, which holds
loopLittleFSStatus() (helperStuff.ino): a 5-min /.health write probe that also runs a platformFSInfo() traversal, the
10-s file flags, the Dallas labels, the SAT markers and the 60-s mount check (another traversal). Instrumentation only:
  - fsSlowNote() (REAL code, compiled on the host) prints '[fs-slow] <step> <ms> ms' at 100 ms or more, nothing below;
  - every sub-step of loopLittleFSStatus() is timed with its own name (source check);
  - doBackgroundTasks() probes CPROBE_FS_END right after loopLittleFSStatus(), so the [loop-stall] line splits the step.
On the code before Step A the function and the probe do not exist and every case fails.
Run: python tests/test_fs_slow_note.py   (OTGW_HELPER_INO / OTGW_MAIN_INO point at other copies for old-vs-fix)
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
HELPER = Path(os.environ.get("OTGW_HELPER_INO", FW / "helperStuff.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
MAIN = Path(os.environ.get("OTGW_MAIN_INO", FW / "OTGW-firmware.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
FSW = Path(os.environ.get("OTGW_FSW_INO", FW / "fsWriter.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, head):
    i = src.find(head)
    if i < 0:
        return ""
    j = src.index("{", i) + 1
    depth = 1
    while depth:
        depth += {"{": 1, "}": -1}.get(src[j], 0)
        j += 1
    return src[i:j]


PRE = r"""
#include <cstdint>
#include <cstdio>
typedef const char* PGM_P;
#define PSTR(s) (s)
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
#define DebugTf(...) printf(__VA_ARGS__)
"""

MAIN_CPP = r"""
int main(int argc, char** argv) {
  uint32_t t0 = 1000;
  g_now = t0 + (uint32_t)atoi(argv[1]);
  fsSlowNote(PSTR("write-probe"), t0);
  printf("|end\n");
  return 0;
}
"""


class FsSlowNote(unittest.TestCase):
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        fn = braced(HELPER, "static void fsSlowNote(PGM_P step, uint32_t t0)")
        if not fn:
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text("#include <cstdlib>\n" + PRE + fn + MAIN_CPP, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def note(self, ms):
        self.assertIsNotNone(self.exe, "fsSlowNote() not found in helperStuff.ino (or no host compiler)")
        # bytes, not text mode: the Windows CRT turns the firmware's "\r\n" into "\r\r\n"
        out = subprocess.run([str(self.exe), str(ms)], capture_output=True, check=True).stdout
        return out.decode("ascii").replace("\r", "")

    def test_slow_step_is_named(self):
        self.assertEqual(self.note(100), "[fs-slow] write-probe 100 ms\n|end\n")
        self.assertEqual(self.note(410), "[fs-slow] write-probe 410 ms\n|end\n")

    def test_fast_step_prints_nothing(self):
        self.assertEqual(self.note(0), "|end\n")
        self.assertEqual(self.note(99), "|end\n")

    def test_every_sub_step_is_timed(self):
        body = braced(HELPER, "void loopLittleFSStatus()")
        self.assertTrue(body, "loopLittleFSStatus() not found")
        for step in ("file-flags", "dallas-labels", "sat-markers"):
            with self.subTest(step=step):
                self.assertIn('fsSlowNote(PSTR("%s")' % step, body)
        # TASK-1995 Step B: the write probe and the mount check run in fsHealthService() on the fsWriter task, which
        # only measures them; the loop task prints the note (fsHealthPrintNote(), tested in test_health_no_fs_flash_ops)
        health = braced(HELPER, "bool fsHealthService()")
        self.assertTrue(health, "fsHealthService() not found")
        for note in ("fsHealthTimed(FS_NOTE_PROBE, t0)", "fsHealthTimed(FS_NOTE_MOUNT, t0)"):
            with self.subTest(note=note):
                self.assertIn(note, health)
        self.assertIn("fsHealthPrintNote();", body)


class WriterTaskPrintsNothing(unittest.TestCase):
    """TASK-1995 Step B respin: no Debug* or telnet output on the fsWriter task. _debugBOL() works in static buffers
    every caller shares, so a print there mixes its line with the loop task's (the telnet write itself is locked);
    the formatting (320 B buffer + vsnprintf) also needs stack the 5120 B task cannot spare. Every function the writer task runs is checked; fsSlowNote()
    counts as output (it calls DebugTf). Red on the first 370 build (ad83affee), which logged from fsHealthService()."""

    PRINT = re.compile(r"\b(Debug\w*|SATDebug\w*|debugTelnet\s*\.\s*\w+|fsSlowNote|Serial\s*\.\s*print\w*)\s*\(")

    def check(self, src, head):
        body = braced(src, head)
        self.assertTrue(body, "%s not found" % head)
        hits = [m.group(0) for m in self.PRINT.finditer(body)]
        self.assertEqual(hits, [], "%s prints on the fsWriter task: %s" % (head, hits))

    def test_health_checks_print_nothing(self):
        for head in ("bool fsHealthService()", "static void fsHealthTimed(", "bool updateLittleFSStatus(const char *probePath)",
                     "bool updateLittleFSStatus(const __FlashStringHelper *probePath)"):
            with self.subTest(fn=head):
                self.check(HELPER, head)

    def test_writer_task_path_prints_nothing(self):
        for head in ("bool fsWriterStep()", "void fsWriterService(const FsWriteJob& job)", "static void fsWriterTask(void*)"):
            with self.subTest(fn=head):
                self.check(FSW, head)

    def test_the_matcher_finds_a_print(self):   # the check itself: a DebugTf in a body is found
        self.assertTrue(self.PRINT.search('  DebugTf(PSTR("x"));'))
        self.assertTrue(self.PRINT.search('  fsSlowNote(PSTR("write-probe"), t0);'))
        self.assertFalse(self.PRINT.search('  fsHealthTimed(FS_NOTE_PROBE, t0);'))

    def test_fs_end_probe_follows_the_fs_check(self):
        self.assertRegex(MAIN, r"loopLittleFSStatus\(\);[^\n]*\n\s*connProbe\(CPROBE_FS_END\);")


if __name__ == "__main__":
    unittest.main(verbosity=2)
