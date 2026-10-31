#!/usr/bin/env python3
"""
TASK-1475 / ADR-202: the Python capture tool scripts/otgw_capture.py.

Every case runs the real tool as a subprocess against in-test fakes: a telnet
listener with a toggle banner that flips states on key presses, an HTTP server
(/api/v2/health, /api/v2/device/info) and, for the MQTT case, a minimal MQTT
3.1.1 broker. The tool is stopped through the <run>/.stop file (or a signal), and
the test reads the merged transcript it leaves behind. Nothing of the tool is
re-implemented here.

Cases ported from tests/test_capture_telnet_worker.py (TASK-1299, which tested
the old bash heredoc worker):
  a  device resets the connection (RST)        -> reconnect, gap line, marker, gaps 1
  b  silent, healthy, constant uptime          -> zero reconnects, probes spaced by the idle period
  c  silent, uptime drops                      -> reconnect 'device rebooted (uptime reset)'
  d  silent, health answers 503                -> no reconnect (device is alive)
  e  silent, health unreachable                -> reconnect 'health probe failed'
  f  stop while the device is down             -> 'Telnet gap still open at stop'
  g  SIGTERM while the device is down          -> totals written once, transcript built (POSIX only)
  h  watchdog gap starts at the last successful probe, not the last telnet byte
New:
  i  TASK-1396: q/D wait for 'Setup finished!' (uptime 0 until then)
  j  TASK-1396: device/info probe body keeps hd_min_free_heap / hd_min_max_block
  k  TASK-1453: no broker -> MQTT skipped line, telnet still captured, no prompt with stdin redirected
  l  TASK-1453: toggles switched on are restored at stop; simulators never sent
  m  TASK-1453: restore after a mid-capture reconnect without reboot compares states
  n  TASK-1453: device gone at stop -> 'restore failed (device likely gone)', transcript still built, exit 0
  o  MQTT via the vendored paho wheel: messages land as 'topic payload' lines
  p  PowerShell-style arguments (-DeviceHost ...) are accepted
  q  browser child dies with the tool (fake browser process; Windows job object / Linux PDEATHSIG)

Run: python tests/test_capture_tool.py
"""

import io
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
TOOL = REPO / "scripts" / "otgw_capture.py"
IDLE = 2  # seconds of telnet silence before the health probe (the real floor is 15)

INITIAL_TOGGLES = [("1", "OTmsg", "0"), ("2", "REST API", "1"), ("3", "MQTT", "0"), ("8", "SensorSim", "0")]


