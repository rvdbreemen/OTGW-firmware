#!/usr/bin/env python3
"""TASK-1989: the [loop-stall] line names the slowest step of the stalled loop() pass.

gruijter's beta.250 capture had 92 loop stalls (200-600 ms) whose cause the log could only suggest by elimination: a
silent LittleFS write leaves no line. The loop already has probe points (connProbe(CPROBE_*), TASK-1757) between its
steps; LoopStepWatch.h keeps the slowest step of each pass from those probes, always (not only in the boot connect
trace), and the stall detector prints it.

StepWatch compiles the real LoopStepWatch.h on the host. StallLine checks the source wiring that the host cannot run:
connProbe() feeds the watch before its trace-only early return, and the stall line prints the step.
Run: python tests/test_loop_step_watch.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402

FW = base.REPO / "src" / "OTGW-firmware"
HDR = FW / "LoopStepWatch.h"

MAIN = r"""
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "@HDR@"
int main(int argc, char** argv) {
  // args: pairs <tag> <ms> ...; a final "read <ms>" reads the slowest step at that time (loop top tag = 1)
  LoopStepWatch w;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "read")) {
      uint8_t f, t; uint32_t ms;
      loopStepSlowest(w, (uint32_t)strtoul(argv[i + 1], 0, 10), 1, &f, &t, &ms);
      printf("%u %u %lu\n", f, t, (unsigned long)ms);
      return 0;
    }
    loopStepProbe(w, (uint8_t)atoi(argv[i]), (uint32_t)strtoul(argv[i + 1], 0, 10), 1);
  }
  return 0;
}
"""


class StepWatch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not HDR.exists():
            raise AssertionError("LoopStepWatch.h missing (code before TASK-1989)")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "w.cpp"
        cpp.write_text(MAIN.replace("@HDR@", HDR.as_posix()), encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / "w.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_w(self, *args):
        out = subprocess.run([str(self.exe)] + [str(a) for a in args], capture_output=True, text=True, check=True).stdout
        return tuple(int(x) for x in out.split())

    def test_names_the_slowest_step_of_the_pass(self):
        # loop top 1 at 0, timers 2 at 5, discovery 3 at 6, SAT 4 at 486 (480 ms in the SAT step), app 5 at 490
        self.assertEqual(self.run_w(1, 0, 2, 5, 3, 6, 4, 486, 5, 490, "read", 495), (3, 4, 480))

    def test_the_tail_to_the_next_loop_top_counts(self):
        # the last probe of the pass at 10, the next loop top reads at 600: the open step is the slowest
        self.assertEqual(self.run_w(1, 0, 2, 4, 7, 10, "read", 600), (7, 1, 590))

    def test_a_new_pass_starts_clean(self):
        # a slow first pass (300 ms in step 2->3), then a fast second pass whose slowest step is 1->2 (2 ms): the 300 ms is gone
        self.assertEqual(self.run_w(1, 0, 2, 1, 3, 301, 7, 302, 1, 303, 2, 305, 3, 306, 7, 307, "read", 308), (1, 2, 2))

    def test_millis_wrap(self):
        self.assertEqual(self.run_w(1, 4294967000, 2, 4294967100, 3, 200, "read", 210), (2, 3, 396))


class StallLine(unittest.TestCase):
    def setUp(self):
        self.mqtt = (FW / "MQTTstuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.main = (FW / "OTGW-firmware.ino").read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_connprobe_feeds_the_watch_before_the_trace_gate(self):
        body = self.mqtt[self.mqtt.index("void connProbe(uint8_t tag) {"):]
        body = body[:body.index("\n}\n")]
        self.assertIn("loopStepProbe(", body)
        self.assertLess(body.index("loopStepProbe("), body.index("if (!sConnTraceOn) return;"))

    def test_stall_line_prints_the_slowest_step(self):
        loop = self.main[self.main.index("void loop()"):]
        stall = loop.index("[loop-stall]")
        self.assertIn("loopSlowestStep(", loop[:stall + 300])
        self.assertLess(stall, loop.index("connProbe(CPROBE_LOOP_TOP);"))   # read before the new pass resets the record


if __name__ == "__main__":
    unittest.main(verbosity=2)
