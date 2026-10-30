#!/usr/bin/env python3
"""OTGW diagnostic capture: telnet debug, MQTT, browser devtools, crash log and
HTTP probes, merged into one transcript file (ADR-202, TASK-1475). A raw
OpenTherm frame recorder writes otframes.log and otgw_simulation.log next to it
(TASK-1503).

This is the one implementation behind the launchers capture-mqtt-debug.bat
(Windows), capture-mqtt-debug.sh (Linux/macOS) and capture-mqtt-debug-macos.sh.
Standard library only, Python 3.9+. MQTT uses paho-mqtt, imported straight from
the vendored wheel in vendor/ next to this file; without it the MQTT stream is
skipped and every other source still runs.

Run with --help for the options. Both spellings work: --device / -DeviceHost.
"""

import argparse
import base64
import ctypes
import json
import os
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
PAHO_WHEEL = "paho_mqtt-2.1.0-py3-none-any.whl"
PAHO_WHEEL_SHA256 = "6db9ba9b34ed5bc6b6e3812718c7e06e2fd7444540df2455d2c51bd58808feee"

SECTION_RULE = "=" * 60
TRANSCRIPT_SECTIONS = (
    ("SUMMARY (summary.txt)", "summary.txt"),
    ("TOOL ERRORS (error.txt)", "error.txt"),
    ("OTGW TELNET DEBUG (telnet.log)", "telnet.log"),
    ("MQTT BROKER STREAM (mqtt.log)", "mqtt.log"),
    ("BROWSER DEVTOOLS (browser.log)", "browser.log"),
    ("DEVICE CRASH LOG (crashlog.log)", "crashlog.log"),
    ("HTTP PROBES (curl-probes.log)", "curl-probes.log"),
)
ERROR_SECTIONS = (
    ("SCRIPT / CAPTURE ERRORS (script.error.log)", "script.error.log"),
    ("MQTT STDERR (mqtt.stderr.log)", "mqtt.stderr.log"),
    ("BROWSER STDERR (browser.stderr.log)", "browser.stderr.log"),
)
INTERMEDIATE_FILES = (
    "summary.txt", "telnet.log", "mqtt.log", "mqtt.stderr.log", "browser.log",
    "browser.stderr.log", "crashlog.log", "curl-probes.log", "script.error.log",
    "error.txt", ".stop",
)
PROBE_PATHS = (
    "/", "/index.html", "/v2.js", "/api/v2/health", "/api/v2/device/info",
    "/api/v2/device/time", "/api/v2/flash/status", "/api/v2/sat/status",
)
# TASK-1396: these probe bodies are kept whole (up to the cap), so fields such as
# hd_min_free_heap / hd_min_max_block in device/info reach the transcript.
PROBE_FULL_BODY = {"/api/v2/health", "/api/v2/device/info"}
PROBE_FULL_BODY_CAP = 8192
PROBE_SNIPPET = 200

REDACT_PATTERNS = (
    re.compile(r"(?i)((?:password|passwd|mqttpasswd|mqtt_password|mqtpassword|token|secret|api[_-]?key)\s*[:=]\s*)('([^']*)'|\"([^\"]*)\"|[^,\s}]+)"),
    re.compile(r"(?i)((?:-P|--password)\s+)(\S+)"),
)

SIMULATOR_LABELS = {"SensorSim", "OTGW-Sim"}
DEBUG_TOGGLE = re.compile(
    r"(?:^|\s)(?P<key>\S)\s+"
    r"(?P<label>[A-Za-z][A-Za-z0-9 /]*?)\s*"
    r"\[(?P<state>[01])\]",
    re.MULTILINE,
)
BANNER_FOOTER = re.compile(r"Press 'h' for command menu|\bOTGW-Sim\b", re.MULTILINE)
REBOOT_MARKER = re.compile(r"ESP\.restart\(\)")
SETUP_FINISHED = "Setup finished!"
UPTIME = re.compile(r"(\d+)\(d\)-(\d+):(\d+)\(H:m\)")

# TASK-1503: raw OpenTherm frame recorder. Source: the device's /ws (the OT log
# WebSocket of the web UI), which is on by default on PIC and OT-Direct boards.
FRAMES_LOG = "otframes.log"
SIM_LOG = "otgw_simulation.log"
FRAMES_HEADER = ("# OTGW raw frame log (TASK-1503). Columns, tab separated: "
                 "host_time (receive time on this computer, ISO-8601, ms, local time with UTC offset) | "
                 "device_time (HH:MM:SS.uuuuuu, device local time as printed in its OT log; the device logs a "
                 "frame one frame late, so both times belong to the NEXT frame's arrival: the difference "
                 "between consecutive lines is the frame gap, shifted by one frame) | "
                 "source letter (T B R A E) | 8 hex digits. Lines starting with # are notes (connects, disconnects).")
# One line of the device OT log: "HH:MM:SS.uuuuuu <direction label> <L><8 hex> <id> ..." (OTGW-Core.ino processOT()).
OT_LOG_LINE = re.compile(
    r"^(\d{2}:\d{2}:\d{2}\.\d{6}) (?:Boiler|Thermostat|Request Boiler|Answer Thermostat|Parity Error|Unknown)\s+"
    r"([A-Z])([0-9A-F]{8})\s+\d+(?:\s|$)")
WS_RECONNECT_MIN = 3.0
WS_RECONNECT_MAX = 30.0

KEEPALIVE_IDLE = 10
KEEPALIVE_INTERVAL = 5
KEEPALIVE_COUNT = 3


def now_iso():
    return datetime.now().astimezone().isoformat(timespec="seconds")


def clock():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def redact(text):
    for pattern in REDACT_PATTERNS:
        text = pattern.sub(r"\1[REDACTED]", text)
    return text


# --------------------------------------------------------------------------- #
# Run context: paths, summary, stop signal
# --------------------------------------------------------------------------- #

class Capture:
    """Shared state of one capture run. Every worker writes through this object."""

    def __init__(self, run_path, echo=True):
        self.run_path = Path(run_path)
        self.stop_event = threading.Event()
        self.stop_reason = None
        self.echo = echo
        self._lock = threading.Lock()

    def path(self, name):
        return self.run_path / name

    def summary(self, line, echo=None):
        with self._lock:
            try:
                with open(self.path("summary.txt"), "a", encoding="utf-8") as f:
                    f.write(line + "\n")
            except OSError:
                pass  # the run folder is gone; nothing to write into
        if self.echo if echo is None else echo:
            print(line, flush=True)

    def script_error(self, line):
        with self._lock:
            try:
                with open(self.path("script.error.log"), "a", encoding="utf-8") as f:
                    f.write(f"{now_iso()}  {line}\n")
            except OSError:
                pass

    def request_stop(self, reason):
        if self.stop_reason is None:
            self.stop_reason = reason
        self.stop_event.set()

    def stopping(self):
        if not self.stop_event.is_set() and self.path(".stop").exists():
            self.request_stop("stop file")
        return self.stop_event.is_set()

    def wait(self, seconds):
        """Sleep up to `seconds`, returning early when the capture stops."""
        end = time.monotonic() + seconds
        while not self.stopping():
            left = end - time.monotonic()
            if left <= 0:
                return
            self.stop_event.wait(min(0.1, left))


