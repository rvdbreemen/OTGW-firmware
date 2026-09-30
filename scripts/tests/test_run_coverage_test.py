#!/usr/bin/env python3
"""Self-test for run_coverage_test.py: a short capture must fail loudly.

The 2026-09-23 OT-Direct run ended 538 s into a 694 s window with 1.66 fixture
loops captured, and the runner compared it as if it were complete. These tests
prove the two failure modes that allowed that now stop the runner with exit 2:

  * the device closes the telnet stream before the window ends (early EOF);
  * the capture holds fewer than two loops of decoded fixture frames.

Everything under test is the real runner, imported, never re-implemented: its
capture(), count_fixture_loops() and main(). Only the device is replaced: a fake
telnet that echoes the two debug toggles the way the firmware does, and stubs for
the HTTP calls (upload, simulate start/stop, preflight). The fake wraps the
runner's real set_debug_flag() only to learn when a read-back has returned, so
its stream never lands in one. No network beyond 127.0.0.1, no device, no broker.

Run: python scripts/tests/test_run_coverage_test.py      (takes about 20 s)
"""
from __future__ import annotations

import contextlib
import io
import json
import os
import socket
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import coverage_baseline          # noqa: E402
import run_coverage_test as rct   # noqa: E402

with open(os.path.join(HERE, "otgw_simulation_coverage.log"), encoding="ascii") as _fh:
    FIXTURE = [line.strip() for line in _fh if line.strip()]

SOURCE_LABEL = {"T": "Thermostat", "B": "Boiler", "R": "Request Boiler",
                "A": "Answer Thermostat", "E": "Parity Error"}
MSG_TYPE = ("Read-Data", "Write-Data", "Invalid-Data", "Reserved",
            "Read-Ack", "Write-Ack", "Data-Invalid", "Unknown-DataId")
NOT_A_FIXTURE_FRAME = "T00FE0000"


def decode_line(frame: str, t: float) -> str:
    """One processOT line in the device's telnet format, e.g.
    07:26:13.825782 (  86768| 40948) processOT   (5170): Boiler  B40000202   0 Read-Ack  >  ..."""
    secs = 7 * 3600 + t
    stamp = (f"{int(secs // 3600) % 24:02d}:{int(secs // 60) % 60:02d}:"
             f"{int(secs) % 60:02d}.{int((secs % 1) * 1e6):06d}")
    msgid = int(frame[3:5], 16)
    mtype = MSG_TYPE[int(frame[1], 16) & 7]
    return (f"{stamp} (  80000| 40000) processOT   (5170): {SOURCE_LABEL[frame[0]]:<18} "
            f"{frame}  {msgid:>2} {mtype:<14} >  Value = {msgid}\r\n")


def synthetic_capture(frames: int) -> list:
    """`frames` decoded fixture frames in replay order, wrapping at the end of the
    fixture, 750 ms apart, with the noise a real capture carries in between."""
    out = []
    for i in range(frames):
        t = i * 0.75
        out.append(decode_line(FIXTURE[i % len(FIXTURE)], t))
        if i % 7 == 0:
            out.append(f"07:00:00.000000 (  80000| 40000) satSimulatio(1136): "
                       f"OTGW-SIM trace: MID=48 VAL=0 (src=otdirect-tx)\r\n")
        if i % 50 == 0:
            out.append(decode_line(NOT_A_FIXTURE_FRAME, t + 0.1))   # must not count
    return out


