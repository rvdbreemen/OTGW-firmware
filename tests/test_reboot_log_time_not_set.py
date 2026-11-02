#!/usr/bin/env python3
"""TASK-1452 (port of 1.x TASK-1253): a reboot logged before NTP reads 'time not set', not a 1970/2106 date.

Before the fix updateRebootLog() formatted time(nullptr) unconditionally. Before NTP the ESP32 clock holds
seconds since boot, so the entry read 1970-01-01 (or 2106 on the 1.x line).

The code under test is cut out of the real src/OTGW-firmware/helperStuff.ino (rebootLogClockValid() and the
reader isRebootLogSummaryLine() .. readLatestCrashLog() / copyOlderRebootLogLines()) and compiled on the host
with the stubs of tests/test_crashlog_coredump.py. OTGW_HELPER_INO points at another copy for old-vs-fix runs.

Run: python tests/test_reboot_log_time_not_set.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402
from test_crashlog_coredump import STUBS  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
HELPER_INO = Path(os.environ.get("OTGW_HELPER_INO", REPO / "src" / "OTGW-firmware" / "helperStuff.ino"))
FW_H = REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h"

MAIN = r"""
int main() {
  // Clock range: seconds since boot (pre-NTP), the 1970 epoch, the 2106 wrap, and a real date.
  const long long probes[] = {0LL, 300LL, 86400LL * 3, 1793546515LL, 4294967295LL};
  for (long long p : probes) printf("valid|%lld|%d\n", p, rebootLogClockValid((int64_t)p) ? 1 : 0);

  // The crash-log reader on a file whose newest entry was written before NTP.
  char summary[160], details[160];
  g_file = "time not set - reboot cause: Task watchdog (6) \r\n"
           "Core dump: loopTask pc=0x42012345 elf=abcdef01 bt=0x42000000\r\n"
           "2026-11-01 08:03:22 - reboot cause: Software reset (3) \r\n";
  bool ok = readLatestCrashLog(summary, sizeof(summary), details, sizeof(details));
  printf("parse|%d|%s|%s\n", ok ? 1 : 0, summary, details);
  return 0;
}
"""


class RebootLogTimeNotSet(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.helper = HELPER_INO.read_text(encoding="utf-8")
        a = cls.helper.find("static bool rebootLogClockValid")
        b = cls.helper.find("bool updateRebootLog(", a + 1)
        if a < 0 or b < 0:
            raise AssertionError("rebootLogClockValid() not found in helperStuff.ino: no pre-NTP guard (TASK-1452)")
        fw = FW_H.read_text(encoding="utf-8")
        m = re.search(r"^#define\s+REBOOT_LOG_LINE_MAX\s+(\d+)", fw, re.M)
        defs = f"#define REBOOT_LOG_LINE_MAX {m.group(1)}\n" if m else ""
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(defs + STUBS + cls.helper[a:b] + "\n" + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        print(cls.out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_clock_range(self):
        got = {int(l.split("|")[1]): int(l.split("|")[2]) for l in self.out.splitlines() if l.startswith("valid|")}
        self.assertEqual(got, {0: 0, 300: 0, 259200: 0, 1793546515: 1, 4294967295: 0})

    def test_reader_finds_a_time_not_set_entry(self):
        row = next(l for l in self.out.splitlines() if l.startswith("parse|")).split("|")
        self.assertEqual(row[1], "1")
        self.assertEqual(row[2], "time not set - reboot cause: Task watchdog (6) ")
        self.assertTrue(row[3].startswith("Core dump: loopTask"), row[3])

    def test_writer_uses_the_guard(self):
        body = self.helper[self.helper.find("bool updateRebootLog("):]
        body = body[:body.find("\n}\n")]
        self.assertRegex(body, r"if \(rebootLogClockValid\(now\)\)")
        self.assertIn('PSTR("time not set - reboot cause: %s (%x) %s\\r\\n")', body)
        self.assertNotIn("forUnixSeconds64(time(nullptr)", body)


if __name__ == "__main__":
    unittest.main()