def http_get(url, timeout, max_bytes=None):
    """Returns (status or None, body bytes, error text or None, content type)."""
    request = urllib.request.Request(url, headers={"User-Agent": "OTGW-capture/2"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read() if max_bytes is None else response.read(max_bytes)
            return response.status, body, None, response.headers.get("content-type", "-")
    except urllib.error.HTTPError as exc:
        try:
            body = exc.read()
        except Exception:
            body = b""
        ctype = exc.headers.get("content-type", "-") if exc.headers else "-"
        return exc.code, body, str(exc), ctype
    except Exception as exc:
        return None, b"", f"{type(exc).__name__}: {exc}", "-"


# --------------------------------------------------------------------------- #
# Telnet debug stream (TASK-1299 reconnect/keepalive/watchdog, TASK-1396, TASK-1453)
# --------------------------------------------------------------------------- #

def parse_toggles(banner):
    """label -> (key, state) for every debug toggle listed in a telnet banner."""
    found = {}
    for match in DEBUG_TOGGLE.finditer(banner):
        label = match.group("label").strip()
        if label and label not in found:
            found[label] = (match.group("key"), match.group("state"))
    return found


def parse_uptime_minutes(text):
    m = UPTIME.search(text)
    if not m:
        return None
    return int(m.group(1)) * 1440 + int(m.group(2)) * 60 + int(m.group(3))


def apply_keepalive(sock):
    """Probe an idle connection so a device that rebooted without a FIN is noticed."""
    applied = []
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        applied.append("SO_KEEPALIVE")
    except Exception as exc:
        return f"not applied: {exc}"
    if os.name == "nt" and hasattr(socket, "SIO_KEEPALIVE_VALS"):
        try:
            sock.ioctl(socket.SIO_KEEPALIVE_VALS, (1, KEEPALIVE_IDLE * 1000, KEEPALIVE_INTERVAL * 1000))
            applied.append("SIO_KEEPALIVE_VALS")
        except Exception:
            pass
    have_idle = False
    for name, value in (("TCP_KEEPIDLE", KEEPALIVE_IDLE), ("TCP_KEEPINTVL", KEEPALIVE_INTERVAL), ("TCP_KEEPCNT", KEEPALIVE_COUNT)):
        if hasattr(socket, name):
            try:
                sock.setsockopt(socket.IPPROTO_TCP, getattr(socket, name), value)
                applied.append(name)
                have_idle = have_idle or name == "TCP_KEEPIDLE"
            except Exception:
                pass
    if not have_idle and hasattr(socket, "TCP_KEEPALIVE"):
        try:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPALIVE, KEEPALIVE_IDLE)
            applied.append("TCP_KEEPALIVE")
        except Exception:
            pass
    return ", ".join(applied)


class TelnetCapture(threading.Thread):
    def __init__(self, cap, host, port, http_base, base_timeout, reconnect_delay,
                 post_disconnect_delay, idle_check):
        super().__init__(name="telnet", daemon=True)
        self.cap = cap
        self.host = host
        self.port = port
        self.http_base = http_base
        self.base_timeout = base_timeout
        self.reconnect_delay = reconnect_delay
        self.post_disconnect_delay = post_disconnect_delay
        self.idle_check = idle_check
        self.writer = None
        self.sock = None
        self.last_byte_mono = time.monotonic()
        self.last_byte_wall = datetime.now(timezone.utc).astimezone()
        self.keepalive_reported = False
        self.gap = None
        self.gap_count = 0
        self.gap_total = 0.0
        self.totals_written = False
        # TASK-1453: toggle states from the first banner, and the states now.
        self.initial_toggles = None
        self.current_toggles = {}
        # TASK-1396: q/D wait until setup() has finished.
        self.dump_pending = False
        self.dump_waiting_reported = False
        self.setup_seen = False
        self.last_setup_probe = 0.0

    # -- helpers -------------------------------------------------------------
    def effective_timeout(self, attempt):
        return min(self.base_timeout + min(max(attempt - 1, 0), 6), 20)

    def read_available(self):
        chunks = []
        while not self.cap.stopping():
            ready, _, _ = select.select([self.sock], [], [], 0)
            if not ready:
                break
            data = self.sock.recv(4096)
            if not data:
                raise ConnectionError("remote closed connection")
            self.last_byte_mono = time.monotonic()
            self.last_byte_wall = datetime.now(timezone.utc).astimezone()
            text = data.decode("ascii", "replace")
            self.writer.write(text)
            self.writer.flush()
            chunks.append(text)
        return "".join(chunks)

    def read_banner(self, timeout):
        chunks = []
        end = time.monotonic() + timeout
        while not self.cap.stopping() and time.monotonic() < end:
            try:
                text = self.read_available()
            except BlockingIOError:
                text = ""
            if text:
                chunks.append(text)
                if BANNER_FOOTER.search("".join(chunks)):
                    break
            self.cap.wait(0.1)
        return "".join(chunks)

    def send_key_and_drain(self, key, timeout=5.0, idle_seconds=0.8):
        self.sock.sendall(key.encode("ascii"))
        deadline = time.monotonic() + timeout
        last_data = time.monotonic()
        while not self.cap.stopping() and time.monotonic() < deadline:
            try:
                text = self.read_available()
            except BlockingIOError:
                text = ""
            if text:
                last_data = time.monotonic()
                if SETUP_FINISHED in text:
                    self.setup_seen = True
            elif time.monotonic() - last_data >= idle_seconds:
                break
            self.cap.wait(0.1)

    def health_probe(self):
        """(alive, uptime minutes or None). Any HTTP answer (503 from the request
        gate, 429) means the device is alive; only connection errors and
        timeouts count as a failed probe."""
        status, body, _err, _ctype = http_get(self.http_base + "/api/v2/health", 5, 4096)
        if status is None:
            return False, None
        if status != 200:
            return True, None
        return True, parse_uptime_minutes(body.decode("utf-8", "replace"))

    # -- debug toggles (enable on connect, restore at stop) -------------------
    def enable_debug_toggles(self, banner):
        toggles = parse_toggles(banner)
        if toggles:
            self.current_toggles = dict(toggles)
            if self.initial_toggles is None:
                self.initial_toggles = dict(toggles)
        results = []
        for label, (key, state) in toggles.items():
            if label in SIMULATOR_LABELS:
                results.append(f"{label}=skipped (simulator)")
            elif state == "0":
                self.sock.sendall(key.encode("ascii"))
                self.current_toggles[label] = (key, "1")
                self.cap.wait(0.3)
                try:
                    self.read_available()
                except BlockingIOError:
                    pass
                results.append(f"{label}=on (sent '{key}')")
            else:
                results.append(f"{label}=already-on")
        return "; ".join(results) if results else "no debug toggles found in banner"

    def toggles_to_restore(self):
        return [(label, key) for label, (key, state) in self.initial_toggles.items()
                if label not in SIMULATOR_LABELS
                and label in self.current_toggles
                and self.current_toggles[label][1] != state]

    def restore_debug_toggles(self):
        """TASK-1453: put every non-simulator toggle back in the state of the first
        banner. Bounded, and never changes the capture outcome."""
        if self.initial_toggles is None:
            self.cap.summary("Debug toggles: no toggle banner seen, nothing to restore.")
            return
        todo = self.toggles_to_restore()
        if not todo:
            self.cap.summary("Debug toggles: as found at capture start, nothing to restore.")
            return
        labels = ", ".join(label for label, _ in todo)
        if self.sock is None:
            # Reconnect once to read the real states; the device may have rebooted.
            try:
                self.sock = socket.create_connection((self.host, self.port), timeout=3)
                self.sock.setblocking(False)
                banner = self.read_banner_unstoppable(3)
                fresh = parse_toggles(banner)
                if not fresh:
                    raise ConnectionError("no toggle banner after reconnect")
                self.current_toggles = fresh
                todo = self.toggles_to_restore()
                if not todo:
                    self.cap.summary("Debug toggles: as found at capture start, nothing to restore.")
                    return
                labels = ", ".join(label for label, _ in todo)
            except Exception as exc:
                self.cap.summary(f"Debug toggles restore failed (device likely gone): {labels} ({exc})")
                self.close_socket()
                return
        done = []
        for label, key in todo:
            try:
                self.sock.sendall(key.encode("ascii"))
                time.sleep(0.3)
                try:
                    self.drain_unstoppable()
                except Exception:
                    pass
                done.append(label)
            except Exception as exc:
                failed = ", ".join(label for label, _ in todo if label not in done)
                if done:
                    self.cap.summary(f"Debug toggles restore: partial ({len(done)} of {len(todo)}): restored {', '.join(done)}; failed {failed} ({exc})")
                else:
                    self.cap.summary(f"Debug toggles restore failed (device likely gone): {failed} ({exc})")
                return
        self.cap.summary(f"Debug toggles restored to their state at capture start: {', '.join(done)}")

    def read_banner_unstoppable(self, timeout):
        """read_banner for the stop path: the stop event is already set."""
        chunks = []
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            ready, _, _ = select.select([self.sock], [], [], 0.1)
            if ready:
                data = self.sock.recv(4096)
                if not data:
                    break
                text = data.decode("ascii", "replace")
                self.writer.write(text)
                chunks.append(text)
                if BANNER_FOOTER.search("".join(chunks)):
                    break
        self.writer.flush()
        return "".join(chunks)

    def drain_unstoppable(self):
        while True:
            ready, _, _ = select.select([self.sock], [], [], 0)
            if not ready:
                break
            data = self.sock.recv(4096)
            if not data:
                break
            self.writer.write(data.decode("ascii", "replace"))
        self.writer.flush()

    # -- settings dump (TASK-1396) ---------------------------------------------
    def maybe_send_dump(self):
        """Send q/D once setup() is done: 'Setup finished!' was seen on this
        connection, or the health endpoint reports an uptime above 0 minutes."""
        if not self.dump_pending:
            return
        ready = self.setup_seen
        if not ready and time.monotonic() - self.last_setup_probe >= 5:
            self.last_setup_probe = time.monotonic()
            ok, up = self.health_probe()
            ready = ok and up is not None and up > 0
        if not ready:
            if not self.dump_waiting_reported:
                self.dump_waiting_reported = True
                self.cap.summary("Settings dump: waiting for 'Setup finished!' (or uptime > 0) before sending q/D.")
            return
        self.dump_pending = False
        self.send_key_and_drain("q")
        self.send_key_and_drain("D")
        self.cap.summary("Settings dump: sent 'q' (read settings) + 'D' (dump settings/state)")

    # -- connection management -----------------------------------------------
    def open_gap(self, why, start=None):
        if self.gap is None:
            self.gap = {"start_wall": start or self.last_byte_wall, "reason": why, "tries": 0, "failed": 0}

    def connect(self, attempt):
        timeout = self.effective_timeout(attempt)
        if not (self.gap is not None and self.gap["tries"] > 1):
            self.cap.summary(f"Telnet connect started: {self.host}:{self.port} (timeout {timeout}s)")
        self.sock = socket.create_connection((self.host, self.port), timeout=timeout)
        ka = apply_keepalive(self.sock)
        self.sock.setblocking(False)
        self.cap.summary(f"Telnet connected: {now_iso()}")
        if not self.keepalive_reported:
            self.keepalive_reported = True
            count = "fixed by Windows (10)" if os.name == "nt" else str(KEEPALIVE_COUNT)
            self.cap.summary(f"Telnet keepalive: idle {KEEPALIVE_IDLE}s interval {KEEPALIVE_INTERVAL}s count {count} (applied: {ka})")
        if self.gap is not None:
            self.gap["end_wall"] = datetime.now(timezone.utc).astimezone()
            self.gap["secs"] = int(round(self.gap["end_wall"].timestamp() - self.gap["start_wall"].timestamp()))
            self.writer.write(f"\n### capture: telnet reconnected after {self.gap['secs']} s gap ({self.gap['reason']})\n")
            self.writer.flush()

    def after_connect(self):
        """Banner, toggles and the settings dump. Runs once the link is up, so a
        drop in here counts as a read failure, not as a failed connect."""
        banner = self.read_banner(self.base_timeout)
        self.setup_seen = SETUP_FINISHED in banner
        self.cap.summary(f"Debug toggle actions: {self.enable_debug_toggles(banner)}")
        self.dump_pending = True
        self.dump_waiting_reported = False
        self.last_setup_probe = 0.0
        self.maybe_send_dump()

    def close_socket(self):
        if self.sock is not None:
            try:
                self.sock.close()
            except Exception:
                pass
        self.sock = None

    def write_totals(self):
        if self.totals_written:
            return
        self.totals_written = True
        if self.gap is not None:
            self.gap_count += 1
            self.gap_total += int(round(time.time() - self.gap["start_wall"].timestamp()))
        self.cap.summary(f"Telnet gaps: {self.gap_count}, total {int(round(self.gap_total))} s")
        if self.gap is not None:
            self.cap.summary(f"Telnet gap still open at stop: since {self.gap['start_wall'].isoformat(timespec='seconds')} ({self.gap['reason']})")

    def run(self):
        try:
            with open(self.cap.path("telnet.log"), "a", encoding="utf-8", errors="replace") as writer:
                self.writer = writer
                self.loop()
                try:
                    self.restore_debug_toggles()
                except Exception as exc:
                    self.cap.summary(f"Debug toggles restore failed (device likely gone): {exc}")
                self.close_socket()
        except Exception as exc:
            self.cap.summary(f"Telnet capture: error - {exc}")
            self.cap.script_error(f"Telnet worker error: {type(exc).__name__}: {exc}")
        finally:
            self.close_socket()
            self.write_totals()

    def loop(self):
        state = {"attempts": 0, "next_connect": 0.0, "reason": None, "scan": ""}
        has_connected = False
        last_probe_mono = time.monotonic()
        last_uptime_min = None
        last_ok_probe_wall = self.last_byte_wall

        def drop(why, line, start=None):
            state["reason"] = why
            state["attempts"] = 0
            state["next_connect"] = time.monotonic() + self.post_disconnect_delay
            state["scan"] = ""
            self.dump_pending = False
            self.open_gap(why, start)
            self.cap.summary(line)
            self.close_socket()

        while not self.cap.stopping():
            if self.sock is None:
                if time.monotonic() < state["next_connect"]:
                    self.cap.wait(min(0.25, state["next_connect"] - time.monotonic()))
                    continue
                try:
                    state["attempts"] += 1
                    attempts = state["attempts"]
                    if self.gap is not None:
                        self.gap["tries"] += 1
                    if has_connected and (self.gap is None or self.gap["tries"] == 1):
                        suffix = f" after {state['reason']}" if state["reason"] else ""
                        self.cap.summary(f"Telnet reconnect attempt #{attempts} started{suffix} (timeout {self.effective_timeout(attempts)}s).")
                    self.connect(attempts)
                    state["scan"] = ""
                    if has_connected:
                        self.cap.summary(f"Telnet reconnected after {attempts} attempt(s): {now_iso()}")
                    if self.gap is not None:
                        g = self.gap
                        self.cap.summary(f"Telnet gap: {g['start_wall'].isoformat(timespec='seconds')} -> {g['end_wall'].isoformat(timespec='seconds')} ({g['secs']} s, {g['failed']} failed connect attempts, {g['reason']})")
                        self.gap_count += 1
                        self.gap_total += g["secs"]
                        self.gap = None
                    has_connected = True
                    state["attempts"] = 0
                    state["reason"] = None
                    last_probe_mono = time.monotonic()
                    last_uptime_min = None
                except Exception as exc:
                    state["next_connect"] = time.monotonic() + self.reconnect_delay
                    attempts = state["attempts"]
                    if self.gap is not None:
                        self.gap["failed"] += 1
                    if self.gap is None or self.gap["failed"] == 1:
                        self.cap.summary(f"Telnet connect attempt #{attempts} failed (timeout {self.effective_timeout(attempts)}s): {exc} Retrying in {self.reconnect_delay:g}s (further failed attempts of this gap are counted, not listed).")
                    self.close_socket()
                    continue
                try:
                    self.after_connect()
                except Exception as exc:
                    drop("read failed", f"Telnet read failed: {exc}; waiting {self.post_disconnect_delay:g}s before reconnect attempts.")
                continue

            try:
                text = self.read_available()
                if text:
                    state["scan"] = (state["scan"] + text)[-512:]
                    if SETUP_FINISHED in state["scan"]:
                        self.setup_seen = True
                    if REBOOT_MARKER.search(state["scan"]):
                        drop("ESP.restart() marker captured",
                             f"Telnet captured ESP.restart(); waiting {self.post_disconnect_delay:g}s before reconnect attempts.")
                        continue
                ready, _, _ = select.select([self.sock], [], [], 0)
                if ready and not self.sock.recv(1, socket.MSG_PEEK):
                    drop("connection closed",
                         f"Telnet disconnected; waiting {self.post_disconnect_delay:g}s before reconnect attempts.")
                    continue
                self.maybe_send_dump()
            except BlockingIOError:
                pass
            except Exception as exc:
                drop("read failed", f"Telnet read failed: {exc}; waiting {self.post_disconnect_delay:g}s before reconnect attempts.")
                continue

            # Silence watchdog: a quiet device is only disconnected when it fails the
            # health probe or its uptime went down (it rebooted).
            if self.idle_check > 0 and self.sock is not None:
                quiet_for = time.monotonic() - max(self.last_byte_mono, last_probe_mono)
                if quiet_for >= self.idle_check:
                    ok, up = self.health_probe()
                    last_probe_mono = time.monotonic()
                    # The device was last known alive at the later of the last telnet
                    # byte and the last successful probe: a watchdog gap starts there.
                    known_alive = max(self.last_byte_wall, last_ok_probe_wall)
                    if not ok:
                        drop("health probe failed",
                             f"Telnet silent for {self.idle_check}s and the health probe failed; reconnecting.",
                             known_alive)
                        continue
                    if up is not None and last_uptime_min is not None and up < last_uptime_min:
                        drop("device rebooted (uptime reset)",
                             f"Telnet silent for {self.idle_check}s and the uptime went down ({last_uptime_min} -> {up} min); reconnecting.",
                             known_alive)
                        continue
                    last_ok_probe_wall = datetime.now(timezone.utc).astimezone()
                    if up is not None:
                        last_uptime_min = up

            self.cap.wait(0.1)


# --------------------------------------------------------------------------- #
# MQTT (paho-mqtt from the vendored wheel)
# --------------------------------------------------------------------------- #

def load_paho():
    """Import paho-mqtt from the vendored wheel (zipimport), else from site-packages."""
    wheel = HERE / "vendor" / PAHO_WHEEL
    if wheel.is_file() and str(wheel) not in sys.path:
        sys.path.insert(0, str(wheel))
    import paho.mqtt.client as mqtt  # noqa: E402
    return mqtt


class MqttCapture:
    """Subscribes like `mosquitto_sub -v`: one 'topic payload' line per message."""

    def __init__(self, cap, host, port, topic, username, password):
        self.cap = cap
        self.host = host
        self.port = port
        self.topic = topic
        self.username = username
        self.password = password
        self.client = None
        self.log = None
        self.err = None
        self.messages = 0
        self.connected_once = False

    def errline(self, line):
        try:
            self.err.write(f"{now_iso()}  {line}\n")
            self.err.flush()
        except Exception:
            pass

    def start(self):
        mqtt = load_paho()
        self.log = open(self.cap.path("mqtt.log"), "a", encoding="utf-8", errors="replace")
        self.err = open(self.cap.path("mqtt.stderr.log"), "a", encoding="utf-8", errors="replace")
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                             client_id=f"otgw-capture-{os.getpid()}", protocol=mqtt.MQTTv311)
        if self.username:
            client.username_pw_set(self.username, self.password or None)

        def on_connect(c, userdata, flags, reason_code, properties):
            if reason_code.is_failure:
                self.errline(f"Connection refused: {reason_code}")
                return
            c.subscribe(self.topic)
            if not self.connected_once:
                self.connected_once = True
                self.cap.summary(f"MQTT connected: {self.host}:{self.port}, subscribed to '{self.topic}'")
            else:
                self.cap.summary(f"MQTT reconnected: {now_iso()}")

        def on_disconnect(c, userdata, flags, reason_code, properties):
            if not self.cap.stopping():
                self.errline(f"Disconnected: {reason_code}; paho reconnects")

        def on_connect_fail(c, userdata):
            self.errline(f"Connect to {self.host}:{self.port} failed; retrying")

        def on_message(c, userdata, msg):
            self.messages += 1
            try:
                self.log.write(f"{msg.topic} {msg.payload.decode('utf-8', 'replace')}\n")
                self.log.flush()
            except Exception:
                pass

        client.on_connect = on_connect
        client.on_disconnect = on_disconnect
        client.on_connect_fail = on_connect_fail
        client.on_message = on_message
        client.reconnect_delay_set(min_delay=1, max_delay=30)
        client.connect_async(self.host, self.port, keepalive=60)
        client.loop_start()
        self.client = client

    def stop(self):
        if self.client is not None:
            try:
                self.client.disconnect()
            except Exception:
                pass
            try:
                self.client.loop_stop()
            except Exception:
                pass
        for handle in (self.log, self.err):
            try:
                if handle:
                    handle.close()
            except Exception:
                pass
        self.cap.summary(f"MQTT capture: {self.messages} message(s) written to mqtt.log.")


