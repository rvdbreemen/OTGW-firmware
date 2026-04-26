#!/usr/bin/env python3
"""
Smoke tests for build.py. Verifies that the release-integrity flags from
TASK-287 / TASK-289 / TASK-290 / TASK-291 still parse and show up in --help.
This catches regressions where a flag is silently renamed or dropped by a
refactor, which would turn the corresponding release gate into a no-op.

TASK-337 (added 2026-04-26): regression tests for the artifact-verification
fail-fast guards that prevent silent build failures (notably the pio
"Python version must be between 3.10 and 3.13" failure mode where pio
prints an error and exits 0 without invoking the actual builder).

Run: python tests/test_build.py
"""

import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_PY = REPO_ROOT / "build.py"


class TestBuildHelpFlags(unittest.TestCase):
    """Each flag below gates a release-integrity or reproducibility concern.
    If any of these disappear silently, that gate is broken."""

    EXPECTED_FLAGS = [
        "--verify-image",     # TASK-290: flash image header sanity check
        "--verify-flash",     # TASK-290: read-back verify after write
        "--reproducible",     # TASK-289: deterministic builds (ffile-prefix-map)
        "--ccache",           # TASK-287: ccache-enabled builds
        "--manifest",         # TASK-287: produce artifact manifest
        "--firmware",         # Long-standing: firmware-only subset
        "--clean",            # Long-standing: clean build
    ]

    @classmethod
    def setUpClass(cls):
        cls.help_text = subprocess.run(
            [sys.executable, str(BUILD_PY), "--help"],
            capture_output=True, text=True, timeout=30,
            cwd=str(REPO_ROOT),
        )

    def test_help_exits_zero(self):
        self.assertEqual(self.help_text.returncode, 0,
                         f"build.py --help exited {self.help_text.returncode}; stderr:\n{self.help_text.stderr}")

    def test_help_output_non_empty(self):
        self.assertTrue(self.help_text.stdout.strip(),
                        "build.py --help produced no stdout")

    def test_all_expected_flags_listed(self):
        missing = [flag for flag in self.EXPECTED_FLAGS if flag not in self.help_text.stdout]
        self.assertFalse(
            missing,
            f"build.py --help is missing expected flags: {missing}\n\nFull help text:\n{self.help_text.stdout}",
        )


class TestBuildImportable(unittest.TestCase):
    """Catch syntax errors and top-level import failures early."""

    def test_build_py_parses(self):
        result = subprocess.run(
            [sys.executable, "-c", "import ast; ast.parse(open(r'{}', encoding='utf-8').read())".format(BUILD_PY)],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0,
                         f"build.py does not parse cleanly:\n{result.stderr}")


class TestArtifactVerificationFailFast(unittest.TestCase):
    """TASK-337 regression: build.py must fail fast when a build step
    returns success without producing the expected artifact. The motivating
    failure mode is pio's pre-flight Python version rejection, where pio
    prints "Python version must be between 3.10 and 3.13" and exits 0
    without invoking the actual builder. Pre-fix, build.py trusted the
    exit code, proceeded through its pipeline, and printed
    'Build completed successfully!' on a fake build."""

    @classmethod
    def setUpClass(cls):
        # Make build.py importable as a module so we can exercise the
        # verify_artifact_exists helper and build_firmware_pio in-process.
        if str(REPO_ROOT) not in sys.path:
            sys.path.insert(0, str(REPO_ROOT))
        import build  # noqa: E402 - intentional late import after sys.path mutation
        cls.build = build

    def test_verify_artifact_exists_passes_when_file_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "firmware.bin"
            artifact.write_bytes(b"fake firmware")
            result = self.build.verify_artifact_exists(artifact, "test step")
            self.assertEqual(result, [artifact])

    def test_verify_artifact_exists_exits_when_file_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            missing = Path(tmp) / "firmware.bin"
            with self.assertRaises(SystemExit) as ctx:
                self.build.verify_artifact_exists(missing, "test step")
            self.assertEqual(
                ctx.exception.code, 2,
                "verify_artifact_exists must exit 2 (artifact verification "
                "failure) when the expected file is missing",
            )

    def test_verify_artifact_exists_glob_returns_matches(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "OTGW-firmware.ino.bin").write_bytes(b"x")
            result = self.build.verify_artifact_exists(
                Path(tmp), "test step", glob_pattern="**/*.ino.bin"
            )
            self.assertEqual(len(result), 1)
            self.assertTrue(result[0].name.endswith(".ino.bin"))

    def test_verify_artifact_exists_glob_exits_when_no_matches(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit) as ctx:
                self.build.verify_artifact_exists(
                    Path(tmp), "test step", glob_pattern="**/*.ino.bin"
                )
            self.assertEqual(ctx.exception.code, 2)

    def test_build_firmware_pio_fails_fast_on_silent_pio(self):
        """Regression for TASK-337: build_firmware_pio must sys.exit when
        pio returns success without producing firmware.bin (the Python
        version rejection failure mode that motivated this task)."""
        with tempfile.TemporaryDirectory() as tmp:
            project_dir = Path(tmp) / "project"
            project_dir.mkdir()

            # Mock subprocess.run inside the build module to simulate pio's
            # "Python version must be between 3.10 and 3.13" path: prints
            # to stdout, exits 0, but does not create the expected
            # .pio/build/<env>/firmware.bin.
            fake_completed = mock.MagicMock()
            fake_completed.returncode = 0
            fake_completed.stdout = (
                "Python version must be between 3.10 and 3.13\n"
            )
            fake_completed.stderr = ""

            target = next(iter(self.build.PIO_ENV_MAP))

            with mock.patch.object(
                self.build.subprocess, "run", return_value=fake_completed
            ):
                with self.assertRaises(SystemExit) as ctx:
                    self.build.build_firmware_pio(project_dir, target)
            self.assertEqual(
                ctx.exception.code, 2,
                "build_firmware_pio must exit 2 when pio returns success "
                "without producing firmware.bin",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
