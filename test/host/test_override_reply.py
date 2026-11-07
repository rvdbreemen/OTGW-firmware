#!/usr/bin/env python3
"""Runs the OT-Direct override-reply host harness (test_override_reply.cpp) for every suite it holds,
so scripts/run_tests.py covers it:

    1178  reply to a forwarded thermostat frame
    1177  boiler cache and OEM data-ids 128-255
    1184  boiler cache and UNKNOWN-DATA-ID replies
    1684  master mode without the OT-Thing controller: no MsgID 1 write and CH enable off without
          SAT or a CS override, gateway mode unchanged

Each suite is the working tree against the code sliced from src/OTGW-firmware/OTDirect.ino. The
old-vs-fix run is a manual step against a revision, because it needs a commit that still has the code
the suite targets:

    powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_override_reply.ps1 -Suite 1684 -OldVsFix -OldRev <rev before TASK-1684>

Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import subprocess
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
PS1 = HERE / "build_and_run_override_reply.ps1"


class OverrideReplyHarness(unittest.TestCase):
    def run_suite(self, suite):
        p = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(PS1), "-Suite", suite],
                           capture_output=True, text=True, timeout=600)
        out = p.stdout + p.stderr
        self.assertEqual(p.returncode, 0, f"suite {suite} exit {p.returncode}\n{out[-4000:]}")
        self.assertIn("RESULT: PASS", out)
        self.assertRegex(out, r"SUMMARY cases=\d+ passed=\d+ failed=none")

    def test_suite_1178(self): self.run_suite("1178")
    def test_suite_1177(self): self.run_suite("1177")
    def test_suite_1184(self): self.run_suite("1184")
    def test_suite_1684(self): self.run_suite("1684")


if __name__ == "__main__":
    if sys.platform != "win32":
        print("the host harness needs MSVC (Windows)")
        sys.exit(0)
    unittest.main(verbosity=2)