# --------------------------------------------------------------------------- #
# Browser devtools over CDP (headless Edge/Chrome/Chromium)
# --------------------------------------------------------------------------- #

def find_browser(explicit):
    if explicit:
        return explicit if os.path.isfile(explicit) else None
    candidates = []
    if os.name == "nt":
        for base in (os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)"), os.environ.get("LOCALAPPDATA")):
            if base:
                candidates += [os.path.join(base, "Microsoft", "Edge", "Application", "msedge.exe"),
                               os.path.join(base, "Google", "Chrome", "Application", "chrome.exe")]
    elif sys.platform == "darwin":
        for base in ("/Applications", os.path.expanduser("~/Applications")):
            candidates += [f"{base}/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
                           f"{base}/Google Chrome.app/Contents/MacOS/Google Chrome",
                           f"{base}/Chromium.app/Contents/MacOS/Chromium"]
    for path in candidates:
        if os.path.isfile(path):
            return path
    import shutil
    for name in ("msedge", "microsoft-edge", "microsoft-edge-stable", "chrome", "google-chrome",
                 "google-chrome-stable", "chromium", "chromium-browser"):
        found = shutil.which(name)
        if found:
            return found
    return None


def port_is_free(port):
    s = socket.socket()
    try:
        s.bind(("127.0.0.1", port))
        return True
    except OSError:
        return False
    finally:
        s.close()


