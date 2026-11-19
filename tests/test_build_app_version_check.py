#!/usr/bin/env python3
"""TASK-1976: build.py refuses an app image whose compiled version string is not the one it stamped.

The beta.318 bench build carried 2.0.0-beta.318+1233ba9 inside the app image while the zip, the .ino.bin name and
/version.hash said 56fb6db: version.h was reverted to its committed stamp (a `git checkout -- version.h` during the
build) after build.py stamped it and before pio compiled OTGW-firmware.ino.cpp. build.py now reads the renamed
app image after the build and exits when the exact semver it built under is missing.

    python tests/test_build_app_version_check.py

Cases: the app image holds the semver -> ok; it holds the semver of another githash (the 318 case) -> refused and the
strings it does hold are named; no app image (filesystem-only build) -> ok, nothing to check.
"""
import importlib.util
import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_PY = REPO_ROOT / "build.py"

GOOD = "2.0.0-beta.318+56fb6db"
STALE = "2.0.0-beta.318+1233ba9"


def load_build():
    spec = importlib.util.spec_from_file_location("build_under_test_1976", BUILD_PY)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class TestAppVersionCheck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = load_build()
        cls.target = "esp32-combo" if "esp32-combo" in cls.build.TARGETS else next(iter(cls.build.TARGETS))
        cls.slug = cls.build.asset_slug(cls.target)

    def _project(self, tmp, app_string):
        d = Path(tmp)
        (d / "build").mkdir()
        if app_string is not None:
            name = f"OTGW-firmware-{self.slug}-{GOOD}.ino.bin"
            (d / "build" / name).write_bytes(b"\xe9\x00" + b"\x00" * 64 + app_string.encode() + b"\x00" * 64)
        return d

    def _run(self, project):
        out = io.StringIO()
        with redirect_stdout(out), redirect_stderr(out):
            ok = self.build.verify_app_version_string(project, GOOD, self.target)
        return ok, out.getvalue()

    def test_matching_string_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            ok, out = self._run(self._project(tmp, GOOD))
            self.assertTrue(ok)
            self.assertIn("App version check", out)   # the build log shows the check ran

    def test_stale_githash_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            ok, out = self._run(self._project(tmp, STALE))
            self.assertFalse(ok)
            self.assertIn(STALE, out)   # names what the image does carry

    def test_no_app_image_is_nothing_to_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            ok, _ = self._run(self._project(tmp, None))
            self.assertTrue(ok)

    def test_main_flow_calls_the_check_after_the_rename(self):
        src = BUILD_PY.read_text(encoding="utf-8")
        rename = src.index("rename_build_artifacts(project_dir, semver, target)\n", src.index("def main"))
        check = src.find("verify_app_version_string(project_dir, semver, target)", rename)
        self.assertGreater(check, rename, "the check must run in main() after the artifacts carry their semver name")


if __name__ == "__main__":
    unittest.main(verbosity=2)
