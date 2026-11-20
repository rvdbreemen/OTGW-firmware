"""TASK-2922: the capture tool records a sleep of the capture machine as a gap in gaps.log.

indigo_light's macOS upload 20261119-091712: the snapshot poller (300 s of time.monotonic between rounds, an
ERROR line on any failed request) wrote 13:48:36 and 13:53:36 and then nothing until 14:19:01, while the run
clock only advanced about one interval. No code runs while the machine sleeps, so the tool can only notice it
afterwards: the wall clock moved further than the run clock. These tests drive the real SleepWatch.check()
from scripts/otgw_capture.py with injected clocks.
"""
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import otgw_capture as oc  # noqa: E402

T0 = datetime(2026, 11, 19, 14, 7, 41, tzinfo=timezone(timedelta(hours=1)))


class Clocks:
    def __init__(self):
        self.wall = T0
        self.mono = 1000.0

    def tick(self, run_s, wall_s=None):
        self.mono += run_s
        self.wall += timedelta(seconds=run_s if wall_s is None else wall_s)


class SleepWatchCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.cap = oc.Capture(self.tmp.name, echo=False, hourly=True)
        self.addCleanup(self.cap.store.close)
        self.clk = Clocks()
        self.watch = oc.SleepWatch(self.cap, wall_fn=lambda: self.clk.wall, mono_fn=lambda: self.clk.mono)

    def gaps(self):
        p = self.cap.path("gaps.log")
        return p.read_text(encoding="utf-8").splitlines() if p.exists() else []

    def test_a_sleep_of_668_s_is_one_gap_with_start_end_and_seconds(self):
        self.clk.tick(1.0)
        self.watch.check()
        self.clk.tick(0.5, wall_s=668.5)          # asleep: the run clock barely moved
        self.assertGreater(self.watch.check(), 600)
        lines = self.gaps()
        self.assertEqual(len(lines), 1, lines)
        start, end, secs, stream, reason = lines[0].split("\t")
        self.assertEqual(stream, "capture-host")
        self.assertEqual(start, "2026-11-19T14:07:42+01:00")
        self.assertEqual(end, "2026-11-19T14:18:50+01:00")
        self.assertEqual(int(secs), 668)
        self.assertIn("asleep", reason)

    def test_normal_ticks_and_a_small_skew_write_nothing(self):
        for _ in range(120):
            self.clk.tick(1.0)
            self.watch.check()
        self.clk.tick(1.0, wall_s=12.0)           # NTP step or a busy machine, below the limit
        self.assertEqual(self.watch.check(), 0)
        self.assertEqual(self.gaps(), [])

    def test_a_wall_clock_step_backwards_is_not_a_gap(self):
        self.clk.tick(1.0, wall_s=-3600.0)
        self.assertEqual(self.watch.check(), 0)
        self.assertEqual(self.gaps(), [])

    def test_the_watch_runs_as_a_thread_and_stops_with_the_capture(self):
        self.watch.start()
        self.cap.request_stop("test")
        self.watch.join(timeout=5)
        self.assertFalse(self.watch.is_alive())


if __name__ == "__main__":
    unittest.main()
