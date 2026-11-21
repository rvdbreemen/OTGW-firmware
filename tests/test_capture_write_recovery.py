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


class SilentStream(unittest.TestCase):
    """TASK-3006 AC #2: a stream that writes nothing for N minutes is reported (TOOL ERRORS when it goes silent,
    gaps.log and summary.txt when it recovers or at stop) instead of the run going on silently."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.t = [datetime(2026, 11, 20, 8, 4, 32).astimezone()]
        clock = lambda: self.t[0]   # noqa: E731
        test = self

        class Cap:
            store = oc.SegmentStore(Path(test.tmp.name), hourly=True, now_fn=clock, background=False)
            errors, summaries, gaps = [], [], []

            def script_error(self, line):
                self.errors.append(line)

            def summary(self, line):
                self.summaries.append(line)

            def gap(self, stream, start, end, reason):
                self.gaps.append((stream, start, end, reason))

        self.cap = Cap()
        self.watch = oc.StreamWatch(self.cap, ["telnet.log", "otframes.log"], 600, now_fn=clock)

    def tearDown(self):
        self.cap.store.close()
        self.tmp.cleanup()

    def advance(self, minutes):
        from datetime import timedelta
        self.t[0] = self.t[0] + timedelta(minutes=minutes)

    def test_e_silence_is_reported_and_closed_on_recovery(self):
        self.cap.store.write("telnet.log", "x\n")
        self.cap.store.write("otframes.log", "y\n")
        self.advance(9)
        self.cap.store.write("otframes.log", "y\n")       # frames keep coming, telnet goes quiet
        self.watch.check()
        self.assertEqual(self.cap.errors, [], "9 min is not yet silent")
        self.advance(2)
        self.watch.check()
        self.assertEqual(len(self.cap.errors), 1)
        self.assertIn("telnet.log has written nothing since", self.cap.errors[0])
        self.watch.check()
        self.assertEqual(len(self.cap.errors), 1, "reported once, not every check")
        self.advance(60)
        self.cap.store.write("telnet.log", "back\n")
        self.watch.check()
        self.assertEqual([g[0] for g in self.cap.gaps], ["telnet.log"])
        self.assertTrue(any(l.startswith("WARNING: telnet.log wrote nothing") and "recovered" in l
                            for l in self.cap.summaries), self.cap.summaries)

    def test_f_still_silent_at_stop_goes_into_the_summary(self):
        self.cap.store.write("otframes.log", "y\n")
        self.advance(30 * 60)                               # the 30 h of will_85903, in minutes
        self.watch.check()
        self.watch.close()
        self.assertTrue(any("otframes.log wrote nothing" in l and "still silent at stop" in l
                            for l in self.cap.summaries), self.cap.summaries)
        self.assertEqual(self.cap.gaps[0][0], "otframes.log")

    def test_h_store_with_the_default_naive_clock(self):
        # The real tool builds the store with datetime.now (naive); the first version of the watch crashed the whole
        # run on "can't subtract offset-naive and offset-aware datetimes" (tests/test_capture_tool.py, 21 failures).
        from datetime import timedelta
        with tempfile.TemporaryDirectory() as d:
            store = oc.SegmentStore(Path(d), hourly=False)
            store.write("telnet.log", "x\n")
            self.cap.store = store
            later = datetime.now().astimezone() + timedelta(minutes=11)
            watch = oc.StreamWatch(self.cap, ["telnet.log"], 600, now_fn=lambda: later)
            watch.check()
            store.close()
        self.assertTrue(any("telnet.log has written nothing" in e for e in self.cap.errors), self.cap.errors)

    def test_g_a_stream_that_never_wrote_is_not_watched(self):
        self.advance(60)
        self.watch.check()
        self.watch.close()
        self.assertEqual((self.cap.errors, self.cap.summaries), ([], []))


class HostSleep(unittest.TestCase):
    """TASK-3006 (will_85903 thinks he closed his laptop lid): SleepWatch only saw a sleep when the wall clock ran
    ahead of time.monotonic (macOS). Where the run clock keeps counting through the suspend, both jump together and
    the old check saw nothing; now a 1 s loop that did not run for 10 min is a sleep too."""

    class Cap:
        def __init__(self):
            self.gaps, self.summaries = [], []

        def gap(self, stream, start, end, reason):
            self.gaps.append((stream, start, end, reason))

        def summary(self, line):
            self.summaries.append(line)

    def setUp(self):
        from datetime import timezone
        self.wall = [datetime(2026, 11, 20, 8, 4, 32, tzinfo=timezone.utc)]
        self.mono = [1000.0]
        self.cap = self.Cap()
        self.watch = oc.SleepWatch(self.cap, wall_fn=lambda: self.wall[0], mono_fn=lambda: self.mono[0])

    def tick(self, run_s, wall_s):
        from datetime import timedelta
        self.mono[0] += run_s
        self.wall[0] += timedelta(seconds=wall_s)

    def test_j_both_clocks_jump_together_is_a_sleep(self):
        self.tick(600, 600)                       # 10 min lid closed, run clock counted through it
        self.assertGreater(self.watch.check(), 500)
        self.assertEqual(self.cap.gaps[0][0], "capture-host")
        self.assertTrue(any("asleep" in l for l in self.cap.summaries), self.cap.summaries)

    def test_k_a_normal_second_is_not_a_sleep(self):
        for _ in range(5):
            self.tick(1.0, 1.0)
            self.assertEqual(self.watch.check(), 0)
        self.assertEqual(self.cap.gaps, [])


class KeepAwakeCase(unittest.TestCase):
    def test_l_windows_holds_and_releases(self):
        calls = []

        class K32:
            def SetThreadExecutionState(self, flags):
                calls.append(flags)
                return 0x80000000

        k = oc.KeepAwake(platform_name="win32", kernel32=K32())
        self.assertIn("Keep awake: on", k.acquire())
        k.release()
        self.assertEqual(calls, [0x80000001, 0x80000000])

    def test_m_macos_caffeinate_watches_this_process(self):
        started = []

        class Proc:
            terminated = False

            def terminate(self):
                Proc.terminated = True

        def popen(args, **kw):
            started.append(args)
            return Proc()

        k = oc.KeepAwake(platform_name="darwin", popen=popen, which=lambda name: "/usr/bin/" + name)
        self.assertIn("caffeinate", k.acquire())
        self.assertEqual(started[0][:3], ["caffeinate", "-i", "-w"])
        k.release()
        self.assertTrue(Proc.terminated)

    def test_n_no_tool_says_so(self):
        k = oc.KeepAwake(platform_name="linux", which=lambda name: None)
        self.assertIn("NOT available", k.acquire())


class MqttCsv(unittest.TestCase):
    """TASK-3006 AC #3: a short-run mqtt.log ("topic payload" lines) gave 0 csv rows, so csv/mqtt.csv was dropped and
    the upload had no MQTT file while the summary reported 143483 messages."""

    def test_i_short_run_lines_become_rows(self):
        with tempfile.TemporaryDirectory() as d:
            src, dst = Path(d) / "mqtt.log", Path(d) / "mqtt.csv"
            src.write_text("OTGW/value/otgw-x/boilertemp 42.50\n"
                           "### capture: mqtt reconnected after 12 s gap (broker)\n"
                           "OTGW/value/otgw-x/status {\"a\": 1, \"b\": \"x y\"}\n"
                           "2026-11-20T08:00:00.000+01:00\t1\tOTGW/value/otgw-x/flame\tON\n", encoding="utf-8")
            self.assertEqual(oc.convert_mqtt_csv(src, dst), 3)
            rows = dst.read_text(encoding="utf-8").splitlines()
            self.assertEqual(rows[1], ",,OTGW/value/otgw-x/boilertemp,42.50")
            self.assertIn("x y", rows[2], "the payload keeps its spaces")
            self.assertTrue(rows[3].endswith("\t".join([]) + "OTGW/value/otgw-x/flame,ON"), rows[3])


if __name__ == "__main__":
    unittest.main()
