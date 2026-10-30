#!/usr/bin/env python3
"""
TASK-1503: raw OpenTherm frame recorder of scripts/otgw_capture.py.

The recorder connects to the device's /ws (the OT log WebSocket), parses every OT
log line into (device timestamp, source letter, 8 hex digits) and writes
  otframes.log         host receive time, device timestamp, source letter, frame
  otgw_simulation.log  the bare frames, one per line, the format /otgw_simulation.log reads
Cases:
  a  parser: OT log lines of every source letter, event lines and noise
  b  writers: column layout, header, LF endings, replay-format regexp
  c  FrameCapture against a fake /ws server: files, count, summary line
  d  FrameCapture degrades when /ws cannot be opened: error logged, no abort

Run: python tests/test_capture_otframes.py
"""

import base64
import hashlib
import os
import re
import socket
import struct
import sys
import tempfile
import threading
import time
import unittest
from datetime import datetime, timezone, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import otgw_capture as oc  # noqa: E402

# Lines as OTGW-Core.ino processOT() builds them: "<HH:MM:SS.uuuuuu> <label 18 chars> <frame> <id> <type>..."
LINES = {
    "T": "10:15:30.123456 Thermostat        T10010A00   1 Write-Data      > ",
    "B": "10:15:30.234567 Boiler            B40000800   0 Read-Ack        > ",
    "R": "10:15:30.345678 Request Boiler    R90190000  25 Read-Data       ",
    "A": "10:15:30.456789 Answer Thermostat  A401920B0  25 Read-Ack        > ",
    "E": "10:15:30.567890 Parity Error      E00000000   0 Read-Data       P",
}
EVENT_LINES = [
    "10:15:30.123456 > TT=20.5",
    "10:15:30.123456 < TT: 20.50",
    "10:15:30.123456 ! something went wrong",
    "PS1 Master 1              = 1",
    '{"type":"keepalive"}',
    "",
]


class ParserTest(unittest.TestCase):
    def test_every_source_letter(self):
        for letter, line in LINES.items():
            ts, got_letter, frame = oc.parse_ot_log_line(line)
            self.assertEqual(ts, "10:15:30" + line[8:15])
            self.assertEqual(got_letter, letter)
            self.assertEqual(len(frame), 8)
            self.assertIn(letter + frame, line)

    def test_values(self):
        self.assertEqual(oc.parse_ot_log_line(LINES["T"]), ("10:15:30.123456", "T", "10010A00"))
        self.assertEqual(oc.parse_ot_log_line(LINES["B"]), ("10:15:30.234567", "B", "40000800"))
        self.assertEqual(oc.parse_ot_log_line(LINES["R"]), ("10:15:30.345678", "R", "90190000"))
        self.assertEqual(oc.parse_ot_log_line(LINES["A"]), ("10:15:30.456789", "A", "401920B0"))

    def test_non_frames_are_none(self):
        for line in EVENT_LINES:
            self.assertIsNone(oc.parse_ot_log_line(line), line)


class WriterTest(unittest.TestCase):
    def test_writers(self):
        with tempfile.TemporaryDirectory() as tmp:
            frames, sim = Path(tmp) / oc.FRAMES_LOG, Path(tmp) / oc.SIM_LOG
            w = oc.FrameLogWriter(frames, sim)
            tz = timezone(timedelta(hours=1))
            w.write(datetime(2026, 10, 30, 10, 15, 30, 123456, tzinfo=tz), "10:15:30.123456", "T", "10010A00")
            w.comment("connected")
            w.write(datetime(2026, 10, 30, 10, 15, 30, 999000, tzinfo=tz), "10:15:30.234567", "B", "40000800")
            w.close()
            self.assertEqual(w.count, 2)
            raw = frames.read_bytes()
            self.assertNotIn(b"\r", raw)
            lines = raw.decode().split("\n")
            self.assertTrue(lines[0].startswith("# "))
            self.assertIn("host_time", lines[0])
            self.assertEqual(lines[1], "2026-10-30T10:15:30.123+01:00\t10:15:30.123456\tT\t10010A00")
            self.assertTrue(lines[2].startswith("# "))
            self.assertEqual(lines[3], "2026-10-30T10:15:30.999+01:00\t10:15:30.234567\tB\t40000800")
            self.assertEqual(sim.read_bytes(), b"T10010A00\nB40000800\n")
            for line in sim.read_text().splitlines():
                self.assertRegex(line, r"^[A-Z][0-9A-F]{8}$")


# ---- fake /ws server -------------------------------------------------------

def ws_text(payload):
    data = payload.encode()
    return bytes([0x81, len(data)]) + data if len(data) < 126 else bytes([0x81, 126]) + struct.pack("!H", len(data)) + data


class FakeWs:
    def __init__(self, messages):
        self.messages = messages
        self.srv = socket.socket()
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(2)
        self.port = self.srv.getsockname()[1]
        threading.Thread(target=self.run, daemon=True).start()

    def run(self):
        try:
            conn, _ = self.srv.accept()
            req = b""
            while b"\r\n\r\n" not in req:
                req += conn.recv(4096)
            key = re.search(rb"Sec-WebSocket-Key: (\S+)", req).group(1)
            acc = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
            conn.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         b"Sec-WebSocket-Accept: " + acc + b"\r\n\r\n")
            conn.sendall(bytes([0x89, 0]))  # ping, must not break the reader
            for m in self.messages:
                conn.sendall(ws_text(m))
                time.sleep(0.02)
            time.sleep(1.0)
            conn.close()
        except OSError:
            pass


class CaptureThreadTest(unittest.TestCase):
    def run_capture(self, port, seconds):
        tmp = tempfile.mkdtemp()
        cap = oc.Capture(tmp, echo=False)
        t = oc.FrameCapture(cap, "127.0.0.1", port)
        t.start()
        cap.wait(seconds)
        cap.request_stop("test")
        t.join(timeout=15)
        self.assertFalse(t.is_alive())
        return cap, Path(tmp)

    def test_records_frames(self):
        msgs = list(LINES.values())[:4] + EVENT_LINES + [LINES["E"]]
        ws = FakeWs(msgs)
        cap, tmp = self.run_capture(ws.port, 1.5)
        rows = [l.split("\t") for l in (tmp / oc.FRAMES_LOG).read_text().splitlines() if not l.startswith("#")]
        self.assertEqual([r[2] + r[3] for r in rows], ["T10010A00", "B40000800", "R90190000", "A401920B0", "E00000000"])
        for r in rows:
            self.assertRegex(r[0], r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}[+-]\d\d:\d\d$")
        sim = (tmp / oc.SIM_LOG).read_text().splitlines()
        self.assertEqual(len(sim), 5)
        for line in sim:
            self.assertRegex(line, r"^[A-Z][0-9A-F]{8}$")
        self.assertIn("5 frame(s)", (tmp / "summary.txt").read_text())

    def test_degrades_when_unreachable(self):
        s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()  # closed port
        cap, tmp = self.run_capture(port, 1.0)
        self.assertIn("otframes", (tmp / "script.error.log").read_text())
        self.assertIn("0 frame(s)", (tmp / "summary.txt").read_text())


if __name__ == "__main__":
    unittest.main()
