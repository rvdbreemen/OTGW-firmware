"""TASK-1335, TASK-1456: build.py caps PlatformIO compiler jobs at 4 (maintainer 2026-10-29).

Run: python tests/test_build_jobs.py
"""
import os
import sys
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import build  # noqa: E402 - after the sys.path change


class PioJobs(unittest.TestCase):
    def jobs(self, env_value=None, cpus=16):
        env = {k: v for k, v in os.environ.items() if k != "OTGW_BUILD_JOBS"}
        if env_value is not None:
            env["OTGW_BUILD_JOBS"] = env_value
        with mock.patch.dict(os.environ, env, clear=True), \
             mock.patch.object(build.os, "cpu_count", return_value=cpus), \
             mock.patch.object(build, "print_info"):
            return build.pio_jobs()

    def test_default_capped_at_4(self):
        self.assertEqual(self.jobs(cpus=16), 4)

    def test_default_follows_small_machine(self):
        self.assertEqual(self.jobs(cpus=2), 2)

    def test_unknown_cpu_count(self):
        self.assertEqual(self.jobs(cpus=None), 1)

    def test_override_below_the_cap(self):
        self.assertEqual(self.jobs("2", cpus=16), 2)

    def test_override_never_above_4(self):
        self.assertEqual(self.jobs("12", cpus=16), 4)
        self.assertEqual(self.jobs("8", cpus=32), 4)

    def test_invalid_override_falls_back(self):
        for bad in ("0", "-3", "eight", "1.5"):
            self.assertEqual(self.jobs(bad, cpus=16), 4, bad)

    def test_every_platformio_run_passes_jobs(self):
        src = (REPO_ROOT / "build.py").read_text(encoding="utf-8")
        runs = [line for line in src.splitlines()
                if '"platformio", "run"' in line and "run_command" in line]
        self.assertEqual(len(runs), 3, runs)
        for i, line in enumerate(src.splitlines()):
            if '"platformio", "run"' in line and "run_command" in line:
                window = "\n".join(src.splitlines()[i:i + 2])
                self.assertIn('"--jobs", str(pio_jobs())', window, line)


if __name__ == "__main__":
    unittest.main(verbosity=2)
