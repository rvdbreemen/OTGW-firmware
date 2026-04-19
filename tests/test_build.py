#!/usr/bin/env python3
"""
Smoke tests for build.py. Verifies that the release-integrity flags from
TASK-287 / TASK-289 / TASK-290 / TASK-291 still parse and show up in --help.
This catches regressions where a flag is silently renamed or dropped by a
refactor, which would turn the corresponding release gate into a no-op.

Run: python tests/test_build.py
"""

import io
import subprocess
import sys
import unittest
from pathlib import Path

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


if __name__ == "__main__":
    unittest.main(verbosity=2)
