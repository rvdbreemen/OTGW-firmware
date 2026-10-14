#!/usr/bin/env python3
"""
TASK-1224: tests for the LittleFS staging step in build.py.

The filesystem image is built from a staging copy of src/OTGW-firmware/data/.
These tests check the invariants that make that safe:
- the source directory is never modified;
- files that are not browser assets reach the image byte-for-byte;
- browser assets (.js/.css) get smaller, and v2.css is left out;
- a minifier that returns nothing fails the step instead of shipping it.

TASK-1225: a guard against CSS comments that close early. A comment text with
a star-slash in it ends the comment; the leftover text then swallows the next
declaration (that dropped --hot from ds-tokens.css).

Run: python tests/test_build_staging.py (needs rjsmin + rcssmin from
requirements-build.txt; the minify tests are skipped without them).
"""

import hashlib
import io
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "src" / "OTGW-firmware" / "data"
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import build  # noqa: E402 - after the sys.path change

try:
    import rcssmin  # noqa: F401
    import rjsmin  # noqa: F401
    HAVE_MINIFIERS = True
except ImportError:
    HAVE_MINIFIERS = False


def tree_hashes(root):
    return {
        p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(root.rglob("*")) if p.is_file()
    }


@unittest.skipUnless(HAVE_MINIFIERS, "rjsmin/rcssmin not installed (requirements-build.txt)")
class TestStagingOnRealData(unittest.TestCase):
    """Stage the real data/ directory into a temp dir."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.staging = Path(cls.tmp.name) / "staging"
        cls.source_before = tree_hashes(DATA_DIR)
        cls.report = build.stage_data_dir(DATA_DIR, cls.staging, minify=True)
        cls.source_after = tree_hashes(DATA_DIR)
        cls.staged = tree_hashes(cls.staging)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_source_untouched(self):
        self.assertEqual(self.source_before, self.source_after)

    def test_excluded_files_not_staged(self):
        for name in build.STAGING_EXCLUDE:
            self.assertNotIn(name, self.staged)

    def test_nothing_else_dropped(self):
        expected = set(self.source_before) - set(build.STAGING_EXCLUDE)
        self.assertEqual(expected, set(self.staged))

    def test_non_browser_files_byte_identical(self):
        for rel, digest in self.staged.items():
            if Path(rel).suffix not in build.MINIFY_SUFFIXES:
                self.assertEqual(digest, self.source_before[rel], f"{rel} changed in staging")

    def test_browser_assets_minified_and_smaller(self):
        minified = {rel for rel, _, _ in self.report}
        expected = {rel for rel in self.staged if Path(rel).suffix in build.MINIFY_SUFFIXES}
        self.assertEqual(minified, expected)
        for rel, before, after in self.report:
            self.assertGreater(after, 0, rel)
            self.assertLess(after, before, f"{rel} did not shrink")

    def test_minified_css_braces_balanced(self):
        for rel, _, _ in self.report:
            if rel.endswith(".css"):
                text = (self.staging / rel).read_text(encoding="utf-8")
                self.assertEqual(text.count("{"), text.count("}"), rel)


class TestStagingWithoutMinify(unittest.TestCase):
    def test_no_minify_copies_everything_but_excluded(self):
        with tempfile.TemporaryDirectory() as tmp:
            staging = Path(tmp) / "staging"
            report = build.stage_data_dir(DATA_DIR, staging, minify=False)
            self.assertEqual(report, [])
            source = tree_hashes(DATA_DIR)
            for name in build.STAGING_EXCLUDE:
                source.pop(name, None)
            self.assertEqual(source, tree_hashes(staging))


class TestStagingFailsLoud(unittest.TestCase):
    def test_empty_minifier_output_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "src"
            src.mkdir()
            (src / "app.js").write_text("var a = 1;\n", encoding="utf-8")
            with mock.patch.object(build, "_minify_asset", return_value=b""):
                with self.assertRaises(RuntimeError):
                    build.stage_data_dir(src, Path(tmp) / "staging", minify=True)

    def test_prepare_exits_nonzero_on_minify_failure(self):
        with mock.patch.object(build, "stage_data_dir", side_effect=RuntimeError("boom")):
            with self.assertRaises(SystemExit) as ctx:
                build.prepare_filesystem_staging(REPO_ROOT, "esp32-combo", minify=True)
            self.assertEqual(ctx.exception.code, 1)

    def test_prepare_exits_nonzero_when_minifier_missing(self):
        with mock.patch.object(build, "stage_data_dir", side_effect=ImportError("No module named 'rjsmin'")):
            with self.assertRaises(SystemExit) as ctx:
                build.prepare_filesystem_staging(REPO_ROOT, "esp32-combo", minify=True)
            self.assertEqual(ctx.exception.code, 1)


class TestCssCommentsCloseCleanly(unittest.TestCase):
    """TASK-1225: after removing CSS comments the way a browser does, no line
    may hold an odd number of double quotes. An odd count means a comment
    closed early and left a broken string behind."""

    def test_no_dangling_quotes_after_comment_strip(self):
        problems = []
        for css in sorted(DATA_DIR.rglob("*.css")):
            text = re.sub(r"/\*.*?\*/", "", css.read_text(encoding="utf-8"), flags=re.S)
            for lineno, line in enumerate(text.splitlines(), 1):
                if line.count('"') % 2:
                    problems.append(f"{css.name}: stripped line {lineno}: {line.strip()[:80]}")
        self.assertEqual(problems, [], "\n".join(problems))


if __name__ == "__main__":
    unittest.main(verbosity=2)
