#!/usr/bin/env python3
"""TASK-3042: the pre-commit task-id check fails on a second backlog file with the same id, passes otherwise.

Runs the REAL scripts/check_backlog_task_ids.py and the REAL .githooks/pre-commit section in a throwaway git repo.
Red on a staged file that duplicates an id (also an archive twin), green on a unique id, on an old duplicate that the
commit does not touch, and after the duplicate is renamed. A mutated copy of the script that ignores the archive
folder must be caught by the archive-twin case.
Run: python tests/test_backlog_task_id_check.py
"""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = REPO / "scripts" / "check_backlog_task_ids.py"


def task(root, d, name, tid):
    p = Path(root) / d
    p.mkdir(parents=True, exist_ok=True)
    (p / name).write_text(f"---\nid: {tid}\ntitle: x\n---\n", encoding="utf-8")
    return f"{d}/{name}"


def run(script, root, staged):
    return subprocess.run([sys.executable, str(script), "--root", str(root), "--staged", *staged],
                          capture_output=True, text=True)


class TaskIdCheck(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_unique_id_passes(self):
        a = task(self.tmp, "backlog/tasks", "task-1 - a.md", "TASK-1")
        task(self.tmp, "backlog/archive/tasks", "task-2 - b.md", "TASK-2")
        self.assertEqual(run(SCRIPT, self.tmp, [a]).returncode, 0)

    def test_staged_duplicate_fails(self):
        a = task(self.tmp, "backlog/tasks", "task-7 - a.md", "TASK-7")
        task(self.tmp, "backlog/tasks", "task-7 - b.md", "TASK-7")
        r = run(SCRIPT, self.tmp, [a])
        self.assertEqual(r.returncode, 1)
        self.assertIn("TASK-7", r.stderr)

    def test_archive_twin_fails(self):
        a = task(self.tmp, "backlog/tasks", "task-9 - a.md", "TASK-9")
        task(self.tmp, "backlog/archive/tasks", "task-9 - old.md", "TASK-9")
        self.assertEqual(run(SCRIPT, self.tmp, [a]).returncode, 1)

    def test_old_duplicate_not_touched_passes(self):
        task(self.tmp, "backlog/tasks", "task-5 - a.md", "TASK-5")
        task(self.tmp, "backlog/archive/tasks", "task-5 - b.md", "TASK-5")
        other = task(self.tmp, "backlog/tasks", "task-6 - c.md", "TASK-6")
        self.assertEqual(run(SCRIPT, self.tmp, [other]).returncode, 0)

    def test_subtask_ids_are_distinct(self):
        a = task(self.tmp, "backlog/tasks", "task-3 - a.md", "TASK-3")
        task(self.tmp, "backlog/tasks", "task-3.1 - b.md", "TASK-3.1")
        self.assertEqual(run(SCRIPT, self.tmp, [a]).returncode, 0)

    def test_mutant_ignoring_archive_is_caught(self):
        mutant = Path(self.tmp) / "mutant.py"
        mutant.write_text(SCRIPT.read_text(encoding="utf-8").replace('"backlog/archive/tasks", ', ""), encoding="utf-8")
        a = task(self.tmp, "backlog/tasks", "task-9 - a.md", "TASK-9")
        task(self.tmp, "backlog/archive/tasks", "task-9 - old.md", "TASK-9")
        self.assertEqual(run(SCRIPT, self.tmp, [a]).returncode, 1)
        self.assertEqual(run(mutant, self.tmp, [a]).returncode, 0, "mutant must differ, else the test checks nothing")

    def test_real_hook_blocks_commit(self):
        repo = Path(self.tmp) / "r"
        (repo / "scripts").mkdir(parents=True)
        (repo / ".githooks").mkdir()
        shutil.copy(SCRIPT, repo / "scripts")
        shutil.copy(REPO / ".githooks" / "pre-commit", repo / ".githooks")
        g = lambda *a: subprocess.run(["git", "-C", str(repo), *a], capture_output=True, text=True)
        g("init", "-q")
        g("config", "core.hooksPath", ".githooks")
        g("config", "user.email", "t@t")
        g("config", "user.name", "t")
        task(repo, "backlog/tasks", "task-4 - a.md", "TASK-4")
        task(repo, "backlog/archive/tasks", "task-4 - b.md", "TASK-4")
        g("add", "backlog")
        r = g("commit", "-q", "-m", "x")
        self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("TASK-4 is used by 2 files", r.stderr)
        (repo / "backlog/archive/tasks/task-4 - b.md").rename(repo / "backlog/archive/tasks/task-99 - b.md")
        (repo / "backlog/archive/tasks/task-99 - b.md").write_text("---\nid: TASK-99\n---\n", encoding="utf-8")
        g("add", "-A", "backlog")
        r = g("commit", "-q", "-m", "x")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