def select_debug_port(preferred):
    for port in range(preferred, min(preferred + 12, 65536)):
        if port_is_free(port):
            return port
    return preferred


def windows_kill_on_close_job(proc):
    """Put the browser in a job that Windows kills when this process ends, also
    when it is killed hard or its console window is closed. Returns the handle,
    which must stay open for the life of the capture."""
    if os.name != "nt":
        return None
    try:
        class IO_COUNTERS(ctypes.Structure):
            _fields_ = [(n, ctypes.c_ulonglong) for n in (
                "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

        class BASIC(ctypes.Structure):
            _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64), ("PerJobUserTimeLimit", ctypes.c_int64),
                        ("LimitFlags", ctypes.c_uint32), ("MinimumWorkingSetSize", ctypes.c_size_t),
                        ("MaximumWorkingSetSize", ctypes.c_size_t), ("ActiveProcessLimit", ctypes.c_uint32),
                        ("Affinity", ctypes.c_size_t), ("PriorityClass", ctypes.c_uint32),
                        ("SchedulingClass", ctypes.c_uint32)]

        class EXTENDED(ctypes.Structure):
            _fields_ = [("BasicLimitInformation", BASIC), ("IoInfo", IO_COUNTERS),
                        ("ProcessMemoryLimit", ctypes.c_size_t), ("JobMemoryLimit", ctypes.c_size_t),
                        ("PeakProcessMemoryUsed", ctypes.c_size_t), ("PeakJobMemoryUsed", ctypes.c_size_t)]

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateJobObjectW.restype = ctypes.c_void_p
        kernel32.OpenProcess.restype = ctypes.c_void_p
        kernel32.SetInformationJobObject.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        kernel32.AssignProcessToJobObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        job = kernel32.CreateJobObjectW(None, None)
        if not job:
            return None
        info = EXTENDED()
        info.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel32.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info)):
            kernel32.CloseHandle(job)
            return None
        handle = kernel32.OpenProcess(0x0001 | 0x0100, False, proc.pid)  # TERMINATE | SET_QUOTA
        if not handle:
            kernel32.CloseHandle(job)
            return None
        ok = kernel32.AssignProcessToJobObject(job, handle)
        kernel32.CloseHandle(handle)
        if not ok:
            kernel32.CloseHandle(job)
            return None
        return job
    except Exception:
        return None


def _linux_die_with_parent():
    """preexec_fn: Linux sends SIGTERM to the browser when the capture dies."""
    try:
        libc = ctypes.CDLL(None, use_errno=True)
        libc.prctl(1, signal.SIGTERM)  # PR_SET_PDEATHSIG
    except Exception:
        pass