class FakeTelnet:
    """Just enough of the device's debug telnet for the real capture().

    Both debug flags start at the opposite of `want`, so capture() needs one
    press per flag. Each press is echoed as 'Debug MQTT: <v>' or 'Debug MQTT
    Gating: <v>', the lines the runner reads back. Once both flags are where
    the runner wants them and the runner has read back its last toggle, decode
    lines stream until `close_after` lines have gone out (then the server
    closes: an early EOF), or until the runner hangs up at the end of its window
    when `close_after` is None.

    Use it as a context manager. It points the runner at its port and wraps the
    runner's real set_debug_flag() to learn when a read-back has returned, so
    the stream never starts inside one, however slow the host.
    """

    def __init__(self, want: bool, close_after: int | None):
        self.srv = socket.create_server(("127.0.0.1", 0))
        self.port = self.srv.getsockname()[1]
        self.want = want
        self.close_after = close_after
        self.flags = {"3": not want, "g": not want}
        self.readback_done = threading.Event()
        self._patches = [
            mock.patch.object(rct, "TELNET_PORT", self.port),
            mock.patch.object(rct, "set_debug_flag", self._signal_on_return(rct.set_debug_flag)),
        ]
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def __enter__(self):
        for p in self._patches:
            p.start()
        return self

    def __exit__(self, *exc):
        for p in reversed(self._patches):
            p.stop()
        return False

    def _signal_on_return(self, real):
        """The runner's own set_debug_flag(), unchanged, plus a signal when it
        returns. The signal is cleared on entry: the key that brings the last
        flag into place is pressed inside the last call, so the stream can only
        start once that call has finished reading back."""
        def wrapper(*args, **kwargs):
            self.readback_done.clear()
            try:
                return real(*args, **kwargs)
            finally:
                self.readback_done.set()
        return wrapper

    def _serve(self):
        conn, _ = self.srv.accept()
        try:
            conn.sendall(b"\r\nOTGW debug telnet (fake)\r\n")
            conn.settimeout(0.05)
            while any(v != self.want for v in self.flags.values()):
                try:
                    key = conn.recv(1).decode()
                except socket.timeout:
                    continue
                if not key:
                    return
                if key in self.flags:
                    self.flags[key] = not self.flags[key]
                    name = "Debug MQTT" if key == "3" else "Debug MQTT Gating"
                    conn.sendall(f"\r\n{name}: {str(self.flags[key]).lower()}\r\n".encode())
            # The runner reads each toggle's echo back for 1.5 s or more (press()).
            # Lines sent during a read-back never reach the capture file, so wait
            # for the runner's last set_debug_flag() call to return. The timeout
            # only keeps a broken run from hanging; the assertions then fail.
            if not self.readback_done.wait(timeout=30):
                return
            sent = 0
            while self.close_after is None or sent < self.close_after:
                conn.sendall(decode_line(FIXTURE[sent % len(FIXTURE)], sent * 0.75).encode())
                sent += 1
                time.sleep(0.005)
            conn.shutdown(socket.SHUT_WR)          # a FIN, as a device dropping the session
            time.sleep(0.2)
        except OSError:
            pass                                   # the runner hung up: its window ended
        finally:
            conn.close()
            self.srv.close()


