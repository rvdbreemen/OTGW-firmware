"""scripts/assert-head.sh: a short and a long form of the same commit pass, a different commit fails (TASK-3095).

ASSERT_HEAD_SCRIPT=<path> runs the same tests against another copy of the script (used to show red on a naive
string compare). Run: python tests/test_assert_head.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = Path(os.environ.get("ASSERT_HEAD_SCRIPT") or ROOT / "scripts" / "assert-head.sh")
GIT = ["git", "-c", "user.email=t@example.invalid", "-c", "user.name=t", "-c", "commit.gpgsign=false"]


def run(cmd, cwd):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


class AssertHead(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.repo = cls.tmp.name
        run(["git", "init", "-q"], cls.repo)
        for n in ("a", "b"):
            Path(cls.repo, n).write_text(n)
            run(["git", "add", n], cls.repo)
            run(GIT + ["commit", "-q", "-m", n], cls.repo)
        cls.head = run(["git", "rev-parse", "HEAD"], cls.repo).stdout.strip()
        cls.prev = run(["git", "rev-parse", "HEAD~1"], cls.repo).stdout.strip()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check(self, arg):
        return run(["sh", str(SCRIPT), arg], self.repo)

    def test_full_hash_passes(self):
        self.assertEqual(self.check(self.head).returncode, 0)

    def test_seven_and_nine_char_forms_pass(self):
        for n in (7, 9):
            r = self.check(self.head[:n])
            self.assertEqual(r.returncode, 0, f"{n}-char form failed: {r.stderr}")

    def test_branch_and_head_names_pass(self):
        self.assertEqual(self.check("HEAD").returncode, 0)

    def test_different_commit_fails_and_prints_both(self):
        for arg in (self.prev, self.prev[:7]):
            r = self.check(arg)
            self.assertEqual(r.returncode, 1)
            self.assertIn(self.prev, r.stderr)
            self.assertIn(self.head, r.stderr)

    def test_unknown_hash_fails(self):
        r = self.check("deadbeefdeadbeef")
        self.assertEqual(r.returncode, 1)

    def test_empty_argument_fails(self):
        self.assertEqual(self.check("").returncode, 1)


if __name__ == "__main__":
    unittest.main()
