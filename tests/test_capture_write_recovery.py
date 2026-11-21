#!/usr/bin/env python3
"""TASK-3006 AC #1: a write error on one stream must not silence that stream for the rest of the run.

will_85903's 31 h capture (20-21 Nov, run folder on a NAS share): at 13:18:51 the first "OSError [Errno 22] Invalid
argument" in the segment store, then 10,000+ by 17:33, and every later write of those streams failed. Cause, in the
real scripts/otgw_capture.py: SegmentStore._handle_locked() returns the cached handle, and write_many() only counts
the OSError and keeps that broken handle, so every later write goes to the same dead handle.

This test drives the real SegmentStore and swaps the open handle for one that fails like a stale share handle (every
write raises OSError 22) while a freshly opened file works:
  a  after one failed write the next write reopens the file and succeeds (fails on the old code: stays dead)
  b  in one write_many() batch, a failing stream does not stop the other streams of the batch
Run: python tests/test_capture_write_recovery.py
"""
import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402


class DeadHandle:
    """Like a file handle on a share that dropped the session: every write fails."""
    closed = False

    def write(self, text):
        raise OSError(22, "Invalid argument")

    def flush(self):
        raise OSError(22, "Invalid argument")

    def close(self):
        self.closed = True


class WriteRecovery(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.run = Path(self.tmp.name)
        self.errors = []
        self.store = oc.SegmentStore(self.run, hourly=False, now_fn=lambda: datetime(2026, 11, 20, 13, 18, 51),
                                     background=False, on_error=self.errors.append)

    def tearDown(self):
        self.store.close()
        self.tmp.cleanup()

    def test_a_next_write_reopens_after_a_dead_handle(self):
        self.assertTrue(self.store.write("transcript.txt", "line 1\n"))
        dead = DeadHandle()
        self.store._handles["transcript.txt"] = dead          # the share drops the open handle
        self.assertFalse(self.store.write("transcript.txt", "line 2\n"), "the failed write is reported")
        self.assertTrue(dead.closed, "the broken handle is closed and dropped")
        self.assertTrue(self.store.write("transcript.txt", "line 3\n"), "the next write reopens and succeeds")
        self.store.close()
        text = (self.run / "transcript.txt").read_text(encoding="utf-8")
        self.assertIn("line 1\n", text)
        self.assertIn("line 3\n", text)
        self.assertEqual(len(self.errors), 1, self.errors)

    def test_b_one_failing_stream_does_not_stop_the_batch(self):
        self.assertTrue(self.store.write_many((("transcript.txt", "a\n"), ("otframes.log", "b\n"))))
        self.store._handles["transcript.txt"] = DeadHandle()
        self.assertFalse(self.store.write_many((("transcript.txt", "c\n"), ("otframes.log", "d\n"))))
        self.store.close()
        self.assertEqual((self.run / "otframes.log").read_text(encoding="utf-8").splitlines()[-1], "d",
                         "the healthy stream of the same batch is still written")


class AlwaysFails:
    """A store whose every write fails (the share is gone for good)."""
    hourly = False

    def write(self, stream, text):
        return False

    def write_many(self, items):
        return False

    def touch(self, stream):
        pass


class Counters(unittest.TestCase):
    """TASK-3006: will_85903's summary said "143483 message(s) written to mqtt.log" and counted 208134 frames, while
    the files held 5,825 and 6,864 lines. Received and written are counted apart."""

    def test_c_frames_count_only_what_was_written(self):
        w = oc.FrameLogWriter(None, None, store=AlwaysFails())
        w.write(datetime(2026, 11, 20, 13, 20).astimezone(), "13:20:00.000000", "B", "40000000")
        self.assertEqual((w.received, w.count), (1, 0))

    def test_d_mqtt_counts_received_and_written_apart(self):
        class Cap:
            store = AlwaysFails()
            lines = []

            def summary(self, line):
                self.lines.append(line)

        cap = Cap()
        m = oc.MqttCapture.__new__(oc.MqttCapture)
        m.cap, m.keep, m.client, m.err, m.gap_start, m.dropped = cap, None, None, None, None, 0
        m.messages, m.written = 0, 0
        m.log = oc.StreamWriter(cap.store, "mqtt.log")
        m.handle("OTGW/value/x/boilertemp", b"42.0")
        self.assertEqual((m.messages, m.written), (1, 0))
        m.stop()
        self.assertTrue(any("1 message(s) received, 0 written" in l and "NOT written" in l for l in cap.lines), cap.lines)


if __name__ == "__main__":
    unittest.main()