class Workdir(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = self._tmp.name
        self.out = os.path.join(self.tmp, "capture.log")
        self.baseline = os.path.join(self.tmp, "baseline.json")

    def tearDown(self):
        self._tmp.cleanup()

    def write_baseline_from(self, lines: list) -> None:
        """A baseline that the given capture matches exactly, so only the new
        checks can stop a run: the comparison itself would PASS."""
        with open(self.baseline, "w", encoding="utf-8") as fh:
            json.dump(coverage_baseline.fingerprint(lines), fh)

    def run_main(self, capture_stub=None, record=False):
        """Run the real main() with the device's HTTP side stubbed. Without a
        capture_stub, the real capture() talks to a FakeTelnet the caller holds.

        Returns (exit code, stderr text, HTTP calls made)."""
        calls = []

        def http(url, method="GET", timeout=15):
            calls.append((method, url))
            return "{}"

        argv = ["run_coverage_test.py", "--host", "127.0.0.1", "--topics", "telnet",
                "--out", self.out, "--baseline", self.baseline]
        if record:
            argv.append("--record")
        patches = [mock.patch.object(sys, "argv", argv),
                   mock.patch.object(rct, "upload_fixture", lambda host, path: None),
                   mock.patch.object(rct, "http", http),
                   mock.patch.object(rct, "preflight_replay", lambda host: 33)]
        if capture_stub is not None:
            patches.append(mock.patch.object(rct, "capture", capture_stub))
        err = io.StringIO()
        with contextlib.ExitStack() as stack:
            for p in patches:
                stack.enter_context(p)
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
                rc = rct.main()
        return rc, err.getvalue(), calls

    def capture_writing(self, lines: list):
        """A capture() stand-in that 'captures' the given lines."""
        def stub(host, seconds, out_path, want_mqtt):
            with open(out_path, "w", encoding="utf-8") as fh:
                fh.writelines(lines)
            return len(lines)
        return stub


class EarlyEofTest(Workdir):
    """Failure mode 1: the device closes the telnet stream mid-window."""

    def test_capture_raises_when_the_device_closes_early(self):
        t0 = time.time()
        with FakeTelnet(want=False, close_after=40), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(RuntimeError) as cm:
                rct.capture("127.0.0.1", 60, self.out, want_mqtt=False)
        self.assertLess(time.time() - t0, 30, "must fail at the close, not wait out the window")
        self.assertIsInstance(cm.exception, rct.CaptureIncomplete)
        self.assertIn("closed the telnet stream", str(cm.exception))
        with open(self.out, encoding="utf-8") as fh:
            self.assertEqual(fh.read().count("processOT"), 40, "the stream before the close was kept")

    def test_capture_returns_normally_when_the_stream_lasts_the_window(self):
        with FakeTelnet(want=False, close_after=None), contextlib.redirect_stdout(io.StringIO()):
            lines = rct.capture("127.0.0.1", 1, self.out, want_mqtt=False)
        self.assertGreater(lines, 0)

    def test_main_exits_2_and_records_nothing_on_early_eof(self):
        with FakeTelnet(want=True, close_after=50):      # telnet mode wants MQTT debug on
            rc, err, calls = self.run_main(record=True)
        self.assertEqual(rc, 2, err)
        self.assertFalse(os.path.exists(self.baseline), "a partial capture became a baseline")
        self.assertIn("INCOMPLETE CAPTURE", err)
        self.assertIn("closed the telnet stream", err)
        self.assertIn(("POST", "http://127.0.0.1/api/v2/simulate/stop"), calls)
        with open(self.out, encoding="utf-8") as fh:
            self.assertEqual(fh.read().count("processOT"), 50, "the whole stream reached the capture")


class TooFewLoopsTest(Workdir):
    """Failure mode 2: the stream stays open but carries under two loops."""

    def test_count_matches_the_2026_09_23_capture(self):
        loops, decoded = rct.count_fixture_loops(synthetic_capture(702), FIXTURE)
        self.assertEqual(decoded, 702)
        self.assertAlmostEqual(loops, 702 / 423)
        self.assertLess(loops, rct.MIN_LOOPS)

    def test_count_of_a_default_window(self):
        loops, _ = rct.count_fixture_loops(synthetic_capture(922), FIXTURE)
        self.assertGreaterEqual(loops, rct.MIN_LOOPS)

    def test_only_fixture_decode_lines_count(self):
        self.assertNotIn(NOT_A_FIXTURE_FRAME, FIXTURE)
        noise = [decode_line(NOT_A_FIXTURE_FRAME, 0.0),
                 "07:00:00.000000 (  80000| 40000) satSimulatio(1136): OTGW-SIM trace\r\n",
                 f"{FIXTURE[5]} seen outside a processOT line\r\n"]
        self.assertEqual(rct.count_fixture_loops(noise, FIXTURE), (0.0, 0))

    def test_main_refuses_to_compare_a_short_capture(self):
        short = synthetic_capture(702)
        self.write_baseline_from(short)
        rc, err, calls = self.run_main(capture_stub=self.capture_writing(short))
        self.assertEqual(rc, 2, err)
        self.assertIn("1.66 fixture loops", err)
        self.assertIn(("POST", "http://127.0.0.1/api/v2/simulate/stop"), calls)

    def test_main_refuses_to_record_a_short_capture(self):
        rc, err, _ = self.run_main(capture_stub=self.capture_writing(synthetic_capture(702)),
                                   record=True)
        self.assertEqual(rc, 2, err)
        self.assertFalse(os.path.exists(self.baseline), "a partial capture became a baseline")

    def test_main_passes_a_two_loop_capture(self):
        full = synthetic_capture(922)
        self.write_baseline_from(full)
        rc, err, _ = self.run_main(capture_stub=self.capture_writing(full))
        self.assertEqual(rc, 0, err)


if __name__ == "__main__":
    unittest.main(verbosity=2)
