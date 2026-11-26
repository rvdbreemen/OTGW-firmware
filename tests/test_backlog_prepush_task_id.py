#!/usr/bin/env python3
"""TASK-3096: the pre-push task id check blocks a push that adds an id origin/dev already has under another name.

Runs the REAL .githooks/pre-push and scripts/check_backlog_task_ids.py with real git: a bare origin, a clone
that stays on an older origin/dev, and a second clone that pushes a task with the same id first. Red: the hooks
that exist today (pre-commit) pass the colliding rebase. Green: the pre-push check blocks it. Also covered: the
same file name (same task), a rename of the old file inside the range, an unfetched origin/dev (warn, skip), a new
unique id, and the bypass.
ID_CHECK_PREPUSH=0 runs the same flow without the pre-push hook (shows what passes today).
Run: python tests/test_backlog_prepush_task_id.py
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
GIT = ["git", "-c", "user.email=t@example.invalid", "-c", "user.name=t", "-c", "commit.gpgsign=false", "-c", "core.autocrlf=false"]
USE_HOOK = os.environ.get("ID_CHECK_PREPUSH", "1") != "0"


def sh(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, **kw)


def g(cwd, *a):
    r = sh(GIT + list(a), cwd)
    assert r.returncode == 0, (a, r.stderr)
    return r.stdout.strip()


def write(root, rel, tid, title="x"):
    p = Path(root, rel)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(f"---\nid: {tid}\ntitle: {title}\n---\n", encoding="utf-8")


class PrePush(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        t = Path(self.tmp)
        self.origin = t / "origin.git"
        sh(["git", "init", "-q", "--bare", "-b", "dev", str(self.origin)], self.tmp)
        self.a = t / "a"
        self.b = t / "b"
        sh(["git", "clone", "-q", str(self.origin), str(self.a)], self.tmp)
        g(self.a, "checkout", "-q", "-b", "dev")
        write(self.a, "backlog/tasks/task-1 - first.md", "TASK-1")
        (self.a / "scripts").mkdir()
        shutil.copy(REPO / "scripts" / "check_backlog_task_ids.py", self.a / "scripts" / "check_backlog_task_ids.py")
        (self.a / ".githooks").mkdir()
        shutil.copy(REPO / ".githooks" / "pre-push", self.a / ".githooks" / "pre-push")
        os.chmod(self.a / ".githooks" / "pre-push", 0o755)
        g(self.a, "add", "-A")
        g(self.a, "commit", "-q", "-m", "base")
        g(self.a, "push", "-q", "origin", "dev")
        sh(["git", "clone", "-q", "-b", "dev", str(self.origin), str(self.b)], self.tmp)
        if USE_HOOK:
            g(self.a, "config", "core.hooksPath", ".githooks")
            g(self.b, "config", "core.hooksPath", ".githooks")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def push(self, repo, *extra, env=None):
        e = dict(os.environ, **(env or {}))
        return sh(["git", "push", *extra, "origin", "dev"], repo, env=e)

    def collide(self):
        """a pushes task-2 'from A'; b (stale) created task-2 'from B' meanwhile, then fetches and rebases."""
        write(self.a, "backlog/tasks/task-2 - from A.md", "TASK-2", "from A")
        g(self.a, "add", "-A"); g(self.a, "commit", "-q", "-m", "A adds 2"); self.assertEqual(self.push(self.a).returncode, 0)
        write(self.b, "backlog/tasks/task-2 - from B.md", "TASK-2", "from B")
        g(self.b, "add", "-A"); g(self.b, "commit", "-q", "-m", "B adds 2")
        g(self.b, "fetch", "-q"); g(self.b, "rebase", "-q", "origin/dev")   # a rebase runs no pre-commit hook

    def test_colliding_id_blocked(self):
        self.collide()
        r = self.push(self.b)
        self.assertNotEqual(r.returncode, 0, "the collision must not get through: " + r.stderr)
        self.assertIn("TASK-2", r.stderr)
        self.assertIn("from A", r.stderr)
        self.assertIn("from B", r.stderr)

    def test_bypass(self):
        self.collide()
        self.assertEqual(self.push(self.b, env={"OTGW_TASKID_HOOK_DISABLE": "1"}).returncode, 0)

    def test_unique_id_passes(self):
        write(self.a, "backlog/tasks/task-3 - three.md", "TASK-3")
        g(self.a, "add", "-A"); g(self.a, "commit", "-q", "-m", "three")
        self.assertEqual(self.push(self.a).returncode, 0)

    def test_same_file_name_is_the_same_task(self):
        # the same file is already on origin/dev, the pushed branch also "adds" it (cherry-picked copy): not a collision
        write(self.a, "backlog/tasks/task-4 - four.md", "TASK-4")
        g(self.a, "add", "-A"); g(self.a, "commit", "-q", "-m", "four"); self.assertEqual(self.push(self.a).returncode, 0)
        g(self.b, "fetch", "-q")
        base = g(self.b, "rev-parse", "origin/dev~1")
        g(self.b, "reset", "-q", "--hard", base)
        write(self.b, "backlog/tasks/task-4 - four.md", "TASK-4")
        g(self.b, "add", "-A"); g(self.b, "commit", "-q", "-m", "four again")
        g(self.b, "fetch", "-q"); g(self.b, "rebase", "-q", "origin/dev")
        r = sh([os.environ.get("PYTHON", "python"), "scripts/check_backlog_task_ids.py", "--pre-push"], self.b,
               input=f"refs/heads/dev {g(self.b, 'rev-parse', 'HEAD')} refs/heads/dev {g(self.b, 'rev-parse', 'origin/dev')}\n")
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_rename_inside_the_range_is_not_a_collision(self):
        # origin/dev has task-1 - first.md; the push renames it (the CLI does that when a title changes)
        g(self.a, "mv", "backlog/tasks/task-1 - first.md", "backlog/tasks/task-1 - first renamed.md")
        g(self.a, "commit", "-q", "-am", "rename")
        self.assertEqual(self.push(self.a).returncode, 0)

    def test_unfetched_origin_warns_and_skips(self):
        self.collide()
        g(self.b, "update-ref", "-d", "refs/remotes/origin/dev")
        r = sh([os.environ.get("PYTHON", "python"), "scripts/check_backlog_task_ids.py", "--pre-push", "--ref", "origin/dev"], self.b,
               input=f"refs/heads/dev {g(self.b, 'rev-parse', 'HEAD')} refs/heads/dev {'0' * 40}\n")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("not fetched", r.stderr)


if __name__ == "__main__":
    unittest.main()
