"""TASK-1872: scripts/run_tests.py classify_py() decides on the exit code plus unittest's summary line.

test/host/test_stream_real_cbuf.py exits 0 and prints "RESULT: PASS", but its built-in mutant
section (presize disabled, must fail) prints the C++ harness line "FAILED (4 failure(s))". The old
classifier marked any output line starting with "FAILED" as a failure, so that file was reported
FAILED on every run. These cases call the REAL classify_py() from scripts/run_tests.py.
"""
import importlib.util
import unittest
from pathlib import Path

RUNNER = Path(__file__).resolve().parent.parent / "scripts" / "run_tests.py"
spec = importlib.util.spec_from_file_location("run_tests_under_test", RUNNER)
run_tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run_tests)
classify_py = run_tests.classify_py

# Trimmed from a real run of test/host/test_stream_real_cbuf.py (2026-11-12, rc 0).
CBUF_STYLE_OUTPUT = """\
== GREEN: the presize the firmware chooses (restChooseStreamSize, healthy heap) ==
ALL PASSED (0 failure(s))
RESULT: PASS

== presize disabled (must fail) ==
FAIL D:\\x\\test_stream_real_cbuf.cpp:154  creates == 50  one ring buffer per response (19900 over 50 polls)
FAILED (4 failure(s))
PASS: green with the presize, red without it
"""


class ClassifyPy(unittest.TestCase):
    def test_in_test_mutant_failed_line_with_rc0_is_pass(self):
        self.assertEqual(classify_py(0, CBUF_STYLE_OUTPUT), "PASS")

    def test_nonzero_exit_is_failed(self):
        self.assertEqual(classify_py(1, CBUF_STYLE_OUTPUT), "FAILED")
        self.assertEqual(classify_py(1, "Ran 3 tests in 0.1s\n\nOK\n"), "FAILED")

    def test_timeout(self):
        self.assertEqual(classify_py(None, ""), "TIMEOUT")

    def test_unittest_failure_summary_with_rc0_is_failed(self):
        # unittest.main(exit=False) or a wrapper that swallows the exit code
        out = "F..\n" + "-" * 70 + "\nRan 3 tests in 0.010s\n\nFAILED (failures=1)\n"
        self.assertEqual(classify_py(0, out), "FAILED")
        self.assertEqual(classify_py(0, "Ran 2 tests in 0.0s\n\nFAILED (errors=2)\n"), "FAILED")
        self.assertEqual(classify_py(0, "Ran 2 tests in 0.0s\n\nFAILED (failures=1, skipped=1)\n"), "FAILED")

    def test_unittest_ok_is_pass(self):
        self.assertEqual(classify_py(0, "Ran 5 tests in 0.2s\n\nOK\n"), "PASS")

    def test_zero_tests_is_skip(self):
        self.assertEqual(classify_py(0, "Ran 0 tests in 0.0s\n\nOK\n"), "SKIP")

    def test_skip_marker_is_skip(self):
        self.assertEqual(classify_py(0, "SKIP: no C++ compiler on this host\n"), "SKIP")


if __name__ == "__main__":
    unittest.main()
