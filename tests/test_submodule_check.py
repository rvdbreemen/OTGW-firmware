#!/usr/bin/env python3
"""
TASK-1725: a fresh `git worktree add` leaves the submodule directories empty, and the build and the
C++ host harnesses then failed on a missing OpenTherm.h instead of saying what was wrong.
scripts/submodule_check.py names the missing submodule and the command that fixes it; build.py and
scripts/run_tests.py call it before they do any work.

  a  a populated tree reports nothing missing
  b  an empty submodule directory is reported, with its path and the git command that fixes it
  c  only the submodules asked for are checked (the build does not need other-projects)
  d  run_tests.py and build.py call the check before their work (source order)

Run: python tests/test_submodule_check.py
"""

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import submodule_check as sc  # noqa: E402


def make_tree(base: Path, populated):
    for name, (path, marker) in sc.SUBMODULES.items():
        (base / path).mkdir(parents=True, exist_ok=True)
        if name in populated:
            m = base / path / marker
            m.parent.mkdir(parents=True, exist_ok=True)
            m.write_text("x", encoding="utf-8")


class SubmoduleCheck(unittest.TestCase):
    def test_a_populated_tree_has_nothing_missing(self):
        with tempfile.TemporaryDirectory() as t:
            make_tree(Path(t), set(sc.SUBMODULES))
            self.assertEqual(sc.missing(Path(t), sc.SUBMODULES), [])

    def test_b_empty_dir_is_named_with_the_fix(self):
        with tempfile.TemporaryDirectory() as t:
            make_tree(Path(t), {"SimpleTelnet", "other-projects"})
            self.assertEqual(sc.missing(Path(t), sc.SUBMODULES), ["OpenTherm"])
            msg = sc.message(["OpenTherm"])
            self.assertIn("src/libraries/OpenTherm", msg)
            self.assertIn("git submodule update --init -- src/libraries/OpenTherm", msg)

    def test_c_only_the_requested_set_is_checked(self):
        with tempfile.TemporaryDirectory() as t:
            make_tree(Path(t), {"OpenTherm", "SimpleTelnet"})
            self.assertEqual(sc.missing(Path(t), sc.BUILD), [])
            self.assertEqual(sc.missing(Path(t), sc.SUBMODULES), ["other-projects"])

    def test_d_tools_check_before_their_work(self):
        rt = (ROOT / "scripts" / "run_tests.py").read_text(encoding="utf-8")
        self.assertLess(rt.index("submodule_check"), rt.index("py_tests = sorted("))
        bp = (ROOT / "build.py").read_text(encoding="utf-8")
        main = bp[bp.index("def main():"):]
        self.assertLess(main.index("submodule_check"), main.index("acquire_build_lock("))


if __name__ == "__main__":
    unittest.main()