class FakeDevice:
    """Telnet + HTTP fake. Toggle state lives on the device and survives reconnects."""

    def __init__(self, toggles=INITIAL_TOGGLES, uptime=5, setup_line=False, info_pad=400):
        self.uptime = uptime
        self.health = "ok"          # ok | 503
        self.health_times = []
        self.accepts = 0
        self.received = []
        self.conns = []
        self.keys = {k: label for k, label, _ in toggles}
        self.states = {label: s for _, label, s in toggles}
        self.setup_line = setup_line
        self.info_pad = info_pad
        self._lock = threading.Lock()
        self.tsock = socket.socket()
        self.tsock.bind(("127.0.0.1", 0))
        self.tsock.listen(5)
        self.telnet_port = self.tsock.getsockname()[1]
        threading.Thread(target=self._accept_loop, daemon=True).start()
        dev = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def send_body(self, code, body):
                self.send_response(code)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                if self.path == "/api/v2/health":
                    dev.health_times.append(time.monotonic())
                    if dev.health != "ok":
                        self.send_body(503, b"{}")
                        return
                    m = dev.uptime
                    self.send_body(200, json.dumps({"health": {"status": "UP", "uptime": "0(d)-%02d:%02d(H:m)" % (m // 60, m % 60)}}).encode())
                elif self.path == "/api/v2/device/info":
                    info = {"device": {"hostname": "fake-otgw", "fwversion": "2.0.0-test", "padding": "x" * dev.info_pad,
                                       "hd_min_free_heap": 12345, "hd_min_max_block": 6789}}
                    self.send_body(200, json.dumps(info).encode())
                else:
                    self.send_body(404, b"{}")

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.http_port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def banner(self):
        lines = ["OTGW fake device"]
        for key, label in self.keys.items():
            lines.append(f"  {key}  {label} [{self.states[label]}]")
        lines.append("Press 'h' for command menu")
        return ("\r\n".join(lines) + "\r\n").encode()

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
                c.sendall(self.banner())
            except OSError:
                pass
            threading.Thread(target=self._client, args=(c,), daemon=True).start()

    def _client(self, c):
        while True:
            try:
                data = c.recv(64)
            except OSError:
                return
            if not data:
                return
            for ch in data.decode("ascii", "replace"):
                with self._lock:
                    self.received.append(ch)
                    if ch in self.keys:
                        label = self.keys[ch]
                        self.states[label] = "1" if self.states[label] == "0" else "0"
                try:
                    c.sendall(f"key {ch}\r\n".encode())
                except OSError:
                    return

    def send_all(self, text):
        with self._lock:
            conns = list(self.conns)
        for c in conns:
            try:
                c.sendall(text.encode())
            except OSError:
                pass

    def reset_connections(self):
        with self._lock:
            conns, self.conns = self.conns, []
        for c in conns:
            try:
                c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
                c.close()
            except OSError:
                pass

    def close_telnet(self):
        try:
            self.tsock.shutdown(socket.SHUT_RDWR)
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


class FakeBroker:
    """Just enough MQTT 3.1.1: CONNACK, SUBACK, one PUBLISH, PINGRESP."""

    def __init__(self, topic="otgw-firmware/value/test", payload=b"42"):
        self.topic = topic
        self.payload = payload
        self.connects = 0
        self.sock = socket.socket()
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(2)
        self.port = self.sock.getsockname()[1]
        threading.Thread(target=self._loop, daemon=True).start()

    @staticmethod
    def _read_packet(c):
        head = c.recv(1)
        if not head:
            return None, b""
        mult, length = 1, 0
        while True:
            b = c.recv(1)[0]
            length += (b & 0x7F) * mult
            if not b & 0x80:
                break
            mult *= 128
        body = b""
        while len(body) < length:
            chunk = c.recv(length - len(body))
            if not chunk:
                break
            body += chunk
        return head[0], body

    def _loop(self):
        while True:
            try:
                c, _ = self.sock.accept()
            except OSError:
                return
            threading.Thread(target=self._client, args=(c,), daemon=True).start()

    def _client(self, c):
        try:
            while True:
                ptype, body = self._read_packet(c)
                if ptype is None:
                    return
                kind = ptype >> 4
                if kind == 1:
                    self.connects += 1
                    c.sendall(b"\x20\x02\x00\x00")
                elif kind == 8:
                    c.sendall(b"\x90\x03" + body[:2] + b"\x00")
                    t = self.topic.encode()
                    var = struct.pack("!H", len(t)) + t + self.payload
                    c.sendall(bytes([0x30, len(var)]) + var)
                elif kind == 12:
                    c.sendall(b"\xd0\x00")
                elif kind == 14:
                    return
        except OSError:
            return

    def shutdown(self):
        self.sock.close()


def wait_for(pred, timeout):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def section(transcript_text, title):
    m = re.search(r"={60}\n=== " + re.escape(title) + r"\n={60}\n(.*?)(?=\n={60}\n=== |\Z)", transcript_text, re.S)
    return m.group(1) if m else ""


class ToolCase(unittest.TestCase):
    def start(self, idle=IDLE, extra=(), dev=None, broker=None, wait_text="Debug toggle actions", stdin=subprocess.DEVNULL):
        self.dev = dev or FakeDevice()
        self.addCleanup(self.dev.shutdown)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = Path(self.tmp.name) / "out"
        env = dict(os.environ, OTGW_CAPTURE_SETTINGS=str(Path(self.tmp.name) / "settings.json"))
        args = [sys.executable, str(TOOL), "--device", "127.0.0.1", "--telnet-port", str(self.dev.telnet_port),
                "--http-port", str(self.dev.http_port), "--output-root", str(self.out),
                "--telnet-connect-timeout-seconds", "2", "--telnet-reconnect-delay-ms", "250",
                "--telnet-post-disconnect-delay-ms", "250", "--idle-check-floor", "1",
                "--telnet-idle-check-seconds", str(idle), "--skip-browser-capture", "--skip-crashlog-capture",
                "--skip-http-probes"]
        if broker is not None:
            args += ["--broker", "127.0.0.1", "--broker-port", str(broker.port)]
        else:
            args += ["--broker", ""]
        args += list(extra)
        self.proc = subprocess.Popen(args, stdin=stdin, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        self.addCleanup(self._kill)
        self.assertTrue(wait_for(lambda: self.run_dir() is not None, 30), "no run folder")  # interpreter start-up under load
        if wait_text:
            self.assertTrue(wait_for(lambda: wait_text in self.text(), 15), "tool never got to '%s':\n%s" % (wait_text, self.text()))

    def _kill(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()
        self.proc.stdout.close()

    def run_dir(self):
        dirs = [d for d in self.out.glob("*") if d.is_dir()] if self.out.exists() else []
        return dirs[0] if dirs else None

    def transcript(self):
        found = list(self.run_dir().glob("transcript-*.txt"))
        return found[0].read_text(encoding="utf-8", errors="replace") if found else None

    def text(self):
        """summary.txt while running, the transcript's SUMMARY section afterwards."""
        d = self.run_dir()
        if d is None:
            return ""
        s = d / "summary.txt"
        if s.exists():
            return s.read_text(encoding="utf-8", errors="replace")
        t = self.transcript()
        return section(t, "SUMMARY (summary.txt)") if t else ""

    def telnet_text(self):
        d = self.run_dir()
        log = d / "telnet.log"
        if log.exists():
            return log.read_text(encoding="utf-8", errors="replace")
        return section(self.transcript() or "", "OTGW TELNET DEBUG (telnet.log)")

    def stop(self, timeout=30):
        (self.run_dir() / ".stop").touch()
        try:
            self.rc = self.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.fail("tool did not stop after the stop file:\n" + self.text())
        self.output = self.proc.stdout.read().decode(errors="replace")   # what the tool printed
        self.assertIsNotNone(self.transcript(), "no transcript written:" + chr(10) + self.output)
        return self.text()

    # ---- TASK-1299 cases (ported) ----------------------------------------
    def test_a_reset_reconnects_with_gap_and_marker(self):
        self.start(idle=0)
        time.sleep(0.5)
        self.dev.reset_connections()
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        t = self.stop()
        self.assertIn("Telnet gaps: 1,", t)
        self.assertRegex(t, r"Telnet gap: \S+ -> \S+ \(\d+ s, \d+ failed connect attempts, (read failed|connection closed)\)")
        secs = re.search(r"Telnet gap: \S+ -> \S+ \((\d+) s", t).group(1)
        self.assertIn("reconnected after %s s gap" % secs, self.telnet_text())

    def test_b_quiet_healthy_device_is_not_disconnected(self):
        self.start()
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 4, 15), "no health probes: " + self.text())
        t = self.stop()
        self.assertEqual(self.dev.accepts, 1, t)
        self.assertNotIn("reconnect", t)
        self.assertIn("Telnet gaps: 0, total 0 s", t)
        watchdog = self.dev.health_times[1:]  # the first probe is the setup check at connect
        gaps = [b - a for a, b in zip(watchdog, watchdog[1:])]
        self.assertTrue(all(g >= IDLE - 0.3 for g in gaps), "probes closer than the idle period: %r" % gaps)

    def test_c_uptime_drop_reconnects(self):
        self.start()
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 2, 10))
        self.dev.uptime = 1
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        t = self.stop()
        self.assertRegex(t, r"Telnet gap: .*device rebooted \(uptime reset\)\)")

    def test_d_health_503_keeps_the_connection(self):
        self.start(wait_text="Settings dump: sent")
        self.dev.health = "503"
        n = len(self.dev.health_times)
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= n + 3, 15), "no health probes: " + self.text())
        t = self.stop()
        self.assertEqual(self.dev.accepts, 1, t)
        self.assertNotIn("reconnect", t)
        self.assertIn("Telnet gaps: 0, total 0 s", t)

    def test_e_health_unreachable_reconnects(self):
        self.start(wait_text="Settings dump: sent")
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
        self.rc = self.proc.wait(timeout=30)
        t = self.text()
        self.assertEqual(t.count("Telnet gaps:"), 1, t)
        self.assertIn("Telnet gap still open at stop: since", t)
        self.assertEqual(self.rc, 130)

    def test_h_watchdog_gap_starts_at_last_successful_probe(self):
        self.start(wait_text="Settings dump: sent")
        t0 = time.monotonic()
        self.assertTrue(wait_for(lambda: len(self.dev.health_times) >= 5, 20))
        self.dev.uptime = 1
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        quiet = time.monotonic() - t0
        t = self.stop()
        secs = int(re.search(r"Telnet gap: \S+ -> \S+ \((\d+) s", t).group(1))
        self.assertGreater(quiet, 3 * IDLE)
        self.assertLessEqual(secs, 2 * IDLE + 2, "gap counted from the last telnet byte, not the last probe: " + t)

    # ---- TASK-1396 ------------------------------------------------------
    def test_i_settings_dump_waits_for_setup_finished(self):
        dev = FakeDevice(uptime=0)
        self.start(idle=0, dev=dev, wait_text="Settings dump: waiting")
        time.sleep(2)
        self.assertNotIn("q", dev.received, "q sent while setup() still ran: %r" % dev.received)
        self.assertNotIn("D", dev.received)
        dev.send_all("Setup finished!\r\n")
        self.assertTrue(wait_for(lambda: "Settings dump: sent" in self.text(), 10), self.text())
        self.stop()
        self.assertIn("q", dev.received)
        self.assertIn("D", dev.received)

    def test_j_device_info_probe_keeps_heap_fields(self):
        dev = FakeDevice(info_pad=400)  # pushes the heap fields past the 200-character cut
        self.addCleanup(dev.shutdown)
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        out = Path(tmp.name) / "out"
        env = dict(os.environ, OTGW_CAPTURE_SETTINGS=str(Path(tmp.name) / "settings.json"))
        rc = subprocess.run([sys.executable, str(TOOL), "--device", "127.0.0.1", "--telnet-port", str(dev.telnet_port),
                             "--http-port", str(dev.http_port), "--output-root", str(out), "--broker", "",
                             "--duration", "3", "--telnet-idle-check-seconds", "0", "--skip-browser-capture",
                             "--skip-crashlog-capture"], stdin=subprocess.DEVNULL, capture_output=True, env=env, timeout=90).returncode
        self.assertEqual(rc, 0)
        transcript = next(out.glob("*/transcript-*.txt")).read_text(encoding="utf-8")
        probes = section(transcript, "HTTP PROBES (curl-probes.log)")
        self.assertIn("hd_min_free_heap", probes)
        self.assertIn("hd_min_max_block", probes)
        self.assertRegex(probes, r"HTTP\s+404 .*/api/v2/sat/status")

    # ---- TASK-1453 ------------------------------------------------------
    def test_k_no_broker_skips_mqtt_and_keeps_telnet(self):
        dev = FakeDevice()
        self.start(dev=dev, idle=0)
        t = self.stop()
        self.assertIn("MQTT capture skipped: no BrokerHost given (telnet, probe and crash log still run).", t)
        self.assertIn("OTGW fake device", self.telnet_text())
        self.assertEqual(self.rc, 0)

    def test_l_toggles_restored_at_stop_simulator_untouched(self):
        dev = FakeDevice()
        self.start(dev=dev, idle=0)
        self.assertTrue(wait_for(lambda: dev.states["OTmsg"] == "1" and dev.states["MQTT"] == "1", 5), dev.states)
        t = self.stop()
        self.assertEqual(dev.states, {label: s for _, label, s in INITIAL_TOGGLES}, t)
        self.assertNotIn("8", dev.received, "simulator toggle key sent")
        self.assertIn("Debug toggles restored to their state at capture start: OTmsg, MQTT", t)

    def test_m_restore_after_reconnect_without_reboot(self):
        dev = FakeDevice()
        self.start(dev=dev, idle=0)
        time.sleep(0.5)
        dev.reset_connections()
        self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
        time.sleep(1)
        t = self.stop()
        self.assertIn("OTmsg=already-on", t)
        self.assertEqual(dev.states, {label: s for _, label, s in INITIAL_TOGGLES}, t)
        self.assertEqual(dev.received.count("1"), 2, dev.received)  # on at start, off at stop
        self.assertNotIn("8", dev.received)

    def test_n_restore_when_device_gone_is_bounded(self):
        dev = FakeDevice()
        self.start(dev=dev, idle=0)
        dev.close_telnet()
        self.assertTrue(wait_for(lambda: "Telnet connect attempt" in self.text() or "Telnet disconnected" in self.text()
                                 or "Telnet read failed" in self.text(), 15), self.text())
        t0 = time.monotonic()
        t = self.stop(timeout=30)
        self.assertLess(time.monotonic() - t0, 20)
        self.assertIn("Debug toggles restore failed (device likely gone)", t)
        self.assertEqual(self.rc, 0)

    # ---- MQTT, CLI, browser ------------------------------------------------
    def test_o_mqtt_messages_via_vendored_paho(self):
        broker = FakeBroker()
        self.addCleanup(broker.shutdown)
        self.start(broker=broker, idle=0, wait_text="MQTT connected")
        time.sleep(1)
        self.stop()
        mqtt = section(self.transcript(), "MQTT BROKER STREAM (mqtt.log)")
        self.assertIn("otgw-firmware/value/test 42", mqtt)

    def test_p_powershell_style_arguments(self):
        dev = FakeDevice()
        self.addCleanup(dev.shutdown)
        with tempfile.TemporaryDirectory() as tmp:
            env = dict(os.environ, OTGW_CAPTURE_SETTINGS=str(Path(tmp) / "s.json"))
            r = subprocess.run([sys.executable, str(TOOL), "-DeviceHost", "127.0.0.1", "-BrokerHost", "",
                                "-DurationSeconds", "2", "-OutputRoot", tmp, "-SkipBrowserCapture", "-SkipCrashlogCapture",
                                "-SkipHttpProbes", "-TelnetIdleCheckSeconds", "0", "--telnet-port", str(dev.telnet_port),
                                "--http-port", str(dev.http_port)], stdin=subprocess.DEVNULL, capture_output=True, env=env, timeout=60)
            self.assertEqual(r.returncode, 0, r.stdout.decode(errors="replace") + r.stderr.decode(errors="replace"))
            self.assertTrue(list(Path(tmp).glob("*/transcript-*.txt")))
            saved = json.loads((Path(tmp) / "s.json").read_text(encoding="utf-8"))
            self.assertEqual(saved["DeviceHost"], "127.0.0.1")
            self.assertNotIn("MqttPassword", saved)

    def test_r_run_without_broker_keeps_saved_mqtt_prefill(self):
        """TASK-1453 risk: saving an empty BrokerHost must not break the next run's prefill."""
        dev = FakeDevice()
        self.addCleanup(dev.shutdown)
        with tempfile.TemporaryDirectory() as tmp:
            settings = Path(tmp) / "s.json"
            seeded = {"DeviceHost": "old-host", "BrokerHost": "broker.lan", "BrokerPort": 1884, "Topic": "otgw/#",
                      "Username": "otgw", "MqttPassword": "kept-secret", "WifiSsid": "bench"}
            settings.write_text(json.dumps(seeded), encoding="utf-8")
            env = dict(os.environ, OTGW_CAPTURE_SETTINGS=str(settings))
            r = subprocess.run([sys.executable, str(TOOL), "--device", "127.0.0.1", "--broker", "", "--duration", "2",
                                "--output-root", tmp, "--skip-browser-capture", "--skip-crashlog-capture", "--skip-http-probes",
                                "--telnet-idle-check-seconds", "0", "--telnet-port", str(dev.telnet_port),
                                "--http-port", str(dev.http_port)], stdin=subprocess.DEVNULL, capture_output=True, env=env, timeout=60)
            self.assertEqual(r.returncode, 0, r.stdout.decode(errors="replace"))
            saved = json.loads(settings.read_text(encoding="utf-8"))
            self.assertEqual(saved, dict(seeded, DeviceHost="127.0.0.1"))

    def test_q_browser_child_dies_with_the_tool(self):
        if not (os.name == "nt" or sys.platform.startswith("linux")):
            self.skipTest("kill-with-parent is implemented for Windows and Linux")
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        pidfile = Path(tmp.name) / "browser.pid"
        fake = Path(tmp.name) / "fake_browser.py"
        fake.write_text("import os,sys,time\nopen(sys.argv[1],'w').write(str(os.getpid()))\ntime.sleep(300)\n", encoding="utf-8")
        if os.name == "nt":
            launcher = Path(tmp.name) / "fake_browser.cmd"
            launcher.write_text(f'@"{sys.executable}" "{fake}" "{pidfile}"\r\n', encoding="utf-8")
            browser = launcher
        else:
            launcher = Path(tmp.name) / "fake_browser.sh"
            launcher.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{fake}" "{pidfile}"\n', encoding="utf-8")
            launcher.chmod(0o755)
            browser = launcher
        dev = FakeDevice()
        self.addCleanup(dev.shutdown)
        env = dict(os.environ, OTGW_CAPTURE_SETTINGS=str(Path(tmp.name) / "s.json"))
        proc = subprocess.Popen([sys.executable, str(TOOL), "--device", "127.0.0.1", "--broker", "",
                                 "--telnet-port", str(dev.telnet_port), "--http-port", str(dev.http_port),
                                 "--output-root", str(Path(tmp.name) / "out"), "--browser-path", str(browser),
                                 "--skip-crashlog-capture", "--skip-http-probes", "--telnet-idle-check-seconds", "0"],
                                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        self.assertTrue(wait_for(lambda: pidfile.exists() and pidfile.read_text().strip(), 15), "fake browser never started")
        bpid = int(pidfile.read_text())
        proc.kill()  # hard kill: no cleanup code in the tool runs
        proc.wait()
        self.assertTrue(wait_for(lambda: not pid_alive(bpid), 10), "browser child %d outlived the killed tool" % bpid)


# ---- TASK-1523: hourly segments, zips, index, disk floor -------------------

sys.path.insert(0, str(REPO / "scripts"))
import zipfile  # noqa: E402
from datetime import datetime  # noqa: E402
from unittest import mock  # noqa: E402
import otgw_capture as oc  # noqa: E402


class FakeClock:
    def __init__(self, start):
        self.now = start

    def __call__(self):
        return self.now


def zip_lines(zip_path, member):
    with zipfile.ZipFile(zip_path) as z:
        return z.read(member).decode("utf-8").splitlines()


class SegmentStoreCase(unittest.TestCase):
    """The rotating writer of the long-run mode (TASK-1523), driven by an injected
    clock. No device; threads only in the one test that wants them."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.run = Path(self.tmp.name)
        self.clock = FakeClock(datetime(2026, 10, 31, 10, 59, 58))
        self.errors = []
        self.store = oc.SegmentStore(self.run, hourly=True, now_fn=self.clock, background=False,
                                     on_error=self.errors.append, retry_seconds=0)

    def test_two_hours_make_two_zips_with_every_line_once(self):
        st = self.store
        st.register("telnet.log")
        st.register("mqtt.log", header="# mqtt header\n")
        sent = []
        for i in range(5):
            st.write("telnet.log", f"t{i}\n")
            st.write("mqtt.log", f"m{i}\n")
            sent += [f"t{i}", f"m{i}"]
        self.clock.now = datetime(2026, 10, 31, 11, 0, 1)    # hour boundary
        for i in range(5, 9):
            st.write("telnet.log", f"t{i}\n")
            sent.append(f"t{i}")
        st.close()                                             # packs the partial last hour
        z10, z11 = self.run / "hours" / "20261031-10.zip", self.run / "hours" / "20261031-11.zip"
        self.assertTrue(z10.is_file() and z11.is_file())
        got = []
        for z in (z10, z11):
            with zipfile.ZipFile(z) as zf:
                self.assertEqual(zf.testzip(), None)
                self.assertTrue(all(i.compress_type == zipfile.ZIP_DEFLATED for i in zf.infolist()))
                for name in zf.namelist():
                    got += [l for l in zf.read(name).decode().splitlines() if not l.startswith("#")]
        self.assertEqual(sorted(got), sorted(sent))
        self.assertEqual(len(got), len(set(got)))
        self.assertEqual(zip_lines(z10, "mqtt.log")[0], "# mqtt header")
        live = self.run / "live"
        self.assertEqual(list(live.iterdir()) if live.exists() else [], [])
        self.assertEqual(self.errors, [])

    def test_zip_is_verified_before_the_plain_files_are_deleted(self):
        st = self.store
        st.register("telnet.log")
        st.write("telnet.log", "keep me\n")
        self.clock.now = datetime(2026, 10, 31, 11, 0, 1)
        with mock.patch.object(zipfile.ZipFile, "testzip", return_value="telnet.log"):
            st.tick()                                          # rotation: pack fails verification
        live_dir = self.run / "live" / "20261031-10"
        self.assertTrue((live_dir / "telnet.log").is_file(), "plain file must survive a failed verification")
        self.assertFalse((self.run / "hours" / "20261031-10.zip").exists())
        self.assertTrue(self.errors, "a failed verification must be reported")
        st.tick()                                              # next tick retries the stale hour, verification ok now
        self.assertTrue((self.run / "hours" / "20261031-10.zip").is_file())
        self.assertFalse(live_dir.exists())
        self.assertEqual(zip_lines(self.run / "hours" / "20261031-10.zip", "telnet.log"), ["keep me"])
        st.close()

    def test_index_csv_rows(self):
        st = self.store
        st.register("telnet.log")
        st.register("otframes.log")
        st.write("telnet.log", "a\nb\n")
        st.write("otframes.log", "f1\n")
        st.note_gap("telnet.log")
        self.clock.now = datetime(2026, 10, 31, 11, 5, 0)
        st.write("telnet.log", "c\n")
        st.close()
        rows = [l.split(",") for l in (self.run / "index.csv").read_text().splitlines()]
        self.assertEqual(rows[0], ["hour", "stream", "lines", "bytes", "first_ts", "last_ts", "gaps"])
        by = {(r[0], r[1]): r for r in rows[1:]}
        t10 = by[("20261031-10", "telnet.log")]
        self.assertEqual((t10[2], t10[3], t10[6]), ("2", "4", "1"))
        self.assertTrue(t10[4].startswith("2026-10-31T10:59:58") and t10[5].startswith("2026-10-31T10:59:58"))
        self.assertEqual(by[("20261031-10", "otframes.log")][2], "1")
        self.assertEqual(by[("20261031-11", "telnet.log")][2], "1")
        self.assertEqual(len(rows), 4)

    def test_no_line_lost_with_concurrent_writers_across_the_boundary(self):
        st = self.store
        st.register("mqtt.log")
        self.clock.now = datetime(2026, 10, 31, 10, 30, 0)
        total, threads = 400, 4
        flipped = threading.Event()

        def writer(n):
            for i in range(total):
                if n == 0 and i == total // 2 and not flipped.is_set():
                    self.clock.now = datetime(2026, 10, 31, 11, 0, 5)
                    flipped.set()
                st.write("mqtt.log", f"w{n}-{i}\n")

        ts = [threading.Thread(target=writer, args=(n,)) for n in range(threads)]
        for t in ts:
            t.start()
        for t in ts:
            t.join()
        st.close()
        got = []
        for z in sorted((self.run / "hours").glob("*.zip")):
            got += zip_lines(z, "mqtt.log")
        self.assertEqual(len(got), total * threads)
        self.assertEqual(len(set(got)), total * threads)
        self.assertEqual(len(list((self.run / "hours").glob("*.zip"))), 2)

    def test_short_mode_keeps_plain_files_in_the_run_folder(self):
        st = oc.SegmentStore(self.run, hourly=False, now_fn=self.clock)
        st.register("telnet.log")
        st.write("telnet.log", "x\n")
        self.clock.now = datetime(2026, 10, 31, 12, 0, 0)
        st.write("telnet.log", "y\n")
        st.close()
        self.assertEqual((self.run / "telnet.log").read_text(), "x\ny\n")
        self.assertFalse((self.run / "hours").exists())


class DiskGuardCase(unittest.TestCase):
    def test_floor_triggers_a_clean_stop(self):
        with tempfile.TemporaryDirectory() as tmp:
            cap = oc.Capture(tmp, echo=False)
            free = [5 * 1024 ** 3]
            guard = oc.DiskGuard(tmp, 1024 ** 3, free_fn=lambda path: free[0])
            self.assertTrue(guard.check(cap))
            self.assertFalse(cap.stopping())
            free[0] = 900 * 1024 ** 2
            self.assertFalse(guard.check(cap))
            self.assertTrue(cap.stopping())
            self.assertIn("free disk space", cap.stop_reason)
            self.assertIn("free disk space", (Path(tmp) / "summary.txt").read_text())

    def test_estimate_line_names_the_floor(self):
        line = oc.disk_estimate_line(72 * 3600, 1024 ** 3)
        self.assertIn("1.0 GB", line)
        self.assertIn("72", line)


def long_run_writes_hour_zip_index_and_short_transcript(self):
    self.start(extra=["--long-run", "--snapshot-seconds", "1"])
    time.sleep(3)
    self.dev.send_all("2026-10-31 SAT line one\r\n")
    time.sleep(1)
    self.stop()
    run = self.run_dir()
    zips = sorted((run / "hours").glob("*.zip"))
    self.assertTrue(zips, list(run.rglob("*")))
    members = {}
    for zp in zips:   # a run that happens to cross an hour boundary has two zips
        with zipfile.ZipFile(zp) as z:
            for name in z.namelist():
                members[name] = members.get(name, "") + z.read(name).decode()
    self.assertIn("telnet.log", members)
    self.assertIn("SAT line one", members["telnet.log"])
    self.assertIn("/api/v2/device/info", members["snapshots.log"])
    self.assertIn("/api/v2/sat?detail=full", members["snapshots.log"])
    index = (run / "index.csv").read_text().splitlines()
    self.assertEqual(index[0], "hour,stream,lines,bytes,first_ts,last_ts,gaps")
    self.assertTrue(any(",telnet.log," in l for l in index))
    self.assertFalse((run / "live").exists() and any((run / "live").iterdir()))
    transcript = self.transcript()
    self.assertIn("HOUR INDEX (index.csv)", transcript)
    self.assertIn("LAST HOUR TAIL", transcript)
    self.assertEqual(self.rc, 0)
    upload = run.parent / (run.name + "-upload.zip")
    self.assertTrue(upload.is_file(), "the single upload file is missing")
    with zipfile.ZipFile(upload) as z:
        names = z.namelist()
    self.assertTrue(any(n.startswith("hours/") and n.endswith(".zip") for n in names), names)
    self.assertTrue({"index.csv", "summary.txt"} <= set(names) and any(n.startswith("transcript-") for n in names), names)
    out = self.output
    self.assertEqual(out.strip().splitlines()[-1], "Upload this one file: " + str(upload), out)


ToolCase.test_s_long_run_writes_hour_zip_index_and_short_transcript = long_run_writes_hour_zip_index_and_short_transcript


import test_capture_otframes as tco  # noqa: E402


class TwoShotWs(tco.FakeWs):
    """/ws fake that serves one batch per connection, closing after each."""

    def __init__(self, batches):
        self.batches = batches
        super().__init__([])

    def run(self):
        for batch in self.batches:
            self.messages = batch
            tco.FakeWs.run(self)


class FrameGapCase(unittest.TestCase):
    def test_ws_drop_leaves_a_gap_marker_and_an_index_gap(self):
        ws = TwoShotWs([[tco.LINES["T"]], [tco.LINES["B"]]])
        with tempfile.TemporaryDirectory() as tmp:
            cap = oc.Capture(tmp, echo=False, hourly=True)
            self.addCleanup(cap.store.close)   # idempotent; releases the files before the folder is removed
            t = oc.FrameCapture(cap, "127.0.0.1", ws.port)
            t.start()
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline and not (cap.path("gaps.log").exists() and "T" in cap.path("gaps.log").read_text()):
                time.sleep(0.2)
            time.sleep(1.0)
            cap.request_stop("test")
            t.join(timeout=15)
            self.assertFalse(t.is_alive())
            gaps = cap.path("gaps.log").read_text()
            self.assertRegex(gaps, r"\totframes\.log\t\S")   # reason: closed by the device, or the reset the OS reports
            self.assertEqual(len(gaps.splitlines()), 1, gaps)
            cap.store.close()
            text = "".join(zipfile.ZipFile(z).read("otframes.log").decode() for z in sorted((Path(tmp) / "hours").glob("*.zip")))
            self.assertRegex(text, r"# \S+ GAP start=\S+ end=\S+ secs=\d+ reason=\S")
            frames = [l.split("\t")[2] + l.split("\t")[3] for l in text.splitlines() if not l.startswith("#")]
            self.assertEqual(frames, ["T10010A00", "B40000800"])
            idx = (Path(tmp) / "index.csv").read_text()
            self.assertRegex(idx, r"otframes\.log,\d+,\d+,\S+,\S+,1\n")


def long_run_telnet_gap_is_in_marker_gaps_and_index(self):
    self.start(idle=0, extra=["--long-run", "--snapshot-seconds", "0"])
    time.sleep(0.5)
    self.dev.reset_connections()
    self.assertTrue(wait_for(lambda: "Telnet gap:" in self.text(), 15), self.text())
    self.stop()
    run = self.run_dir()
    t = self.transcript()
    self.assertRegex(section(t, "GAP MARKERS (gaps.log)"), r"\ttelnet\.log\t(read failed|connection closed)")
    members = {}
    for zp in (run / "hours").glob("*.zip"):
        with zipfile.ZipFile(zp) as z:
            for name in z.namelist():
                members[name] = members.get(name, "") + z.read(name).decode()
    self.assertRegex(members["telnet.log"], r"reconnected after \d+ s gap \([a-z ]+\); gap start=\S+ end=\S+")
    self.assertNotIn("snapshots.log", members)   # --snapshot-seconds 0 switches them off
    self.assertRegex((run / "index.csv").read_text(), r"telnet\.log,\d+,\d+,\S+,\S+,1\n")


ToolCase.test_t_long_run_telnet_gap_marker_gaps_log_and_index = long_run_telnet_gap_is_in_marker_gaps_and_index


class HourCsvCase(unittest.TestCase):
    """TASK-1523 AC #8/#9: CSVs inside the hour zip, one upload zip, and the hardening of the hour packer."""

    SAT_LINE = "10:59:58.123456 SATTICK room=20.5 target=21.0 out=40 mode=heat\n"
    FRAME_T = "2026-10-31T10:59:58.100+01:00\t10:59:58.100000\tT\t10010A00\n"
    FRAME_B = "2026-10-31T10:59:58.350+01:00\t10:59:58.350000\tB\t40000800\n"

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.run = Path(self.tmp.name) / "run"
        self.run.mkdir()
        self.clock = FakeClock(datetime(2026, 10, 31, 10, 59, 58))
        self.errors = []
        self.store = oc.SegmentStore(self.run, hourly=True, now_fn=self.clock, background=False,
                                     on_error=self.errors.append, retry_seconds=0)
        self.store.register("otframes.log", header=oc.FRAMES_HEADER + "\n", newline="\n")
        self.store.register("mqtt.log", header=oc.MQTT_HOURLY_HEADER)

    def fill_hour(self):
        st = self.store
        st.write("telnet.log", "boot noise\n" + self.SAT_LINE)
        st.write("otframes.log", self.FRAME_T)
        st.write("otframes.log", self.FRAME_B)
        st.write("otframes.log", "# 2026-10-31T10:59:59+01:00 GAP start=a end=b secs=1 reason=test\n")
        st.write("mqtt.log", "2026-10-31T10:59:58.200+01:00\t1\totgw/set/x\t{\"a\": 1}\n")

    def test_hour_zip_holds_the_csv_files(self):
        self.fill_hour()
        self.store.close()
        z = self.run / "hours" / "20261031-10.zip"
        with zipfile.ZipFile(z) as zf:
            names = set(zf.namelist())
        self.assertTrue({"telnet.log", "otframes.log", "mqtt.log", "sattick.csv", "otframes.csv", "mqtt.csv"} <= names, names)
        self.assertNotIn("satboot.csv", names)          # empty kinds are skipped
        self.assertEqual(zip_lines(z, "sattick.csv"), ["time,room,target,out,mode", "10:59:58.123456,20.5,21.0,40,heat"])
        self.assertEqual(zip_lines(z, "otframes.csv"), [
            "host_time,device_time,source,frame,msgid,type,value_hb,value_lb,f88,u16",
            "2026-10-31T10:59:58.100+01:00,10:59:58.100000,T,10010A00,1,WRITE-DATA,10,0,10.0,2560",
            "2026-10-31T10:59:58.350+01:00,10:59:58.350000,B,40000800,0,READ-ACK,8,0,8.0,2048"])
        self.assertEqual(zip_lines(z, "mqtt.csv"), [
            "host_time,retain,topic,payload",
            '2026-10-31T10:59:58.200+01:00,1,otgw/set/x,"{""a"": 1}"'])
        self.assertEqual(self.errors, [])
        self.assertFalse(list(self.run.glob("csv-*.tmp")), "temporary CSV folder must be removed")

    def test_signed_f88_value(self):
        word = oc.decode_ot_frame("1001FF00")   # 0xFF00 = -1.0
        self.assertEqual((word[0], word[1], word[4], word[5]), (1, "WRITE-DATA", -1.0, 0xFF00))
        self.assertIsNone(oc.decode_ot_frame("xyz"))

    def test_conversion_error_never_blocks_the_zip(self):
        self.fill_hour()
        with mock.patch.object(oc, "convert_otframes_csv", side_effect=ValueError("boom")):
            self.store.close()
        z = self.run / "hours" / "20261031-10.zip"
        with zipfile.ZipFile(z) as zf:
            names = set(zf.namelist())
        self.assertTrue({"telnet.log", "otframes.log", "mqtt.log", "sattick.csv", "mqtt.csv"} <= names, names)
        self.assertNotIn("otframes.csv", names)
        self.assertEqual(len([e for e in self.errors if "csv conversion of otframes.log failed" in e]), 1, self.errors)
        self.assertFalse((self.run / "live" / "20261031-10").exists())

    def test_upload_zip_long_run(self):
        self.fill_hour()
        self.clock.now = datetime(2026, 10, 31, 11, 0, 5)
        self.store.write("telnet.log", "second hour\n")
        self.store.close()
        cap = oc.Capture(self.run, echo=False, store=self.store)
        cap.summary("ResolvedHostname: bench")
        cap.path(oc.GAPS_LOG).write_text("gap line\n")
        transcript = self.run / "transcript-x.txt"
        transcript.write_text("short transcript\n")
        upload = oc.build_upload_zip(cap, transcript, True)
        self.assertEqual(upload, Path(self.tmp.name) / "run-upload.zip")
        with zipfile.ZipFile(upload) as zf:
            names = set(zf.namelist())
            self.assertEqual(zf.testzip(), None)
            self.assertTrue({"hours/20261031-10.zip", "hours/20261031-11.zip", "index.csv", "gaps.log", "summary.txt",
                             "transcript-x.txt"} <= names, names)
            inner = zipfile.ZipFile(io.BytesIO(zf.read("hours/20261031-10.zip")))
            self.assertIn("sattick.csv", inner.namelist())
        self.assertFalse(list(self.run.glob("csv-*.tmp")))

    def test_upload_zip_short_run_holds_transcript_frames_and_csv(self):
        store = oc.SegmentStore(self.run, hourly=False)
        store.register("otframes.log", header=oc.FRAMES_HEADER + "\n", newline="\n")
        store.write("telnet.log", self.SAT_LINE)
        store.write("otframes.log", self.FRAME_T)
        store.close()
        cap = oc.Capture(self.run, echo=False, store=store)
        transcript = self.run / "transcript-y.txt"
        transcript.write_text("merged\n")
        upload = oc.build_upload_zip(cap, transcript, False)
        with zipfile.ZipFile(upload) as zf:
            names = set(zf.namelist())
        self.assertTrue({"transcript-y.txt", "otframes.log", "csv/sattick.csv", "csv/otframes.csv"} <= names, names)

    def test_failed_delete_does_not_pack_the_hour_twice(self):
        self.fill_hour()
        self.clock.now = datetime(2026, 10, 31, 11, 0, 5)
        with mock.patch.object(oc.shutil, "rmtree", lambda *a, **k: None):   # the delete of the plain files fails
            self.store.tick()
        hours = self.run / "hours"
        self.assertEqual([p.name for p in hours.glob("*.zip")], ["20261031-10.zip"])
        leftover = self.run / "live" / "20261031-10.packed"
        self.assertTrue(leftover.is_dir(), "the half-deleted hour must be recognisable")
        self.assertTrue(any("could not delete" in e for e in self.errors), self.errors)
        for _ in range(3):                                   # retries while the delete keeps failing
            with mock.patch.object(oc.shutil, "rmtree", lambda *a, **k: None):
                self.store.tick()
        self.assertEqual([p.name for p in hours.glob("*.zip")], ["20261031-10.zip"])
        self.store.tick()                                    # delete works again
        self.assertFalse(leftover.exists())
        self.assertEqual([p.name for p in hours.glob("*.zip")], ["20261031-10.zip"])
        index = (self.run / "index.csv").read_text().splitlines()
        self.assertEqual(len([l for l in index if l.startswith("20261031-10,telnet.log")]), 1, index)
        self.store.close()

    def test_clock_jump_back_never_writes_into_a_packed_hour(self):
        self.store.write("telnet.log", "hour ten\n")
        self.clock.now = datetime(2026, 10, 31, 11, 0, 5)
        self.store.write("telnet.log", "hour eleven\n")      # hour ten is packed now
        self.clock.now = datetime(2026, 10, 31, 10, 20, 0)   # NTP step back across the hour
        self.store.write("telnet.log", "after the jump\n")
        self.store.close()
        zips = sorted(p.name for p in (self.run / "hours").glob("*.zip"))
        self.assertEqual(zips, ["20261031-10.zip", "20261031-11.zip"])
        self.assertEqual(zip_lines(self.run / "hours" / zips[0], "telnet.log"), ["hour ten"])
        self.assertEqual(zip_lines(self.run / "hours" / zips[1], "telnet.log"), ["hour eleven", "after the jump"])
        self.assertEqual(self.errors, [])

    def test_zip_is_flushed_to_disk_before_it_is_moved_into_place(self):
        self.store.write("telnet.log", "x\n")
        with mock.patch.object(oc.os, "fsync") as fsync:
            self.store.close()
        self.assertTrue(fsync.called)
        self.assertTrue((self.run / "hours" / "20261031-10.zip").is_file())


class TelnetBufferCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.run = Path(self.tmp.name)
        self.clock = FakeClock(datetime(2026, 10, 31, 10, 59, 58))
        store = oc.SegmentStore(self.run, hourly=True, now_fn=self.clock, background=False)
        self.cap = oc.Capture(self.run, echo=False, store=store)
        self.t = oc.TelnetCapture(self.cap, "127.0.0.1", 23, "http://127.0.0.1", 2, 0.25, 0.25, 0)
        self.t.writer = oc.StreamWriter(store, "telnet.log")

    def test_a_line_is_never_split_over_two_hours(self):
        self.t.emit("12:00:00 SAT")                  # no newline yet: nothing written
        self.assertFalse(list((self.run / "live").glob("*/telnet.log")))
        self.clock.now = datetime(2026, 10, 31, 11, 0, 1)
        self.t.emit("TICK a=1\nnext li")
        self.t.emit("ne\n")
        self.t.emit("tail without newline")
        self.t.flush_partial()                       # the link ends inside a line
        self.cap.store.close()
        z10, z11 = self.run / "hours" / "20261031-10.zip", self.run / "hours" / "20261031-11.zip"
        self.assertFalse(z10.exists(), "nothing was written in hour ten")
        self.assertEqual(zip_lines(z11, "telnet.log"), ["12:00:00 SATTICK a=1", "next line", "tail without newline"])

    def test_metadata_is_kept_in_memory(self):
        self.t.emit("hostname: bench-otgw\nversion: 2.0.0-alpha.9\nunique_id: otgw-ABC\n")
        self.assertEqual(self.cap.metadata_seen, {"hostname": "bench-otgw", "firmware_version": "2.0.0-alpha.9",
                                                  "unique_id": "otgw-ABC"})
        self.cap.store.close()   # the hour with the banner is gone from live/ now
        name = oc.resolve_metadata(self.cap, "127.0.0.1", "http://127.0.0.1:1", "20261031-100000")
        self.assertIn("bench-otgw", name)
        self.assertIn("2.0.0-alpha.9", name)
        self.assertIn("otgw-ABC", name)


class FakeSock:
    def __init__(self):
        self.sent = []

    def sendall(self, data):
        self.sent.append(data)


class SatTraceToggleCase(unittest.TestCase):
    BANNER = "  1 OTmsg [0]\n  5 SATlog [0]\n  8 SATtrace [0]\nPress 'h' for command menu\n"

    def run_toggles(self, include_trace):
        with tempfile.TemporaryDirectory() as tmp:
            cap = oc.Capture(tmp, echo=False)
            t = oc.TelnetCapture(cap, "127.0.0.1", 23, "http://127.0.0.1", 2, 0.25, 0.25, 0)
            t.sock = FakeSock()
            t.read_available = lambda: ""
            t.include_trace = include_trace
            result = t.enable_debug_toggles(self.BANNER)
            cap.store.close()
            return t.sock.sent, result

    def test_trace_is_left_off_by_default(self):
        sent, result = self.run_toggles(False)
        self.assertEqual(sent, [b"1", b"5"])
        self.assertIn("SATtrace=left off", result)

    def test_trace_goes_on_with_the_option(self):
        sent, result = self.run_toggles(True)
        self.assertEqual(sent, [b"1", b"5", b"8"])
        self.assertIn("SATtrace=on", result)


def pid_alive(pid):
    if os.name == "nt":
        import ctypes
        k = ctypes.windll.kernel32
        h = k.OpenProcess(0x1000, 0, pid)
        if not h:
            return False
        code = ctypes.c_ulong()
        k.GetExitCodeProcess(h, ctypes.byref(code))
        k.CloseHandle(h)
        return code.value == 259
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    try:
        with open(f"/proc/{pid}/stat") as f:
            return f.read().split()[2] != "Z"
    except OSError:
        return True


if __name__ == "__main__":
    unittest.main(verbosity=2)
