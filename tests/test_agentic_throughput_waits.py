"""Wait measurement of scripts/agentic_throughput.py (TASK-1647), on synthetic git log text."""
import datetime as dt
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import agentic_throughput as at  # noqa: E402

T = "backlog/tasks/task-{0} - Some-title.md"
A = "backlog/archive/tasks/task-{0} - Some-title.md"


def commit(when, *files):
    lines = [f"COMMIT deadbeef {when}", ""]
    for path, body in files:
        lines += [f"diff --git a/{path} b/{path}", f"--- a/{path}", f"+++ b/{path}", "@@ -1 +1 @@"] + body
    return "\n".join(lines) + "\n"


def run(log, now="2026-11-05T00:00:00"):
    return at.wait_intervals(at.parse_wait_events(log), dt.datetime.fromisoformat(now))


class WaitTests(unittest.TestCase):
    def test_label_add_and_remove_uses_updated_date(self):
        log = commit("2026-11-02T10:00:00+01:00", (T.format(7), ["+  - needs-maintainer-ok",
                                                                  "+updated_date: '2026-11-01 08:00'"]))
        log += commit("2026-11-04T10:00:00+01:00", (T.format(7), ["-  - needs-maintainer-ok",
                                                                  "+updated_date: '2026-11-03 08:00'"]))
        self.assertEqual(run(log), [("TASK-7", "maintainer", dt.datetime(2026, 11, 1, 8), dt.datetime(2026, 11, 3, 8))])

    def test_commit_time_in_utc_when_no_updated_date(self):
        log = commit("2026-11-02T10:00:00+01:00", (T.format(8), ["+  - needs-field-data"]))
        self.assertEqual(run(log), [("TASK-8", "field", dt.datetime(2026, 11, 2, 9), None)])

    def test_in_review_to_waiting_for_field_is_one_wait(self):
        log = commit("2026-10-01T00:00:00+00:00", (T.format(9), ["+status: In Review"]))
        log += commit("2026-10-20T00:00:00+00:00", (T.format(9), ["-status: In Review", "+status: To Do",
                                                                  "+  - waiting-for-field"]))
        self.assertEqual(run(log), [("TASK-9", "field", dt.datetime(2026, 10, 1), None)])

    def test_archive_move_is_not_a_stop(self):
        log = commit("2026-10-01T00:00:00+00:00", (T.format(10), ["+  - needs-decision"]))
        log += commit("2026-10-05T00:00:00+00:00", (T.format(10), ["-  - needs-decision"]),
                      (A.format(10), ["+  - needs-decision"]))
        self.assertEqual(run(log), [("TASK-10", "maintainer", dt.datetime(2026, 10, 1), None)])

    def test_done_ends_a_wait_with_a_stale_marker(self):
        log = commit("2026-10-01T00:00:00+00:00", (T.format(11), ["+  - needs-maintainer-ok"]))
        log += commit("2026-10-03T00:00:00+00:00", (T.format(11), ["-status: To Do", "+status: Done"]))
        self.assertEqual(run(log), [("TASK-11", "maintainer", dt.datetime(2026, 10, 1), dt.datetime(2026, 10, 3))])

    def test_origin_labels_are_not_waits(self):
        log = commit("2026-10-01T00:00:00+00:00", (T.format(12), ["+  - field", "+  - field-report"]))
        self.assertEqual(run(log), [])

    def test_dotted_subtask_id(self):
        log = commit("2026-10-01T00:00:00+00:00", ("backlog/tasks/task-865.3 - Sub.md", ["+  - needs-decision"]))
        self.assertEqual(run(log)[0][0], "TASK-865.3")


if __name__ == "__main__":
    unittest.main()
