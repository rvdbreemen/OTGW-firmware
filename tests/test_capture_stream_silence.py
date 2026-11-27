#!/usr/bin/env python3
"""TASK-3009: reconnect a device stream that delivers nothing for N min while the health probe still works.

will_85903's run (20 Nov): from 08:04 to 13:18 both device streams (telnet, /ws frames) delivered nothing over
open sockets while the device was healthy. The 60 s telnet idle check only reconnects when the HTTP health probe
fails or the uptime drops, and the frame recorder only reconnects when its socket closes, so the tool kept
waiting. TASK-3006 reports such a silence (StreamWatch); these tests cover the reconnect.

The real TelnetCapture and FrameCapture run against fakes that keep the socket open and send nothing (no real
device). The silence threshold is a few seconds here (`silent_reconnect`, seconds; the tool sets it from
--stream-silent-minutes):
  a  telnet: silent past the threshold, health probe OK -> one reconnect with a gap marker
  b  telnet: silent, health probe failing -> this path does not reconnect (the idle check owns that case)
  c  frames: silent past the threshold, health probe OK -> reconnect, gap marker, new frames recorded
  d  frames: silent, health probe failing -> no reconnect from this path
  e  the CLI wires --stream-silent-minutes to both recorders
Run: python tests/test_capture_stream_silence.py
"""
import base64
import hashlib
import re
import socket
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
sys.path.insert(0, str(REPO / "tests"))
import otgw_capture as oc  # noqa: E402
from test_capture_tool import FakeDevice, wait_for  # noqa: E402
from test_capture_otframes import LINES, ws_text  # noqa: E402

SILENT = 2   # seconds


class SilentWs:
    """/ws fake: every connection gets the handshake and `first` once, then silence with the socket held open."""

    def __init__(self, first):
        self.first = first
        self.connections = 0
        self._conns = []
        self.srv = socket.socket()
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(5)
        self.port = self.srv.getsockname()[1]
        threading.Thread(target=self._accept, daemon=True).start()

    def _accept(self):
        while True:
            try:
                conn, _ = self.srv.accept()
            except OSError:
                return
            self.connections += 1
            self._conns.append(conn)
            threading.Thread(target=self._serve, args=(conn,), daemon=True).start()

    def _serve(self, conn):
        try:
            req = b""
            while b"\r\n\r\n" not in req:
                chunk = conn.recv(4096)
                if not chunk:
                    return
                req += chunk
            key = re.search(rb"Sec-WebSocket-Key: (\S+)", req).group(1)
            acc = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
            conn.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         b"Sec-WebSocket-Accept: " + acc + b"\r\n\r\n")
            conn.sendall(ws_text(self.first))
            while conn.recv(4096):
                pass
        except OSError:
            pass

    def shutdown(self):
        try:
            self.srv.close()
        except OSError:
            pass
        for c in self._conns:
            try:
                c.close()
            except OSError:
                pass


class SilenceCase(unittest.TestCase):
    def setUp(self):
        self.dev = FakeDevice()
        self.addCleanup(self.dev.shutdown)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.cap = oc.Capture(self.tmp.name, echo=False, hourly=True)
        self.addCleanup(self.cap.store.close)
        self.http_base = "http://127.0.0.1:%d" % self.dev.http_port

    def finish(self, thread):
        self.cap.request_stop("test")
        thread.join(timeout=20)
        self.assertFalse(thread.is_alive())
        self.cap.store.close()

    def read(self, name):
        """A log, also when the hour packer already moved it into a zip."""
        f = Path(self.tmp.name) / name
        if f.exists():
            return f.read_text(encoding="utf-8", errors="replace")
        import zipfile
        out = ""
        for z in sorted((Path(self.tmp.name) / "hours").glob("*.zip")):
            with zipfile.ZipFile(z) as zf:
                if name in zf.namelist():
                    out += zf.read(name).decode("utf-8", "replace")
        return out

    def telnet(self):
        t = oc.TelnetCapture(self.cap, "127.0.0.1", self.dev.telnet_port, self.http_base, 2, 0.25, 0.25, 0)
        t.silent_reconnect = SILENT
        t.start()
        self.addCleanup(lambda: self.finish(t) if t.is_alive() else None)
        return t

    def frames(self, ws):
        t = oc.FrameCapture(self.cap, "127.0.0.1", ws.port)
        t.silent_reconnect = SILENT
        t.health_url = self.http_base + "/api/v2/health"
        t.start()
        self.addCleanup(lambda: self.finish(t) if t.is_alive() else None)
        return t

    def test_a_silent_telnet_with_a_healthy_device_is_reconnected(self):
        t = self.telnet()
        ok = wait_for(lambda: self.dev.accepts >= 2, 20)
        self.assertTrue(ok, "telnet stayed on the silent connection (accepts=%d)\nsummary:\n%s"
                        % (self.dev.accepts, self.read("summary.txt")))
        time.sleep(1.5)   # the new connection's banner; hourly mode keeps the logs in the store until close
        self.finish(t)
        log = self.read("telnet.log")
        self.assertRegex(log, r"### capture: telnet reconnected after \d+ s gap \(stream silent[^)]*\)")
        self.assertRegex(self.read("gaps.log"), r"telnet\.log\t.*stream silent")

    def test_b_silent_telnet_with_a_failing_probe_is_left_to_the_idle_check(self):
        self.dev.close_http()
        t = self.telnet()
        time.sleep(SILENT + 4)
        self.assertEqual(self.dev.accepts, 1, "reconnected although the health probe failed")
        self.finish(t)

    def test_c_silent_frame_stream_with_a_healthy_device_is_reconnected(self):
        ws = SilentWs(LINES["T"])
        self.addCleanup(ws.shutdown)
        t = self.frames(ws)
        ok = wait_for(lambda: ws.connections >= 2, 25)
        self.assertTrue(ok, "frame recorder stayed on the silent /ws (connections=%d)\notframes.log:\n%s"
                        % (ws.connections, self.read(oc.FRAMES_LOG)))
        time.sleep(1.5)   # the frame of the second connection
        self.finish(t)
        self.assertGreaterEqual(self.read(oc.FRAMES_LOG).count("	T	10010A00"), 2, self.read(oc.FRAMES_LOG))
        self.assertRegex(self.read(oc.FRAMES_LOG), r"GAP start=\S+ end=\S+ secs=\d+ reason=.*silent")
        self.assertRegex(self.read("gaps.log"), r"otframes\.log\t.*silent")

    def test_d_silent_frame_stream_with_a_failing_probe_is_not_reconnected(self):
        self.dev.close_http()
        ws = SilentWs(LINES["T"])
        self.addCleanup(ws.shutdown)
        t = self.frames(ws)
        time.sleep(SILENT + 4)
        self.assertEqual(ws.connections, 1, "reconnected although the health probe failed")
        self.finish(t)

    def test_e_cli_sets_both_recorders_from_stream_silent_minutes(self):
        import argparse
        args = oc.build_parser().parse_args(["--stream-silent-minutes", "7"]) if hasattr(oc, "build_parser") else None
        self.assertIsNotNone(args, "no build_parser() to read the option from")
        self.assertEqual(args.stream_silent_minutes, 7)
        self.assertEqual(oc.silent_reconnect_seconds(args), 7 * 60)
        args = oc.build_parser().parse_args(["--stream-silent-minutes", "0"])
        self.assertEqual(oc.silent_reconnect_seconds(args), 0)


if __name__ == "__main__":
    unittest.main()
