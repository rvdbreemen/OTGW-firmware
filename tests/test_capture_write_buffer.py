#!/usr/bin/env python3
"""TASK-3006 parts 2 and 3: a run folder that cannot be written (a NAS share that is gone) must not lose lines silently.

Three captures in a row (will_85903: beta.410, 418 #1, 418 #2) lost data after the run folder on a NAS share failed
with Errno 22: 65,405 frames received against 40,330 saved, and summary.txt did not say it. Part 1 (reopen the file
after a write error, tests/test_capture_write_recovery.py) fixes a stale handle. This file covers the case where the
reopen itself fails because the share is down:
  a  the lines written during the outage are kept (bounded, in memory) and go out IN ORDER on the next write that works
  b  an outage that never ends: at stop the lines go to a local fallback folder and the tool says where
  c  the memory buffer is bounded; what does not fit is counted as lost and reported
  d  integrity_lines(): received, saved, in memory, in fallback, lost per stream; no warning when equal
All tests drive the real SegmentStore / Capture; the outage is simulated by making _handle_locked() raise OSError 22.
Old code (origin/dev before this commit): a and b lose the lines, c and d have no such API.
Run: python tests/test_capture_write_buffer.py
"""
import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402


class Outage(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.run = self.root / "run"
        self.run.mkdir()
        self.errors = []
        self.store = oc.SegmentStore(self.run, hourly=False, now_fn=lambda: datetime(2026, 11, 25, 9, 0, 0),
                                     background=False, on_error=self.errors.append)
        self.store.fallback_root = self.root / "fallback"      # a local folder, never the user's real one
        self.down = False
        real = self.store._handle_locked

        def flaky(stream):
            if self.down:
                raise OSError(22, "Invalid argument")           # the share is gone: even the reopen fails
            return real(stream)
        self.store._handle_locked = flaky

    def tearDown(self):
        self.down = False
        try:
            self.store.close()
        finally:
            self.tmp.cleanup()

    def lines(self, path):
        return Path(path).read_text(encoding="utf-8").splitlines()

    def test_a_outage_lines_are_kept_and_written_in_order(self):
        self.assertTrue(self.store.write("transcript.txt", "l1\n"))
        self.down = True
        self.assertFalse(self.store.write("transcript.txt", "l2\n"))
        self.assertFalse(self.store.write("transcript.txt", "l3\n"))
        self.down = False
        self.assertTrue(self.store.write("transcript.txt", "l4\n"), "the first write that works succeeds")
        self.store.close()
        self.assertEqual(self.lines(self.run / "transcript.txt"), ["l1", "l2", "l3", "l4"],
                         "nothing lost, in the order they were written")
        row = [r for r in self.store.integrity_lines() if "transcript.txt" in r][0]
        self.assertIn("received 4 line(s), saved 4", row)
        self.assertNotIn("WARNING", row)

    def test_b_outage_that_never_ends_goes_to_the_fallback_folder(self):
        self.assertTrue(self.store.write("transcript.txt", "l1\n"))
        self.down = True
        self.store.write("transcript.txt", "l2\n")
        self.store.write("transcript.txt", "l3\n")
        self.store.close()                                     # the run folder is still unwritable
        fallback = self.root / "fallback" / "run" / "transcript.txt"
        self.assertTrue(fallback.is_file(), "the lines are saved locally")
        self.assertEqual(self.lines(fallback), ["l2", "l3"])
        self.assertTrue(any(str(fallback) in e for e in self.errors), "the tool says where: " + repr(self.errors))
        row = [r for r in self.store.integrity_lines() if "transcript.txt" in r][0]
        self.assertIn("in fallback 2", row)
        self.assertIn("WARNING", row)
        self.assertIn(str(fallback), row, "summary.txt names the fallback file")

    def test_c_the_buffer_is_bounded_and_the_loss_is_counted(self):
        self.store.pending_max_bytes = 100
        self.down = True
        for i in range(10):
            self.store.write("transcript.txt", f"line {i:02d} " + "x" * 20 + "\n")   # 30 bytes each
        self.assertLessEqual(self.store._pending_bytes, 100)
        self.down = False
        self.store.write("transcript.txt", "last\n")
        self.store.close()
        kept = self.lines(self.run / "transcript.txt")
        self.assertEqual(kept[-1], "last")
        self.assertIn("line 09 " + "x" * 20, kept, "the newest lines are the ones that stay")
        self.assertNotIn("line 00 " + "x" * 20, kept, "the oldest are dropped when the buffer is full")
        row = [r for r in self.store.integrity_lines() if "transcript.txt" in r][0]
        self.assertIn("received 11 line(s)", row)
        self.assertRegex(row, r"lost [1-9]")
        self.assertIn("WARNING", row)
        self.assertEqual(len([e for e in self.errors if "buffer" in e and "full" in e]), 1, "said once, not per line")


class Integrity(unittest.TestCase):
    def test_d_no_warning_when_everything_was_saved(self):
        with tempfile.TemporaryDirectory() as tmp:
            store = oc.SegmentStore(Path(tmp), hourly=False, background=False)
            for i in range(3):
                store.write("telnet.log", f"x{i}\n")
            store.write("otframes.log", "a\nb\n")
            rows = store.integrity_lines()
            store.close()
        self.assertEqual(rows, [
            "STREAM INTEGRITY otframes.log: received 2 line(s), saved 2, in memory 0, in fallback 0, lost 0",
            "STREAM INTEGRITY telnet.log: received 3 line(s), saved 3, in memory 0, in fallback 0, lost 0",
        ])


if __name__ == "__main__":
    unittest.main(verbosity=2)
