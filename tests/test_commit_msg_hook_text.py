"""The commit-msg rejection for a TASK-NNN without a task file names both ways out in one line (TASK-3095).

Runs the REAL .githooks/commit-msg in a scratch repo. COMMIT_MSG_HOOK=<path> runs it against another copy
(used to show red on the hook text from before). Run: python tests/test_commit_msg_hook_text.py
"""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOOK = Path(os.environ.get("COMMIT_MSG_HOOK") or ROOT / ".githooks" / "commit-msg")


class HookText(unittest.TestCase):
    def run_hook(self, message):
        with tempfile.TemporaryDirectory() as d:
            subprocess.run(["git", "init", "-q"], cwd=d, check=True)
            msg = Path(d, "MSG")
            msg.write_text(message, encoding="utf-8")
            return subprocess.run(["sh", str(HOOK), str(msg)], cwd=d, capture_output=True, text=True)

    def test_missing_task_file_names_both_ways_out_in_one_line(self):
        r = self.run_hook("fix: something (TASK-99999)\n")
        self.assertEqual(r.returncode, 1)
        lines = [l for l in r.stderr.splitlines() if "stage the task file" in l]
        self.assertEqual(len(lines), 1, r.stderr)
        line = lines[0]
        self.assertIn("drop the TASK-NNN reference", line)
        self.assertIn("[no-task]", line)
        self.assertNotIn("[no-src]", r.stderr, "no tag the hook does not know")

    def test_no_task_tag_still_passes_a_docs_commit(self):
        with tempfile.TemporaryDirectory() as d:
            subprocess.run(["git", "init", "-q"], cwd=d, check=True)
            Path(d, "docs").mkdir()
            Path(d, "docs", "x.md").write_text("x")
            subprocess.run(["git", "add", "docs/x.md"], cwd=d, check=True)
            msg = Path(d, "MSG"); msg.write_text("docs: x [no-task]\n", encoding="utf-8")
            r = subprocess.run(["sh", str(HOOK), str(msg)], cwd=d, capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == "__main__":
    unittest.main()
