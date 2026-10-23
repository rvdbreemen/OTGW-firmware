#!/usr/bin/env python3
"""
TASK-1299: the telnet worker embedded in scripts/capture-mqtt-debug.sh.

The worker is a Python program inside a bash heredoc. This test extracts that
heredoc with a regex, runs it as a subprocess against an in-test fake device
(a telnet listener plus an HTTP /api/v2/health endpoint with controllable
uptime and status) and reads the summary.txt and telnet.log it writes. It runs
the real worker code; nothing is re-implemented here. The device URL has no
port in the script, so the test rewrites it to the fake HTTP port.

Cases:
  a  device resets the connection (RST)        -> reconnect, gap line, marker, gaps 1
  b  silent, healthy, constant uptime          -> zero reconnects, probes spaced by the idle period
  c  silent, uptime drops                      -> reconnect 'device rebooted (uptime reset)'
  d  silent, health answers 503                -> no reconnect (device is alive)
  e  silent, health unreachable                -> reconnect 'health probe failed'
  f  stop while the device is down             -> 'Telnet gap still open at stop'
  h  watchdog gap starts at the last successful probe, not the last telnet byte
  g  SIGTERM while the device is down          -> totals still written (POSIX only)

OTGW_CAPTURE_SH points the test at another copy of the script (used to show it
red on the version before the fix). Run: python tests/test_capture_telnet_worker.py
"""

import json
import os
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = Path(os.environ.get("OTGW_CAPTURE_SH", REPO / "scripts" / "capture-mqtt-debug.sh"))
IDLE = 2  # seconds without telnet data before the health probe (the bash parser enforces >= 15)

BANNER = b"OTGW fake device\r\nPress 'h' for command menu\r\n"


def extract_worker(text):
    m = re.search(r"start_telnet_capture\(\) \{.*?<<'PY' &\n(.*?)\nPY\n", text, re.S)
    if not m:
        raise AssertionError("telnet worker heredoc not found in " + str(SCRIPT))
    return m.group(1)


