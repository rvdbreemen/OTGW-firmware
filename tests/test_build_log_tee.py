"""TASK-1894: build.py writes its own build log, child tools included.

Redirecting build.bat from an agent's Bash or PowerShell tool gave 0-byte files, so the success lines the push
check asks for ('Successfully created ESP32S3 image', 'Build completed successfully') could not be shown. build.py
now tees its own stdout/stderr into build/logs/build-<time>.log (copied to build/logs/latest.log at the end) and
streams child processes through Python, because a child with stdout=None writes to file descriptor 1 directly.

The test uses the REAL helpers of build.py (start_build_log, run_command, run_streamed, stop_build_log) with a
child Python process that prints the esptool success line, so no firmware build is needed. On a build.py without
these helpers the test fails (red).
"""
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import build  # noqa: E402

CHILD_OUT = "Successfully created ESP32S3 image."
CHILD_ERR = "a warning on stderr"


class BuildLogTee(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.console = io.StringIO()
        self.saved = (sys.stdout, sys.stderr)
        sys.stdout = self.console
        sys.stderr = self.console

    def tearDown(self):
        build.stop_build_log(getattr(self, "path", None)) if getattr(build, "_BUILD_LOG", None) else None
        sys.stdout, sys.stderr = self.saved
        self.tmp.cleanup()

    def test_child_output_and_own_prints_reach_the_log_and_the_console(self):
        self.path = build.start_build_log(self.tmp.name)
        child = [sys.executable, "-c", f"import sys; print({CHILD_OUT!r}); print({CHILD_ERR!r}, file=sys.stderr)"]
        build.run_command(child)                       # the streamed path (show_output=True, no capture)
        print("  Build completed successfully!")
        build.stop_build_log(self.path)
        log = Path(self.path).read_text(encoding="utf-8")
        for line in (CHILD_OUT, CHILD_ERR, "Build completed successfully!"):
            self.assertIn(line, log, f"{line!r} missing from the build log")
            self.assertIn(line, self.console.getvalue(), f"{line!r} missing from the console")
        latest = Path(self.tmp.name) / "build" / "logs" / "latest.log"
        self.assertEqual(latest.read_text(encoding="utf-8"), log, "latest.log is a copy of the run's log")

    def test_a_failing_child_still_raises_and_its_output_is_logged(self):
        self.path = build.start_build_log(self.tmp.name)
        child = [sys.executable, "-c", "import sys; print('pio says: error 7'); sys.exit(3)"]
        rc = build.run_streamed(child)
        self.assertEqual(rc, 3)
        with self.assertRaises(SystemExit):           # run_command(check=True) reports and exits as before
            build.run_command(child)
        build.stop_build_log(self.path)
        self.assertIn("pio says: error 7", Path(self.path).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