class WebSocket:
    """Minimal RFC 6455 client: enough for the local CDP endpoint."""

    def __init__(self, url):
        rest = url.split("://", 1)[1]
        host_port, path = rest.split("/", 1)
        host, port = host_port, 80
        if ":" in host_port:
            host, port_text = host_port.rsplit(":", 1)
            port = int(port_text)
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(1)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        request = (f"GET /{path} HTTP/1.1\r\nHost: {host_port}\r\nUpgrade: websocket\r\n"
                   f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n")
        self.sock.sendall(request.encode("ascii"))
        response = b""
        while b"\r\n\r\n" not in response:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("websocket closed during upgrade")
            response += chunk
        if b" 101 " not in response.split(b"\r\n", 1)[0]:
            raise RuntimeError("websocket upgrade failed")

    def send_json(self, payload):
        data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        mask = os.urandom(4)
        header = bytearray([0x81])
        length = len(data)
        if length < 126:
            header.append(0x80 | length)
        elif length < 65536:
            header.append(0x80 | 126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack("!Q", length))
        header.extend(mask)
        self.sock.sendall(bytes(header) + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def send_pong(self, payload=b""):
        mask = os.urandom(4)
        self.sock.sendall(bytes([0x8A, 0x80 | len(payload)]) + mask +
                          bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def recv_text(self):
        chunks = []
        while True:
            self._in_frame = False
            first = self._read_exact(2)
            self._in_frame = True
            fin, opcode = first[0] & 0x80, first[0] & 0x0F
            masked, length = first[1] & 0x80, first[1] & 0x7F
            if length == 126:
                length = struct.unpack("!H", self._read_exact(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._read_exact(8))[0]
            mask = self._read_exact(4) if masked else b""
            data = self._read_exact(length) if length else b""
            if masked:
                data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            if opcode == 8:
                return None
            if opcode == 9:
                self.send_pong(data[:125])
                continue
            if opcode in (1, 0):
                chunks.append(data)
            if fin:
                return b"".join(chunks).decode("utf-8", "replace")

    def _read_exact(self, count):
        data = b""
        started = time.monotonic()
        while len(data) < count:
            try:
                chunk = self.sock.recv(count - len(data))
            except socket.timeout:
                # A timeout between frames is the caller's idle tick. Inside a frame it must not
                # lose the bytes already read (TASK-1503), so keep reading for up to 10 s.
                if (data or getattr(self, "_in_frame", False)) and time.monotonic() - started < 10:
                    continue
                raise
            if not chunk:
                raise ConnectionError("websocket closed")
            data += chunk
        return data

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


class BrowserCapture(threading.Thread):
    def __init__(self, cap, browser_path, device_url, debug_port, profile_dir):
        super().__init__(name="browser", daemon=True)
        self.cap = cap
        self.browser_path = browser_path
        self.device_url = device_url
        self.debug_port = debug_port
        self.profile_dir = profile_dir
        self.proc = None
        self.job = None
        self.result = "Browser capture: completed normally."

    def log(self, line):
        with open(self.cap.path("browser.log"), "a", encoding="utf-8") as f:
            f.write(f"{clock()}  {line}\n")

    def run(self):
        ws = None
        stderr_handle = None
        requests = {}
        cdp_id = [0]

        def send(method, params=None):
            cdp_id[0] += 1
            payload = {"id": cdp_id[0], "method": method}
            if params:
                payload["params"] = params
            ws.send_json(payload)

        try:
            self.log(f"browser executable: {self.browser_path}")
            self.log(f"device url: {self.device_url}   cdp port: {self.debug_port}")
            args = [self.browser_path, "--headless=new", "--disable-gpu", "--no-first-run",
                    "--no-default-browser-check", "--disable-extensions", "--disable-background-networking",
                    "--mute-audio",
                    # Quiet Chromium's own stderr sink (task-manager fallback, USB
                    # device_event_log, SmartScreen DNS timeout); browser.log is unaffected.
                    "--disable-logging", "--log-level=3", "--disable-features=msSmartScreenProtection",
                    "--remote-allow-origins=*", f"--remote-debugging-port={self.debug_port}",
                    f"--user-data-dir={self.profile_dir}", "about:blank"]
            stderr_handle = open(self.cap.path("browser.stderr.log"), "ab")
            popen_kwargs = {"stdout": subprocess.DEVNULL, "stderr": stderr_handle, "stdin": subprocess.DEVNULL}
            if sys.platform.startswith("linux"):
                popen_kwargs["preexec_fn"] = _linux_die_with_parent
            if os.name == "nt":
                popen_kwargs["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            self.proc = subprocess.Popen(args, **popen_kwargs)
            self.job = windows_kill_on_close_job(self.proc)

            ws_url = None
            deadline = time.monotonic() + 15
            while not self.cap.stopping() and time.monotonic() < deadline:
                try:
                    with urllib.request.urlopen(f"http://127.0.0.1:{self.debug_port}/json", timeout=1) as response:
                        targets = json.loads(response.read().decode("utf-8"))
                    for target in targets:
                        if target.get("type") == "page" and target.get("webSocketDebuggerUrl"):
                            ws_url = target["webSocketDebuggerUrl"]
                            break
                    if ws_url:
                        break
                except Exception:
                    self.cap.wait(0.3)
                self.cap.wait(0.2)
            if not ws_url:
                if self.cap.stopping():
                    self.result = "Browser capture: stopped before CDP page target was ready."
                    self.log("stop requested before cdp page target was ready")
                    return
                raise RuntimeError(f"CDP page target not found on port {self.debug_port} within 15s")

            self.log(f"cdp websocket: {ws_url}")
            ws = WebSocket(ws_url)
            self.log("cdp connected")
            for method in ("Network.enable", "Runtime.enable", "Log.enable", "Page.enable"):
                send(method)
            send("Page.navigate", {"url": self.device_url})
            self.log(f"domains enabled; navigating to {self.device_url}")

            while not self.cap.stopping():
                try:
                    text = ws.recv_text()
                except socket.timeout:
                    continue
                if text is None:
                    break
                try:
                    evt = json.loads(text)
                except Exception:
                    continue
                method = evt.get("method")
                params = evt.get("params", {})
                if method == "Runtime.consoleAPICalled":
                    parts = []
                    for arg in params.get("args", []):
                        if "value" in arg:
                            parts.append(str(arg["value"]))
                        elif arg.get("description"):
                            parts.append(str(arg["description"]))
                        elif arg.get("unserializableValue"):
                            parts.append(str(arg["unserializableValue"]))
                        else:
                            parts.append(f"[{arg.get('type', 'unknown')}]")
                    self.log(f"[console.{params.get('type', 'log')}] " + " ".join(parts))
                elif method == "Runtime.exceptionThrown":
                    details = params.get("exceptionDetails", {})
                    exc = details.get("exception", {})
                    self.log("[exception] " + str(exc.get("description") or details.get("text") or "unknown exception"))
                elif method == "Log.entryAdded":
                    entry = params.get("entry", {})
                    where = f" ({entry.get('url')})" if entry.get("url") else ""
                    self.log(f"[log.{entry.get('level')}] {entry.get('text')}{where}")
                elif method == "Network.requestWillBeSent":
                    rid = params.get("requestId")
                    if rid:
                        requests[rid] = {"url": params.get("request", {}).get("url", ""),
                                         "start": float(params.get("timestamp", 0)), "status": ""}
                elif method == "Network.responseReceived":
                    rid = params.get("requestId")
                    if rid in requests:
                        requests[rid]["status"] = str(params.get("response", {}).get("status", ""))
                elif method == "Network.loadingFinished":
                    record = requests.pop(params.get("requestId"), None)
                    if record:
                        ms = int((float(params.get("timestamp", 0)) - record["start"]) * 1000)
                        self.log(f"[net] {ms:6d} ms  {record['status']:>3}  {record['url']}")
                elif method == "Network.loadingFailed":
                    record = requests.pop(params.get("requestId"), None)
                    if record:
                        self.log(f"[net] FAILED      {params.get('errorText')}  {record['url']}")

            for record in requests.values():
                status = record["status"] or "no-response"
                self.log(f"[net] PENDING     {status:>3}  {record['url']}  (started, never finished)")
            self.log("stop requested; closing capture")
        except Exception as exc:
            self.result = f"Browser capture: error - {exc}"
            try:
                self.log(f"[worker-error] {exc}")
            except Exception:
                pass
        finally:
            if ws:
                ws.close()
            self.stop_browser()
            if stderr_handle:
                try:
                    stderr_handle.close()
                except Exception:
                    pass
            self.cap.summary(self.result)

    def stop_browser(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
                self.proc.wait(timeout=3)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass


# --------------------------------------------------------------------------- #
# Crash log + reboot_log.txt polling
# --------------------------------------------------------------------------- #

class CrashlogCapture(threading.Thread):
    def __init__(self, cap, crashlog_url, reboot_log_url, poll_seconds):
        super().__init__(name="crashlog", daemon=True)
        self.cap = cap
        self.crashlog_url = crashlog_url
        self.reboot_log_url = reboot_log_url
        self.poll_seconds = poll_seconds

    def log(self, line):
        with open(self.cap.path("crashlog.log"), "a", encoding="utf-8") as f:
            f.write(f"{clock()}  {line}\n")

    def get(self, url):
        result = None
        for attempt in range(2):
            status, body, err, _ctype = http_get(url, 10)
            result = (status, body.decode("utf-8", "replace"), err)
            if status is not None:
                return result
            if attempt == 0 and not self.cap.stopping():
                self.cap.wait(1.5)
        return result

    def run(self):
        result = "Crash-log capture: completed normally."
        last_crash = None
        last_reboot = None
        polls = 0
        try:
            self.log(f"crashlog endpoint: {self.crashlog_url}")
            self.log(f"reboot-log file:   {self.reboot_log_url}")
            self.log(f"poll interval:     {self.poll_seconds}s")
            while not self.cap.stopping():
                polls += 1
                status, body, error = self.get(self.crashlog_url)
                if status == 200 and body:
                    trimmed = body.strip()
                    if trimmed != last_crash:
                        self.log(f"[crashlog #{polls} CHANGED] {trimmed}")
                        last_crash = trimmed
                elif status is not None:
                    marker = f"status-{status}"
                    if marker != last_crash:
                        self.log(f"[crashlog #{polls}] HTTP {status} (endpoint unavailable on this firmware?)")
                        last_crash = marker
                else:
                    self.log(f"[crashlog #{polls}] request failed after retry: {error}")

                status, body, error = self.get(self.reboot_log_url)
                if status == 200 and body:
                    trimmed = body.strip()
                    if trimmed and trimmed != last_reboot:
                        self.log(f"[reboot_log.txt #{polls} CHANGED]")
                        for line in trimmed.splitlines():
                            if line.strip():
                                self.log(f"  | {line}")
                        last_reboot = trimmed
                elif status is None:
                    self.log(f"[reboot_log.txt #{polls}] request failed after retry: {error}")
                self.cap.wait(self.poll_seconds)
            self.log(f"stop requested; closing crash-log capture ({polls} polls)")
        except Exception as exc:
            result = f"Crash-log capture: error - {exc}"
            try:
                self.log(f"[worker-error] {exc}")
            except Exception:
                pass
        finally:
            self.cap.summary(result)


# --------------------------------------------------------------------------- #
# Raw OpenTherm frames (TASK-1503)
# --------------------------------------------------------------------------- #

def parse_ot_log_line(text):
    """One device OT log line -> (device_time, source letter, 8 hex digits), or None
    for event lines (> < ! *), keepalives and anything else."""
    match = OT_LOG_LINE.match(text.strip("\r\n"))
    return match.groups() if match else None


class FrameLogWriter:
    """Writes otframes.log (timed) and otgw_simulation.log (bare frames, the format the
    firmware replay reads). Both are LF-terminated on every platform."""

    def __init__(self, frames_path, sim_path):
        self.count = 0
        self._lock = threading.Lock()
        self._frames = open(frames_path, "w", encoding="utf-8", newline="\n", buffering=1)
        self._sim = open(sim_path, "w", encoding="ascii", newline="\n", buffering=1)
        self._frames.write(FRAMES_HEADER + "\n")

    def write(self, host_time, device_time, letter, frame):
        host = host_time.astimezone().isoformat(timespec="milliseconds")
        with self._lock:
            self._frames.write(f"{host}\t{device_time}\t{letter}\t{frame}\n")
            self._sim.write(f"{letter}{frame}\n")
            self.count += 1

    def comment(self, text):
        with self._lock:
            self._frames.write(f"# {now_iso()} {text}\n")

    def close(self):
        with self._lock:
            for f in (self._frames, self._sim):
                try:
                    f.close()
                except OSError:
                    pass


class FrameCapture(threading.Thread):
    """Reads the device's /ws (the web UI's OT log stream) and records every frame.
    One extra WebSocket client; the device allows 3. Reconnects with a growing delay."""

    def __init__(self, cap, host, port=80):
        super().__init__(name="otframes", daemon=True)
        self.cap = cap
        self.host = host
        self.port = port
        self.writer = None

    def run(self):
        result = "Frame recorder: completed normally."
        try:
            self.writer = FrameLogWriter(self.cap.path(FRAMES_LOG), self.cap.path(SIM_LOG))
        except OSError as exc:
            self.cap.script_error(f"otframes: cannot create {FRAMES_LOG}/{SIM_LOG}: {exc}")
            self.cap.summary(f"Frame recorder: error - {exc}")
            return
        delay = WS_RECONNECT_MIN
        last_error = None
        try:
            while not self.cap.stopping():
                ws = None
                try:
                    ws = WebSocket(f"ws://{self.host}:{self.port}/ws")
                    ws.sock.settimeout(1)
                    self.writer.comment("connected to /ws")
                    last_error = None
                    delay = WS_RECONNECT_MIN
                    self._read(ws)
                    self.writer.comment("disconnected from /ws")
                except Exception as exc:
                    text = f"{type(exc).__name__}: {exc}"
                    if text != last_error:   # one line per distinct failure, not one per retry
                        self.cap.script_error(f"otframes: /ws on {self.host}:{self.port} - {text}")
                        last_error = text
                finally:
                    if ws is not None:
                        ws.close()
                self.cap.wait(delay)
                delay = min(delay * 2, WS_RECONNECT_MAX)
        except Exception as exc:
            result = f"Frame recorder: error - {exc}"
            self.cap.script_error(f"otframes: worker error {type(exc).__name__}: {exc}")
        finally:
            count = self.writer.count
            self.writer.close()
            self.cap.summary(f"{result} {count} frame(s) -> {FRAMES_LOG}, {SIM_LOG}")

    def _read(self, ws):
        while not self.cap.stopping():
            try:
                text = ws.recv_text()
            except socket.timeout:
                continue
            if text is None:
                return
            received = datetime.now().astimezone()
            for line in text.splitlines():
                parsed = parse_ot_log_line(line)
                if parsed:
                    self.writer.write(received, *parsed)


# --------------------------------------------------------------------------- #
# End of run: HTTP probes, metadata, merged transcript
# --------------------------------------------------------------------------- #

def run_http_probes(cap, http_base, timeout):
    def collapse(text):
        return " ".join(text.split())

    with open(cap.path("curl-probes.log"), "w", encoding="utf-8") as log:
        log.write("HTTP probes via Python urllib\n")
        log.write(f"Target host : {http_base}\n")
        log.write(f"Per-request timeout: {timeout}s\n")
        log.write(f"Started: {now_iso()}\n")
        log.write("Columns: <time>  <result>  <http_code> <time_total> <size> <content_type>  <url>\n\n")
        for path in PROBE_PATHS:
            url = http_base + path
            started = time.monotonic()
            status, body, err, ctype = http_get(url, timeout)
            elapsed = time.monotonic() - started
            if status is None:
                label = "TIMEOUT " if "timed out" in (err or "").lower() else "ERROR   "
                log.write(f"{clock()}  {label} {err} after {elapsed:.3f}s  {url}\n")
                continue
            label = "OK      " if 200 <= status < 400 else "HTTP    "
            log.write(f"{clock()}  {label} {status} {elapsed:.3f}s {len(body)}B {ctype}  {url}\n")
            if path in PROBE_FULL_BODY:
                snippet = collapse(body[:PROBE_FULL_BODY_CAP].decode("utf-8", "replace"))
                cut = len(body) > PROBE_FULL_BODY_CAP
            else:
                snippet = collapse(body[:PROBE_SNIPPET * 2].decode("utf-8", "replace"))
                cut = len(snippet) > PROBE_SNIPPET
                snippet = snippet[:PROBE_SNIPPET]
            if snippet:
                log.write(f"              body: {snippet}{'...' if cut else ''}\n")
        log.write(f"\nFinished: {now_iso()}\n")


def resolve_metadata(cap, device_host, http_base, run_name):
    metadata = {"hostname": "", "firmware_version": "", "unique_id": "", "pic_device_id": ""}
    sources = []

    def usable(value):
        text = "" if value is None else str(value).strip()
        return text.lower() not in {"", "null", "unknown", "(not set)"}

    def add(name, value, source, prefer_existing=False):
        if not usable(value) or (prefer_existing and usable(metadata[name])):
            return
        metadata[name] = str(value).strip()
        sources.append(f"{name}={source}")

    def prop(value, name):
        return value.get(name) if isinstance(value, dict) else None

    def setting(settings, name):
        entry = prop(settings, name)
        return entry["value"] if isinstance(entry, dict) and "value" in entry else entry

    def fetch_json(path):
        status, body, err, _ctype = http_get(http_base + path, 2)
        if status != 200:
            raise RuntimeError(err or f"HTTP {status}")
        return json.loads(body.decode("utf-8"))

    patterns = {
        "hostname": re.compile(r"^\s*hostname:\s*(.+?)\s*$", re.IGNORECASE),
        "firmware_version": re.compile(r"^\s*version:\s*(.+?)\s*$", re.IGNORECASE),
        "unique_id": re.compile(r"^\s*unique_id:\s*(.+?)\s*$", re.IGNORECASE),
        "pic_device_id": re.compile(r"^\s*device_id:\s*(.+?)\s*$", re.IGNORECASE),
    }
    try:
        with open(cap.path("telnet.log"), "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                for name, pattern in patterns.items():
                    match = pattern.match(line)
                    if match:
                        add(name, match.group(1), "telnet.log", prefer_existing=name in {"hostname", "firmware_version"})
    except OSError:
        pass

    needs_api = not all(usable(metadata[n]) for n in ("hostname", "firmware_version", "unique_id"))
    warnings = 0
    if needs_api:
        try:
            debug = prop(fetch_json("/api/v2/debug"), "debug")
            add("hostname", prop(debug, "settings.hostname"), "/api/v2/debug", prefer_existing=True)
            add("firmware_version", prop(debug, "build.version"), "/api/v2/debug", prefer_existing=True)
            add("unique_id", prop(debug, "settings.mqtt.unique_id"), "/api/v2/debug")
        except Exception as exc:
            warnings += 1
            cap.script_error(f"Metadata lookup /api/v2/debug failed: {exc}")
    if needs_api and not usable(metadata["unique_id"]):
        try:
            settings = prop(fetch_json("/api/v2/settings"), "settings")
            add("hostname", setting(settings, "hostname"), "/api/v2/settings", prefer_existing=True)
            add("unique_id", setting(settings, "mqttuniqueid"), "/api/v2/settings")
        except Exception as exc:
            warnings += 1
            cap.script_error(f"Metadata lookup /api/v2/settings failed: {exc}")
    if needs_api:
        try:
            device = prop(fetch_json("/api/v2/device/info"), "device")
            add("hostname", prop(device, "hostname"), "/api/v2/device/info", prefer_existing=True)
            add("firmware_version", prop(device, "fwversion"), "/api/v2/device/info", prefer_existing=True)
            if not usable(metadata["unique_id"]):
                mac = re.sub(r"[^0-9A-Fa-f]", "", str(prop(device, "macaddress") or ""))
                if mac:
                    add("unique_id", f"otgw-{mac.upper()}", "/api/v2/device/info macaddress")
        except Exception as exc:
            warnings += 1
            cap.script_error(f"Metadata lookup /api/v2/device/info failed: {exc}")

    def safe_part(value, fallback, max_length=64):
        part = str(value).strip() if usable(value) else fallback
        part = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "-", part)
        part = re.sub(r"\s+", "-", part)
        part = re.sub(r"-+", "-", part).strip(".-") or fallback
        return part[:max_length].strip(".-") or fallback

    hostname = metadata["hostname"] if usable(metadata["hostname"]) else device_host
    version = metadata["firmware_version"] if usable(metadata["firmware_version"]) else "unknown-version"
    device_id = metadata["unique_id"] if usable(metadata["unique_id"]) else metadata["pic_device_id"]
    if not usable(device_id):
        device_id = "unknown-id"
    filename = "transcript-{}-{}-{}-{}.txt".format(
        run_name, safe_part(version, "unknown-version"), safe_part(hostname, device_host), safe_part(device_id, "unknown-id"))
    cap.summary(f"ResolvedHostname: {hostname}", echo=False)
    cap.summary(f"ResolvedFirmwareVersion: {version}", echo=False)
    cap.summary(f"ResolvedUniqueId: {metadata['unique_id'] if usable(metadata['unique_id']) else '(unknown)'}", echo=False)
    cap.summary(f"ResolvedPicDeviceId: {metadata['pic_device_id'] if usable(metadata['pic_device_id']) else '(unknown)'}", echo=False)
    cap.summary(f"ResolvedDeviceIdForFilename: {device_id}", echo=False)
    cap.summary(f"Metadata sources: {', '.join(sources) if sources else '(none)'}", echo=False)
    if warnings:
        cap.summary(f"Metadata lookup warnings: {warnings} (see error.txt)", echo=False)
    return filename


def read_redacted(path):
    try:
        with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
            text = f.read()
    except FileNotFoundError:
        return None
    except OSError as exc:
        return f"(could not read: {exc})\n"
    text = "".join(redact(line) for line in text.splitlines(keepends=True))
    if text and not text.endswith("\n"):
        text += "\n"
    return text


def write_sections(out, cap, sections, empty_marker=None):
    for title, name in sections:
        out.write(f"{SECTION_RULE}\n=== {title}\n{SECTION_RULE}\n")
        text = read_redacted(cap.path(name))
        if text is None:
            out.write("(not present)\n")
        elif not text.strip() and empty_marker:
            out.write(f"{empty_marker}\n")
        else:
            out.write(text)
        out.write("\n")


def build_tool_error_log(cap):
    with open(cap.path("error.txt"), "w", encoding="utf-8") as out:
        out.write("OTGW capture - tool stderr and script errors\n")
        out.write(f"Generated: {now_iso()}\n")
        out.write(f"Run folder: {cap.run_path}\n\n")
        write_sections(out, cap, ERROR_SECTIONS, empty_marker="(empty)")


def merge_transcript(cap, transcript):
    tmp = transcript.with_name(transcript.name + ".tmp")
    with open(tmp, "w", encoding="utf-8") as out:
        out.write("OTGW capture - merged transcript\n")
        out.write(f"Generated: {now_iso()}\n")
        out.write(f"Run folder: {cap.run_path}\n")
        out.write("Upload THIS single file; it contains every capture log below.\n\n")
        write_sections(out, cap, TRANSCRIPT_SECTIONS)
    os.replace(tmp, transcript)


# --------------------------------------------------------------------------- #
# Settings, prompts, command line
# --------------------------------------------------------------------------- #

def settings_path():
    override = os.environ.get("OTGW_CAPTURE_SETTINGS")
    if override:
        return Path(override)
    base = os.environ.get("LOCALAPPDATA") or (os.environ.get("TEMP") if os.name == "nt" else None)
    if base:
        return Path(base) / "OTGW-capture" / "capture-settings.json"
    return Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "otgw-capture" / "capture-settings.json"


def load_settings():
    try:
        with open(settings_path(), encoding="utf-8-sig") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def save_settings(args, persist_password):
    """Remember the last-used values to pre-fill the next run's prompts. The MQTT
    password is written only with -SaveSecrets and a non-empty password; a stored
    one is otherwise kept as it was (TASK-833, TASK-879). A run without a broker
    leaves the saved MQTT values alone, so the next run still pre-fills them
    (TASK-1453)."""
    path = settings_path()
    try:
        previous = load_settings()
        data = dict(previous)  # keep keys other tools added (WifiSsid, Hardware, ...)
        data["DeviceHost"] = args.device
        if args.broker:
            data.update({"BrokerHost": args.broker, "BrokerPort": args.broker_port,
                         "Topic": args.topic, "Username": args.username or ""})
        if persist_password and args.password:
            data["MqttPassword"] = args.password
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        if os.name != "nt":
            os.chmod(path, 0o600)
        return path
    except Exception:
        return None


def interactive():
    try:
        return sys.stdin is not None and sys.stdin.isatty()
    except Exception:
        return False


def ask(prompt, default=""):
    shown = f"{prompt} [{default}]: " if default else f"{prompt}: "
    try:
        answer = input(shown).strip()
    except EOFError:
        answer = ""
    return answer or default


ALIASES = {
    "-DeviceHost": "--device", "-BrokerHost": "--broker", "-BrokerPort": "--broker-port",
    "-Topic": "--topic", "-Username": "--username", "-Password": "--password",
    "-SaveSecrets": "--save-secrets", "-OutputRoot": "--output-root", "-DurationSeconds": "--duration",
    "-TelnetConnectTimeoutSeconds": "--telnet-connect-timeout-seconds",
    "-TelnetReconnectDelayMilliseconds": "--telnet-reconnect-delay-ms",
    "-TelnetPostDisconnectDelayMilliseconds": "--telnet-post-disconnect-delay-ms",
    "-TelnetIdleCheckSeconds": "--telnet-idle-check-seconds",
    "-SkipBrowserCapture": "--skip-browser-capture", "-BrowserUrl": "--browser-url",
    "-BrowserDebugPort": "--browser-debug-port", "-BrowserPath": "--browser-path",
    "-SkipHttpProbes": "--skip-http-probes", "-HttpProbeTimeoutSeconds": "--http-probe-timeout-seconds",
    "-SkipCrashlogCapture": "--skip-crashlog-capture", "-CrashlogUrl": "--crashlog-url",
    "-CrashlogPollSeconds": "--crashlog-poll-seconds", "-NoPrompt": "--no-prompt",
    "-Help": "--help", "-?": "--help", "/?": "--help",
    "--user": "--username", "--duration-seconds": "--duration",
    "--telnet-reconnect-delay-milliseconds": "--telnet-reconnect-delay-ms",
    "--telnet-post-disconnect-delay-milliseconds": "--telnet-post-disconnect-delay-ms",
    # Accepted and ignored: there is no mosquitto_sub any more (ADR-202).
    "-MosquittoSubPath": "--mosquitto-sub-path", "-SkipToolInstall": "--skip-tool-install",
}


def normalize_argv(argv):
    lowered = {k.lower(): v for k, v in ALIASES.items()}
    out = []
    for arg in argv:
        name, eq, value = arg.partition("=")
        mapped = lowered.get(name.lower())
        if mapped and (not eq or name.startswith("--")):
            out.append(mapped + (eq + value if eq else ""))
        else:
            out.append(arg)
    return out


def ranged(lo, hi, allow_zero=False):
    def check(text):
        value = int(text)
        if (allow_zero and value == 0) or lo <= value <= hi:
            return value
        raise argparse.ArgumentTypeError(f"must be {'0 or ' if allow_zero else ''}{lo}..{hi}")
    return check


def build_parser():
    p = argparse.ArgumentParser(
        prog="capture-mqtt-debug",
        description="OTGW diagnostic capture: telnet debug, MQTT, browser devtools, crash log and HTTP "
                    "probes, merged into ONE transcript file to upload. Press Q or Ctrl+C to stop.",
        epilog="PowerShell-style names work too: -DeviceHost, -BrokerHost, -DurationSeconds, -SaveSecrets, ...",
        allow_abbrev=False)
    p.add_argument("--device", metavar="HOST", help="OTGW device host or IP (prompted when missing)")
    p.add_argument("--broker", metavar="HOST", help="MQTT broker host; empty = skip the MQTT stream")
    p.add_argument("--broker-port", type=ranged(1, 65535), default=1883)
    p.add_argument("--topic", default="#", help="MQTT topic filter (default '#')")
    p.add_argument("--username", help="MQTT username (blank = anonymous)")
    p.add_argument("--password", help="MQTT password (prompted when a username is given)")
    p.add_argument("--save-secrets", action="store_true",
                   help="also store the MQTT password in the local settings file, for unattended runs")
    p.add_argument("--output-root", default="logs/mqtt-diagnostics")
    p.add_argument("--duration", type=ranged(1, 2147483647), help="stop after N seconds (default: until Q/Ctrl+C)")
    p.add_argument("--no-prompt", action="store_true", help="never prompt; fail when the device host is missing")
    p.add_argument("--telnet-connect-timeout-seconds", type=ranged(2, 60), default=6)
    p.add_argument("--telnet-reconnect-delay-ms", type=ranged(250, 60000), default=500)
    p.add_argument("--telnet-post-disconnect-delay-ms", type=ranged(250, 60000), default=1000)
    p.add_argument("--telnet-idle-check-seconds", type=ranged(1, 3600, allow_zero=True), default=60,
                   help="health-probe a silent telnet link after N s (0 = off)")
    p.add_argument("--skip-browser-capture", action="store_true")
    p.add_argument("--browser-url")
    p.add_argument("--browser-debug-port", type=ranged(1, 65535), default=9222)
    p.add_argument("--browser-path")
    p.add_argument("--skip-http-probes", action="store_true")
    p.add_argument("--http-probe-timeout-seconds", type=ranged(1, 60), default=10)
    p.add_argument("--skip-frame-capture", action="store_true",
                   help="do not record otframes.log / otgw_simulation.log (raw OpenTherm frames)")
    p.add_argument("--skip-crashlog-capture", action="store_true")
    p.add_argument("--crashlog-url")
    p.add_argument("--crashlog-poll-seconds", type=ranged(5, 3600), default=30)
    # Test and bench hooks; not needed for a normal capture.
    p.add_argument("--telnet-port", type=ranged(1, 65535), default=23, help=argparse.SUPPRESS)
    p.add_argument("--http-port", type=ranged(1, 65535), default=80, help=argparse.SUPPRESS)
    p.add_argument("--idle-check-floor", type=int, default=15, help=argparse.SUPPRESS)
    p.add_argument("--mosquitto-sub-path", help=argparse.SUPPRESS)
    p.add_argument("--skip-tool-install", action="store_true", help=argparse.SUPPRESS)
    return p


def resolve_inputs(args, parser):
    saved = load_settings()
    can_prompt = interactive() and not args.no_prompt
    broker_given = args.broker is not None
    if not args.device and can_prompt:
        args.device = ask("OTGW device host", str(saved.get("DeviceHost") or ""))
    if not args.device:
        parser.error("Device host is required (--device / -DeviceHost).")
    if not broker_given and can_prompt:
        args.broker = ask("MQTT broker host (Enter = skip MQTT)", str(saved.get("BrokerHost") or ""))
    args.broker = (args.broker or "").strip()
    if args.broker and args.username is None and can_prompt and not broker_given:
        args.username = ask("MQTT username (blank for anonymous)", str(saved.get("Username") or ""))
    if args.broker and args.username and not args.password:
        stored = saved.get("MqttPassword")
        if stored and args.username == saved.get("Username"):
            args.password = str(stored)
        elif can_prompt:
            import getpass
            args.password = getpass.getpass(f"MQTT password for {args.username}: ")
    if args.password and not args.username:
        parser.error("Username is required when a password is supplied.")


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #

class KeyReader:
    """Non-blocking single-key reader for the Q stop key; inert without a console."""

    def __init__(self):
        self.enabled = interactive()
        self.saved = None
        if self.enabled and os.name != "nt":
            try:
                import termios
                import tty
                self.saved = termios.tcgetattr(sys.stdin.fileno())
                tty.setcbreak(sys.stdin.fileno())
            except Exception:
                self.enabled = False

    def key(self):
        if not self.enabled:
            return None
        try:
            if os.name == "nt":
                import msvcrt
                return msvcrt.getwch() if msvcrt.kbhit() else None
            ready, _, _ = select.select([sys.stdin], [], [], 0)
            return sys.stdin.read(1) if ready else None
        except Exception:
            return None

    def close(self):
        if self.saved is not None:
            try:
                import termios
                termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self.saved)
            except Exception:
                pass


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(normalize_argv(sys.argv[1:] if argv is None else argv))
    if 0 < args.telnet_idle_check_seconds < args.idle_check_floor:
        parser.error(f"--telnet-idle-check-seconds must be 0 or >= {args.idle_check_floor}")
    resolve_inputs(args, parser)
    saved_path = save_settings(args, args.save_secrets)

    http_base = f"http://{args.device}" + ("" if args.http_port == 80 else f":{args.http_port}")
    run_name = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_path = Path(args.output_root).resolve() / run_name
    run_path.mkdir(parents=True, exist_ok=True)
    cap = Capture(run_path)
    for name in ("summary.txt", "telnet.log", "mqtt.log", "mqtt.stderr.log", "browser.log",
                 "browser.stderr.log", "crashlog.log", "script.error.log"):
        cap.path(name).touch()

    def on_signal(reason):
        def handler(signum, frame):
            cap.request_stop(reason)
        return handler

    signal.signal(signal.SIGINT, on_signal("Ctrl+C/Ctrl+Break"))
    for sig_name, reason in (("SIGTERM", "terminated (SIGTERM)"), ("SIGHUP", "terminal closed (SIGHUP)"),
                             ("SIGBREAK", "Ctrl+C/Ctrl+Break")):
        if hasattr(signal, sig_name):
            try:
                signal.signal(getattr(signal, sig_name), on_signal(reason))
            except Exception:
                pass

    s = cap.summary
    s("OTGW MQTT diagnostic capture", echo=False)
    s(f"Run folder: {run_path}", echo=False)
    s(f"Started: {now_iso()}", echo=False)
    s(f"DeviceHost: {args.device}", echo=False)
    s(f"TelnetPort: {args.telnet_port}", echo=False)
    s(f"TelnetConnectTimeoutSeconds: {args.telnet_connect_timeout_seconds}", echo=False)
    s(f"TelnetPostDisconnectDelayMilliseconds: {args.telnet_post_disconnect_delay_ms}", echo=False)
    s(f"TelnetReconnectDelayMilliseconds: {args.telnet_reconnect_delay_ms}", echo=False)
    s(f"TelnetIdleCheckSeconds: {args.telnet_idle_check_seconds}", echo=False)
    s(f"BrokerHost: {args.broker or '(none)'}", echo=False)
    s(f"BrokerPort: {args.broker_port}", echo=False)
    s(f"Topic: {args.topic}", echo=False)
    s(f"Username supplied: {bool(args.username)}", echo=False)
    s(f"DurationSeconds: {args.duration or 'until manual stop'}", echo=False)
    s(f"SkipBrowserCapture: {args.skip_browser_capture}", echo=False)
    s(f"SkipCrashlogCapture: {args.skip_crashlog_capture}", echo=False)
    s(f"SkipFrameCapture: {args.skip_frame_capture}", echo=False)
    s(f"SkipHttpProbes: {args.skip_http_probes}", echo=False)
    s(f"HttpProbeTimeoutSeconds: {args.http_probe_timeout_seconds}", echo=False)
    s(f"CrashlogPollSeconds: {args.crashlog_poll_seconds}", echo=False)
    s(f"Tool stderr log: {cap.path('error.txt')}", echo=False)
    if saved_path:
        s(f"Settings prefill saved: {saved_path}", echo=False)
    s("Telnet timeout strategy: adaptive (base + retry backoff, capped at 20s)", echo=False)
    s(f"Telnet idle check: {str(args.telnet_idle_check_seconds) + 's (health probe gated)' if args.telnet_idle_check_seconds else 'disabled'}", echo=False)
    s(f"Python: {sys.version.split()[0]} ({sys.executable})", echo=False)
    s(f"Platform: {sys.platform}", echo=False)

    workers = []
    browser = None
    if args.skip_browser_capture:
        s("Browser capture: disabled (--skip-browser-capture).")
    else:
        browser_path = find_browser(args.browser_path)
        if browser_path:
            port = select_debug_port(args.browser_debug_port)
            profile = Path(os.environ.get("TEMP") or os.environ.get("TMPDIR") or "/tmp") / f"otgw-browser-{run_name}-{os.getpid()}"
            profile.mkdir(parents=True, exist_ok=True)
            url = args.browser_url or f"{http_base}/"
            s(f"Browser capture: {browser_path}", echo=False)
            s(f"Browser url: {url}", echo=False)
            s(f"Browser CDP port: {port}", echo=False)
            browser = BrowserCapture(cap, browser_path, url, port, profile)
            browser.start()
            workers.append((browser, 12))
            s("Browser capture started (headless, writing browser.log; stderr in browser.stderr.log).")
        else:
            s("Browser capture: skipped - no Edge/Chrome/Chromium found (pass --browser-path or --skip-browser-capture).")

    if args.skip_crashlog_capture:
        s("Crash-log capture: disabled (--skip-crashlog-capture).")
    else:
        crash_url = args.crashlog_url or f"{http_base}/api/v2/device/crashlog"
        s(f"Crash-log endpoint: {crash_url}", echo=False)
        s(f"Crash-log reboot-log file: {http_base}/reboot_log.txt", echo=False)
        s(f"Crash-log poll interval: {args.crashlog_poll_seconds}s", echo=False)
        crash = CrashlogCapture(cap, crash_url, f"{http_base}/reboot_log.txt", args.crashlog_poll_seconds)
        crash.start()
        workers.append((crash, 25))
        s("Crash-log capture started (polling, writing crashlog.log).")

    if args.skip_frame_capture:
        s("Frame recorder: disabled (--skip-frame-capture).")
    else:
        frames = FrameCapture(cap, args.device, args.http_port)
        frames.start()
        workers.append((frames, 10))
        s(f"Frame recorder started: ws://{args.device}:{args.http_port}/ws -> {FRAMES_LOG}, {SIM_LOG} (kept next to the transcript).")

    mqtt_capture = None
    if not args.broker:
        s("MQTT capture skipped: no BrokerHost given (telnet, probe and crash log still run).")
    else:
        try:
            mqtt_capture = MqttCapture(cap, args.broker, args.broker_port, args.topic, args.username, args.password)
            mqtt_capture.start()
            s(f"MQTT capture started: paho-mqtt, broker {args.broker}:{args.broker_port}, topic '{args.topic}' (writing mqtt.log).")
        except Exception as exc:
            mqtt_capture = None
            s(f"MQTT capture disabled: {type(exc).__name__}: {exc}")
            cap.script_error(f"MQTT capture disabled: {type(exc).__name__}: {exc}")

    telnet = TelnetCapture(cap, args.device, args.telnet_port, http_base, args.telnet_connect_timeout_seconds,
                           args.telnet_reconnect_delay_ms / 1000.0, args.telnet_post_disconnect_delay_ms / 1000.0,
                           args.telnet_idle_check_seconds)
    telnet.start()
    s(f"Capture started: {now_iso()}", echo=False)
    print(f"Capturing into {run_path}")
    print("Press Q to stop cleanly (Ctrl+C also stops).", flush=True)

    keys = KeyReader()
    started = time.monotonic()
    try:
        while not cap.stopping():
            if args.duration and time.monotonic() - started >= args.duration:
                cap.request_stop("duration elapsed")
                break
            key = keys.key()
            if key in ("q", "Q"):
                cap.request_stop("Q key")
                break
            cap.stop_event.wait(0.2)
    except KeyboardInterrupt:
        cap.request_stop("Ctrl+C/Ctrl+Break")
    finally:
        keys.close()
        cap.request_stop("capture shutdown")

    print()
    s(f"Capture stop reason: {cap.stop_reason}")
    s(f"Stopping: {now_iso()}", echo=False)
    if mqtt_capture:
        mqtt_capture.stop()
    telnet.join(timeout=20)
    if telnet.is_alive():
        s("Telnet capture worker did not stop within 20s.")
    for worker, grace in workers:
        worker.join(timeout=grace)
        if worker.is_alive():
            s(f"{worker.name} capture worker did not stop within {grace}s.")
    if browser is not None:
        browser.stop_browser()

    if args.skip_http_probes:
        s("HTTP probes: disabled (--skip-http-probes).", echo=False)
    else:
        print("Running post-capture HTTP probes -> curl-probes.log", flush=True)
        try:
            run_http_probes(cap, http_base, args.http_probe_timeout_seconds)
            s(f"HTTP probes: ran {len(PROBE_PATHS)} request(s) -> curl-probes.log", echo=False)
        except Exception as exc:
            s("HTTP probes: failed (see error.txt)", echo=False)
            cap.script_error(f"HTTP probes failed: {type(exc).__name__}: {exc}")

    try:
        filename = resolve_metadata(cap, args.device, http_base, run_name)
    except Exception as exc:
        cap.script_error(f"Metadata resolution failed: {exc}")
        filename = f"transcript-{run_name}-unknown-version-{args.device}-unknown-id.txt"
    transcript = run_path / filename
    s(f"Transcript filename: {filename}", echo=False)
    s(f"Finished: {now_iso()}", echo=False)

    try:
        build_tool_error_log(cap)
        merge_transcript(cap, transcript)
    except Exception as exc:
        print(f"Could not build the merged transcript ({exc}); intermediate capture files were kept.", file=sys.stderr)
        print(f"Diagnostic capture folder: {run_path}")
        return 1
    for name in INTERMEDIATE_FILES:
        try:
            cap.path(name).unlink()
        except FileNotFoundError:
            pass
        except OSError:
            pass
    if browser is not None:
        import shutil
        shutil.rmtree(browser.profile_dir, ignore_errors=True)
    for name in (FRAMES_LOG, SIM_LOG):
        if cap.path(name).exists():
            print(f"Raw OpenTherm frames (kept, for replays): {cap.path(name)}")
    print(f"Merged transcript (single upload): {transcript}")
    print(f"Diagnostic capture folder: {run_path}")
    signalled = cap.stop_reason in ("Ctrl+C/Ctrl+Break", "terminated (SIGTERM)", "terminal closed (SIGHUP)")
    return 130 if signalled else 0


if __name__ == "__main__":
    sys.exit(main())