class FakeDevice:
    def __init__(self):
        self.uptime = 5
        self.health = "ok"          # ok | 503
        self.health_times = []
        self.accepts = 0
        self.conns = []
        self._lock = threading.Lock()
        self.tsock = socket.socket()
        self.tsock.bind(("127.0.0.1", 0))
        self.tsock.listen(5)
        self.telnet_port = self.tsock.getsockname()[1]
        self.tclosed = False
        threading.Thread(target=self._accept_loop, daemon=True).start()
        dev = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def do_GET(self):
                dev.health_times.append(time.monotonic())
                if dev.health != "ok":
                    self.send_response(503)
                    self.end_headers()
                    return
                m = dev.uptime
                body = json.dumps({"health": {"status": "UP", "uptime": "0(d)-%02d:%02d(H:m)" % (m // 60, m % 60)}}).encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.http_port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def _accept_loop(self):
        while True:
            try:
                c, _ = self.tsock.accept()
            except OSError:
                return
            self.accepts += 1
            with self._lock:
                self.conns.append(c)
            try:
                c.sendall(BANNER)
            except OSError:
                pass

    def reset_connections(self):
        """Close every client connection with an RST (SO_LINGER 0)."""
        with self._lock:
            conns, self.conns = self.conns, []
        for c in conns:
            try:
                c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
                c.close()
            except OSError:
                pass

    def close_telnet(self):
        self.tclosed = True
        try:
            self.tsock.shutdown(socket.SHUT_RDWR)  # wakes accept() on Linux
        except OSError:
            pass
        self.tsock.close()
        self.reset_connections()

    def close_http(self):
        self.httpd.shutdown()
        self.httpd.server_close()

    def shutdown(self):
        for f in (self.close_telnet, self.close_http):
            try:
                f()
            except Exception:
                pass


def wait_for(pred, timeout):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


class WorkerCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.worker_src = extract_worker(SCRIPT.read_text(encoding="utf-8"))

    def start(self, idle=IDLE):
        self.dev = FakeDevice()
        self.addCleanup(self.dev.shutdown)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        d = Path(self.tmp.name)
        src = self.worker_src
        # The script has no port for the health URL; point it at the fake HTTP port.
        src = src.replace('f"http://{host}/api/v2/health"', 'f"http://{host}:%d/api/v2/health"' % self.dev.http_port)
        self.worker_py = d / "worker.py"
        self.worker_py.write_text(src, encoding="utf-8")
        self.summary = d / "summary.txt"
        self.telnet_log = d / "telnet.log"
        self.stop_file = d / ".stop"
        self.summary.touch()
        self.telnet_log.touch()
        self.proc = subprocess.Popen(
            [sys.executable, str(self.worker_py), "127.0.0.1", str(self.dev.telnet_port),
             str(self.telnet_log), str(self.summary), str(self.stop_file),
             "2", "250", "250", str(idle)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.addCleanup(self._kill)
        self.assertTrue(wait_for(lambda: "Settings dump" in self.text(), 10), "worker never connected:\n" + self.text())

    def _kill(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        self.proc.stdout.close()

    def text(self):
        return self.summary.read_text(encoding="utf-8", errors="replace")

    def stop(self):
        self.stop_file.touch()
        try:
            self.proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            self.fail("worker did not stop after the stop file:\n" + self.text())
        return self.text()

    def count(self, text, needle):
        return text.count(needle)

    def test_a_reset_reconnects_with_gap_and_marker(self):
        self.start(idle=0)
        time.sleep(0.5)
        self.dev.reset_connections()
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        t = self.stop()
        self.assertIn("Telnet gaps: 1,", t)
        self.assertRegex(t, r"Telnet gap: \S+ -> \S+ \(\d+ s, \d+ failed connect attempts, read failed\)")
        self.assertIn("### capture: telnet reconnected after", self.telnet_log.read_text(errors="replace"))
        # marker and summary agree on the length
        secs = re.search(r"Telnet gap: \S+ -> \S+ \((\d+) s", t).group(1)
        self.assertIn("reconnected after %s s gap" % secs, self.telnet_log.read_text(errors="replace"))

    def test_b_quiet_healthy_device_is_not_disconnected(self):
        self.start()
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 3, 15), "no health probes: " + self.text())
        t = self.stop()
        self.assertEqual(self.dev.accepts, 1, t)
        self.assertNotIn("reconnect", t)
        self.assertIn("Telnet gaps: 0, total 0 s", t)
        gaps = [b - a for a, b in zip(self.dev.health_times, self.dev.health_times[1:])]
        self.assertTrue(all(g >= IDLE - 0.3 for g in gaps), "probes closer than the idle period: %r" % gaps)

    def test_c_uptime_drop_reconnects(self):
        self.start()
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 1, 10))
        self.dev.uptime = 1
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        t = self.stop()
        self.assertIn("device rebooted (uptime reset)", t)
        self.assertRegex(t, r"Telnet gap: .*device rebooted \(uptime reset\)\)")

    def test_h_watchdog_gap_starts_at_last_successful_probe(self):
        self.start()
        t0 = time.monotonic()
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 4, 20))
        self.dev.uptime = 1
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        quiet = time.monotonic() - t0
        t = self.stop()
        secs = int(re.search(r"Telnet gap: \S+ -> \S+ \((\d+) s", t).group(1))
        self.assertGreater(quiet, 3 * IDLE)
        self.assertLessEqual(secs, 2 * IDLE + 2, "gap counted from the last telnet byte, not the last probe: " + t)

    def test_d_health_503_keeps_the_connection(self):
        self.start()
        self.dev.health = "503"
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 3, 15), "no health probes: " + self.text())
        t = self.stop()
        self.assertEqual(self.dev.accepts, 1, t)
        self.assertNotIn("reconnect", t)
        self.assertIn("Telnet gaps: 0, total 0 s", t)

    def test_e_health_unreachable_reconnects(self):
        self.start()
        self.dev.close_http()
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        t = self.stop()
        self.assertRegex(t, r"Telnet gap: .*health probe failed\)")
        self.assertIn("Telnet gaps: 1,", t)

    def test_f_stop_while_device_down_reports_open_gap(self):
        self.start(idle=0)
        self.dev.close_telnet()
        self.assertTrue(wait_for(lambda: "Telnet connect attempt" in self.text(), 15), self.text())
        t = self.stop()
        self.assertIn("Telnet gap still open at stop: since", t)
        self.assertIn("Telnet gaps: 1,", t)
        self.assertEqual(t.count("Telnet gaps:"), 1)

    @unittest.skipIf(os.name == "nt", "SIGTERM handling is the POSIX stop path")
    def test_g_sigterm_writes_totals_once(self):
        self.start(idle=0)
        self.dev.close_telnet()
        self.assertTrue(wait_for(lambda: "Telnet connect attempt" in self.text(), 15), self.text())
        self.proc.send_signal(signal.SIGTERM)
        self.proc.wait(timeout=10)
        t = self.text()
        self.assertEqual(t.count("Telnet gaps:"), 1, t)
        self.assertIn("Telnet gap still open at stop: since", t)


if __name__ == "__main__":
    unittest.main(verbosity=2)
