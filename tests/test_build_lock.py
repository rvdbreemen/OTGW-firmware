"""TASK-1488: build.py holds a machine-wide lock so only one build runs at a time.

Runs the real acquire_build_lock()/release_build_lock() from build.py in child processes
against a temporary lock path. Red on a build.py without the lock (the functions are missing).

Run: python tests/test_build_lock.py
"""
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD_PY_DIR = Path(os.environ.get("OTGW_BUILD_PY_DIR", REPO))

CHILD = r"""
import sys, time
sys.path.insert(0, sys.argv[1])
import build
lock = build.acquire_build_lock("child", wait_minutes=float(sys.argv[3]), poll_seconds=0.2, lock_path=sys.argv[2])
print("START", time.time(), flush=True)
time.sleep(float(sys.argv[4]))
print("END", time.time(), flush=True)
build.release_build_lock(lock)
"""


def run_child(lock, wait, hold):
    return subprocess.Popen([sys.executable, "-c", CHILD, str(BUILD_PY_DIR), str(lock), str(wait), str(hold)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def stamps(out):
    d = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] in ("START", "END"):
            d[parts[0]] = float(parts[1])
    return d


class BuildLock(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.lock = Path(self.tmp.name) / "build.lock"

    def tearDown(self):
        self.tmp.cleanup()

    def test_second_build_waits_for_the_first(self):
        a = run_child(self.lock, 5, 2.0)
        time.sleep(0.8)                       # a holds the lock
        b = run_child(self.lock, 5, 0.1)
        oa, _ = a.communicate(timeout=30)
        ob, _ = b.communicate(timeout=30)
        sa, sb = stamps(oa), stamps(ob)
        self.assertIn("END", sa, oa)
        self.assertIn("START", sb, ob)
        self.assertGreaterEqual(sb["START"], sa["END"] - 0.05, (oa, ob))   # b started only after a ended
        self.assertIn("Waiting for the running build", ob)
        self.assertFalse(self.lock.exists())

    def test_wait_zero_refuses_and_names_the_holder(self):
        a = run_child(self.lock, 5, 2.0)
        time.sleep(0.8)
        b = run_child(self.lock, 0, 0.1)
        ob, _ = b.communicate(timeout=30)
        a.communicate(timeout=30)
        self.assertEqual(b.returncode, 3, ob)
        self.assertIn("Another build is running", ob)
        self.assertIn("child", ob)              # holder line names its worktree
        self.assertNotIn("START", ob)

    def test_stale_lock_of_a_dead_pid_is_taken_over(self):
        self.lock.write_text("999999\nold-tree\n2026-01-01 00:00:00\n", encoding="utf-8")
        b = run_child(self.lock, 0, 0.1)
        ob, _ = b.communicate(timeout=30)
        self.assertEqual(b.returncode, 0, ob)
        self.assertIn("stale build lock", ob)
        self.assertIn("START", ob)


if __name__ == "__main__":
    unittest.main(verbosity=2)
