#!/usr/bin/env python3
"""TASK-3006 follow-up: a write that HANGS on a dead share must not stop the capture.

The realistic failure of the three NAS losses (will_85903): the share goes away and the next write() on the open
handle does not fail, it blocks for minutes. SegmentStore.write_many() holds the store lock while it writes, so every
stream (telnet, mqtt, frames) stops behind that one call. Part 1 (reopen after an error) and parts 2 and 3 (memory
buffer, fallback folder, STREAM INTEGRITY) handle errors; this covers the hang:
  a  the hung write is given up after io_timeout; the store then leaves the run folder alone for io_cooldown seconds and
     keeps the lines in memory, so later writes return at once; after the share is back the lines are written in order
  b  at stop with a share that still hangs, close() does not hang either: one wait, then the fallback folder
The hang is a handle whose write() blocks on an Event (a safety limit of 3 s ends it, so a run on the OLD tool, which
has no deadline, finishes too, but takes the full 3 s and fails the time limit).
Run: python tests/test_capture_write_hang.py
"""
import sys
import tempfile
import threading
import time
import unittest
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402

HANG_SAFETY_SECONDS = 3.0


class HungHandle:
    """A handle on a share that stopped answering: write() blocks until released (or the safety limit), then fails."""

    def __init__(self):
        self.release = threading.Event()
        self.closed = False

    def write(self, text):
        self.release.wait(HANG_SAFETY_SECONDS)
        raise OSError(22, "Invalid argument")

    def flush(self):
        pass

    def close(self):
        self.closed = True


class WriteHang(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.run = self.root / "run"
        self.run.mkdir()
        self.errors = []
        self.store = oc.SegmentStore(self.run, hourly=False, now_fn=lambda: datetime(2026, 11, 25, 9, 0, 0),
                                     background=False, on_error=self.errors.append)
        self.store.fallback_root = self.root / "fallback"
        self.store.io_timeout = 0.3
        self.store.io_cooldown = 60
        self.hung = HungHandle()

    def tearDown(self):
        self.hung.release.set()
        try:
            self.store.close()
        finally:
            self.tmp.cleanup()

    def test_a_a_hung_write_is_given_up_and_the_folder_is_left_alone(self):
        self.assertTrue(self.store.write("transcript.txt", "l1\n"))
        self.store._handles["transcript.txt"] = self.hung       # the share stops answering
        t0 = time.monotonic()
        self.assertFalse(self.store.write("transcript.txt", "l2\n"))
        first = time.monotonic() - t0
        self.assertLess(first, 2.0, f"the hung write was given up after the deadline, not after {first:.1f} s")
        t1 = time.monotonic()
        for i in range(3, 8):
            self.store.write("transcript.txt", f"l{i}\n")
            self.store.write("otframes.log", f"f{i}\n")           # other streams do not wait behind it either
        self.assertLess(time.monotonic() - t1, 0.5, "while the folder is left alone, writes return at once")
        self.assertTrue(any("did not finish within" in e for e in self.errors), self.errors)
        # the share comes back: the cooldown is over, the old call finally fails, the lines go out in order, once
        self.hung.release.set()
        self.store._stuck_until = 0.0
        self.assertTrue(self.store.write("transcript.txt", "l9\n"))
        self.store.close()
        self.assertEqual((self.run / "transcript.txt").read_text(encoding="utf-8").splitlines(),
                         ["l1", "l2", "l3", "l4", "l5", "l6", "l7", "l9"])
        self.assertEqual((self.run / "otframes.log").read_text(encoding="utf-8").splitlines()[-5:],
                         ["f3", "f4", "f5", "f6", "f7"], "after the file's own header line, in order")
        rows = {r.split(":")[0]: r for r in self.store.integrity_lines()}
        self.assertNotIn("WARNING", rows["STREAM INTEGRITY transcript.txt"])

    def test_b_stop_with_a_share_that_still_hangs_does_not_hang(self):
        self.assertTrue(self.store.write("transcript.txt", "l1\n"))
        self.store._handle_locked = lambda stream: self.hung      # every operation on the folder hangs
        self.store._handles.clear()
        self.assertFalse(self.store.write("transcript.txt", "l2\n"))
        t0 = time.monotonic()
        self.store.close()
        self.assertLess(time.monotonic() - t0, 2.0, "close() waits once, then falls back")
        fallback = self.root / "fallback" / "run" / "transcript.txt"
        self.assertTrue(fallback.is_file(), "the buffered line is saved in the fallback folder")
        self.assertEqual(fallback.read_text(encoding="utf-8").splitlines(), ["l2"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
