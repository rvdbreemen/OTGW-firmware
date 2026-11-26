#!/usr/bin/env python3
"""TASK-3098: a docs(adr)/docs(skill) subject naming Robert or Accepted must not carry a time later than the author date.

Red/green on the real subjects of this repo: 7e127ff84 (authored 00:44, says ~01:10) fails; 62b60aa98 (no clock
time in the pattern) and 9bec44df4 (06:01 exact, authored 06:05) pass. The real .githooks/commit-msg runs the check
in a scratch repo. COMMIT_TIME_SCRIPT=<path> runs the tests against another copy of the script (a mutant).
Run: python tests/test_commit_subject_time.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = Path(os.environ.get("COMMIT_TIME_SCRIPT") or REPO / "scripts" / "check_commit_subject_time.py")
HOOK = REPO / ".githooks" / "commit-msg"


def check(subject, author_iso, tmp):
    f = Path(tmp, "MSG")
    f.write_text(subject + "\n", encoding="utf-8")
    return subprocess.run([sys.executable, str(SCRIPT), str(f), "--author-date", author_iso], capture_output=True, text=True)


class SubjectTime(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_future_time_fails(self):   # the subject of 7e127ff84, authored 00:44 +01:00
        s = "docs(adr): ADR-265 Accepted (Robert, 2026-11-26 ~01:10): MQTT pacer, accepted without the .61 numbers [no-task]"
        r = check(s, "2026-11-26T00:44:06+01:00", self.tmp)
        self.assertEqual(r.returncode, 1, r.stderr)
        self.assertIn("01:10", r.stderr)

    def test_corrected_subject_passes(self):   # 62b60aa98
        s = "docs(adr): ADR-265 and ADR-218 correct the time of Robert's answer (about 00:40 on 2026-11-26, not 01:10), with a correction note [no-task]"
        self.assertEqual(check(s, "2026-11-26T00:48:05+01:00", self.tmp).returncode, 0)

    def test_past_exact_time_passes(self):   # 9bec44df4: 06:01, authored 06:05
        s = "docs(adr): ADR-218 Accepted, unconditional (Robert DM 1575375907462316074, 2026-11-26 06:01) [no-task]"
        self.assertEqual(check(s, "2026-11-26T06:05:03+01:00", self.tmp).returncode, 0)

    def test_offset_is_the_author_offset(self):
        s = "docs(adr): ADR-1 Accepted (Robert, 2026-11-26 06:10)"
        self.assertEqual(check(s, "2026-11-26T06:05:00+01:00", self.tmp).returncode, 1)
        self.assertEqual(check(s, "2026-11-26T06:15:00+01:00", self.tmp).returncode, 0)

    def test_same_minute_is_not_later(self):
        s = "docs(adr): ADR-1 Accepted (Robert, 2026-11-26 06:05)"
        self.assertEqual(check(s, "2026-11-26T06:05:00+01:00", self.tmp).returncode, 0)
        self.assertEqual(check(s, "2026-11-26T06:04:59+01:00", self.tmp).returncode, 1)

    def test_planned_slot_does_not_trigger(self):
        s = "docs(adr): ADR-1 Accepted by Robert, goes in the 08:00 beta"
        self.assertEqual(check(s, "2026-11-26T00:44:00+01:00", self.tmp).returncode, 0)

    def test_other_prefix_or_no_name_does_not_trigger(self):
        t = "2026-11-26T00:44:00+01:00"
        self.assertEqual(check("fix: Accepted by Robert 2026-11-26 09:00", t, self.tmp).returncode, 0)
        self.assertEqual(check("docs(adr): ADR-9 clarified 2026-11-26 09:00", t, self.tmp).returncode, 0)
        self.assertEqual(check("docs(skill): beta flow, Robert 2026-11-26 ~09:00", t, self.tmp).returncode, 1)

    def test_real_hook_blocks_and_passes(self):
        repo = Path(self.tmp, "r")
        (repo / "scripts").mkdir(parents=True)
        (repo / ".githooks").mkdir()
        subprocess.run(["git", "init", "-q"], cwd=repo, check=True)
        shutil.copy(SCRIPT, repo / "scripts" / "check_commit_subject_time.py")
        shutil.copy(HOOK, repo / ".githooks" / "commit-msg")
        env = dict(os.environ, GIT_AUTHOR_DATE="2026-11-26T00:44:06+01:00")

        def run(subject):
            f = repo / "MSG"
            f.write_text(subject + "\n", encoding="utf-8")
            return subprocess.run(["sh", ".githooks/commit-msg", "MSG"], cwd=repo, capture_output=True, text=True, env=env)
        r = run("docs(adr): ADR-265 Accepted (Robert, 2026-11-26 ~01:10) [no-task]")
        self.assertEqual(r.returncode, 1, r.stderr)
        self.assertIn("commit-time", r.stderr)
        self.assertEqual(run("docs(adr): ADR-218 Accepted (Robert, 2026-11-26 00:40) [no-task]").returncode, 0)


if __name__ == "__main__":
    unittest.main()
