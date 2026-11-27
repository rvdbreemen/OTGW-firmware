#!/usr/bin/env python3
"""TASK-2979: the browser capture survives a CDP websocket error.

A beta tester's 24 h run (20 Nov): telnet and MQTT dropped for 1 to 2 s and came back, but the tool printed
'Browser capture: error - websocket closed' and the web UI console/network capture stayed dead for the rest of
the run. Cause: BrowserCapture.run() ended its thread on the first exception.

The real BrowserCapture runs against a fake CDP endpoint (an HTTP /json target list plus a WebSocket server)
and a fake browser process (subprocess.Popen is replaced; no real browser, no real device):
  a  the first CDP connection dies without a close frame; the tool re-attaches, keeps recording the console and
     logs 'Browser capture: reconnected' in summary.txt (fails on the old code: thread ends, no second connection)
  b  a stop request during the back-off ends the thread cleanly
Run: python tests/test_capture_browser_reconnect.py
"""
import base64
import hashlib
import json
import re
import socket
import sys
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402


def ws_text(payload):
    data = payload.encode()
    return bytes([0x81, len(data)]) + data if len(data) < 126 else bytes([0x81, 126]) + len(data).to_bytes(2, "big") + data


def console_event(text):
    return json.dumps({"method": "Runtime.consoleAPICalled",
                       "params": {"type": "log", "args": [{"type": "string", "value": text}]}})


class FakeCdp:
    """One page target on /json and a websocket per connection: `plans` says what each connection does
    ('die' = send one console line, then close the TCP connection without a close frame; 'hold' = send one
    console line and stay open). Connections beyond the plan hold."""

    def __init__(self, plans):
        self.plans = list(plans)
        self.connections = 0
        self._lock = threading.Lock()
        self.srv = socket.socket()
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(5)
        self.ws_port = self.srv.getsockname()[1]
        threading.Thread(target=self._accept, daemon=True).start()
        cdp = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def do_GET(self):
                body = json.dumps([{"type": "page", "webSocketDebuggerUrl": "ws://127.0.0.1:%d/devtools/page/1" % cdp.ws_port}]).encode()
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.http_port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def _accept(self):
        while True:
            try:
                conn, _ = self.srv.accept()
            except OSError:
                return
            with self._lock:
                self.connections += 1
                n = self.connections
            threading.Thread(target=self._serve, args=(conn, n), daemon=True).start()

    def _serve(self, conn, n):
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
            conn.sendall(ws_text(console_event("console line from connection %d" % n)))
            plan = self.plans[n - 1] if n - 1 < len(self.plans) else "hold"
            if plan == "die":
                time.sleep(0.3)
                conn.close()
                return
            while conn.recv(4096):   # hold: swallow the CDP commands until the tool disconnects
                pass
        except OSError:
            pass
        finally:
            conn.close()

    def shutdown(self):
        for closer in (self.srv.close, self.httpd.shutdown, self.httpd.server_close):
            try:
                closer()
            except Exception:
                pass


class FakeProc:
    def poll(self):
        return None

    def terminate(self):
        pass

    def wait(self, timeout=None):
        return 0

    def kill(self):
        pass


def wait_for(cond, seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if cond():
            return True
        time.sleep(0.1)
    return cond()


class BrowserReconnect(unittest.TestCase):
    def start(self, plans):
        self.cdp = FakeCdp(plans)
        self.addCleanup(self.cdp.shutdown)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.cap = oc.Capture(self.tmp.name, echo=False)
        self.addCleanup(self.cap.store.close)
        for owner, name, value in ((oc.subprocess, "Popen", FakeProc()), (oc, "windows_kill_on_close_job", None)):
            p = mock.patch.object(owner, name, return_value=value)
            p.start()
            self.addCleanup(p.stop)
        self.bc = oc.BrowserCapture(self.cap, "fake-browser", "http://127.0.0.1:1/", self.cdp.http_port, self.tmp.name)
        self.bc.start()
        self.addCleanup(self.stop)

    def stop(self):
        self.cap.request_stop("test")
        self.bc.join(timeout=15)

    def browser_log(self):
        f = Path(self.tmp.name) / "browser.log"
        return f.read_text(encoding="utf-8", errors="replace") if f.exists() else ""

    def summary(self):
        f = Path(self.tmp.name) / "summary.txt"
        return f.read_text(encoding="utf-8", errors="replace") if f.exists() else ""

    def test_a_cdp_websocket_death_is_followed_by_a_reattach(self):
        self.start(["die", "hold"])
        ok = wait_for(lambda: "console line from connection 2" in self.browser_log(), 25)
        self.assertTrue(ok, "the browser capture never re-attached after the CDP websocket died.\n"
                            "thread alive=%s connections=%d\nbrowser.log:\n%s\nsummary:\n%s"
                        % (self.bc.is_alive(), self.cdp.connections, self.browser_log(), self.summary()))
        self.assertIn("console line from connection 1", self.browser_log())
        self.assertTrue(self.bc.is_alive(), "the capture thread must keep running")
        self.assertIn("Browser capture: reconnected", self.summary())
        self.assertEqual(self.summary().count("Browser capture: reconnected"), 1, self.summary())
        self.stop()
        self.assertFalse(self.bc.is_alive())
        s = self.summary()
        self.assertNotIn("Browser capture: error", s)
        self.assertIn("Browser capture: completed normally", s)

    def test_b_stop_during_backoff_ends_the_thread(self):
        self.start(["die", "die", "die", "die"])
        self.assertTrue(wait_for(lambda: self.cdp.connections >= 2, 25), "no re-attach")
        t0 = time.monotonic()
        self.stop()
        self.assertFalse(self.bc.is_alive())
        self.assertLess(time.monotonic() - t0, 10)


if __name__ == "__main__":
    unittest.main()
