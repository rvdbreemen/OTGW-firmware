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
        self.assertIsNotNone(self.transcript(), "no transcript written:\n" + self.proc.stdout.read().decode(errors="replace"))
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
