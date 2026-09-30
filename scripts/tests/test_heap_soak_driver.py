#!/usr/bin/env python3
"""
Self-test for scripts/heap_soak_driver.py (TASK-1036).

Runs the REAL driver as a subprocess against a local stub device, and checks
that it flags what it must flag: a reboot, an outage longer than the
threshold, the replay switching off, and a republish that fails or does not
drain. A clean run must exit 0.

The stub is tied to the firmware, not written from memory. Its response
formats (uptime string, /api/v2/simulate status, republish 200 and 429,
sendApiError, the 503 and 409 messages) are read out of
src/OTGW-firmware/*.ino by anchor when the test starts. Every device/info key
that the stub serves or the driver reads must appear as je.field(F("<key>")
inside sendDeviceInfoV2(). If that contract drifts, these tests fail instead
of passing against a fiction.

What the stub cannot show: the timing and heap behaviour of a real device.
That needs the bench.

Run: python scripts/tests/test_heap_soak_driver.py   (stdlib only, about a minute)
"""

import importlib.util
import json
import re
import shlex
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FW = REPO / "src" / "OTGW-firmware"
DRIVER = REPO / "scripts" / "heap_soak_driver.py"
JSON = "application/json"


# --- reading the firmware contract -------------------------------------------

def c_function(source, anchor_regex):
    """Text of the C function whose definition matches anchor_regex (which must
    end at the opening brace), up to the matching closing brace. Comments and
    string/char literals are skipped while counting braces."""
    m = re.search(anchor_regex, source)
    if not m:
        raise AssertionError(f"anchor not found in the firmware source: {anchor_regex}")
    i, depth, n = m.end() - 1, 0, len(source)
    if source[i] != "{":
        raise AssertionError(f"anchor must end at the opening brace: {anchor_regex}")
    while i < n:
        if source.startswith("//", i):
            i = source.find("\n", i)
            if i < 0:
                break
            continue
        if source.startswith("/*", i):
            end = source.find("*/", i)
            if end < 0:
                break
            i = end + 2
            continue
        c = source[i]
        if c in "\"'":
            j = i + 1
            while j < n and source[j] != c:
                j += 2 if source[j] == "\\" else 1
            i = j + 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return source[m.start():i + 1]
        i += 1
    raise AssertionError(f"no closing brace for: {anchor_regex}")


PSTR_RE = re.compile(r'PSTR\(\s*"((?:[^"\\]|\\.)*)"\s*\)')


def c_unescape(text):
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "r": "\r", "t": "\t"}.get(m.group(1), m.group(1)), text)


def pstr_format(function_text, must_contain):
    """The one single-literal PSTR format in function_text that contains
    must_contain, converted from printf to Python %-format."""
    hits = [c_unescape(m.group(1)) for m in PSTR_RE.finditer(function_text)]
    hits = [h for h in hits if must_contain in h]
    if len(hits) != 1:
        raise AssertionError(f"expected one PSTR containing {must_contain!r}, found {len(hits)}")
    return re.sub(r"%l?u|%S", lambda m: "%s" if m.group(0) == "%S" else "%d", hits[0])


def api_error_message(function_text, status, after=""):
    """Message of the first sendApiError(status, F("...")) after the text `after`."""
    m = re.search(re.escape(after) + r'.*?sendApiError\(' + str(status) + r', F\("([^"]+)"\)\)',
                  function_text, re.S)
    if not m:
        raise AssertionError(f"no sendApiError({status}, ...) found after {after!r}")
    return m.group(1)


class FirmwareContract:
    """Formats, messages and device/info keys read from src/OTGW-firmware."""

    def __init__(self):
        rest = (FW / "restAPI.ino").read_text(encoding="utf-8", errors="replace")
        helper = (FW / "helperStuff.ino").read_text(encoding="utf-8", errors="replace")
        mqtt = (FW / "MQTTstuff.ino").read_text(encoding="utf-8", errors="replace")
        debug = (FW / "handleDebug.ino").read_text(encoding="utf-8", errors="replace")

        self.uptime_fn = c_function(helper, r"String upTime\(\)\s*\{")
        self.sim_status_fn = c_function(rest, r"static void sendSimulationStatus\(\)\s*\{")
        self.simulate_fn = c_function(rest, r"static void handleSimulate\([^)]*\)\s*\{")
        self.discovery_fn = c_function(rest, r"static void handleDiscovery\([^)]*\)\s*\{")
        self.api_error_fn = c_function(rest, r"static void sendApiError\([^)]*\)\s*\{")
        self.device_info_fn = c_function(rest, r"void sendDeviceInfoV2\(\)\s*\{")
        self.mark_all_fn = c_function(mqtt, r"void markAllMQTTConfigPending\(\)\s*\{")
        self.debug_src = debug
        self.rest_src = rest

        self.uptime_fmt = pstr_format(self.uptime_fn, "(H:m)")
        self.sim_status_fmt = pstr_format(self.sim_status_fn, '"available":true')
        self.republish_200_fmt = pstr_format(self.discovery_fn, '"marked_pending"')
        self.republish_429_fmt = pstr_format(self.discovery_fn, "Republish cooldown active")
        self.api_error_fmt = pstr_format(self.api_error_fn, '"error":{"status":%d')
        self.low_heap_msg = api_error_message(self.device_info_fn, 503)
        self.no_mqtt_msg = api_error_message(self.discovery_fn, 503, after='PSTR("republish")')
        self.no_fs_msg = api_error_message(self.simulate_fn, 409)
        cooldown = re.search(r"REPUBLISH_COOLDOWN_MS\s*=\s*(\d+)UL", self.discovery_fn)
        if not cooldown:
            raise AssertionError("REPUBLISH_COOLDOWN_MS not found in handleDiscovery")
        self.republish_cooldown_ms = int(cooldown.group(1))
        self.device_info_keys = set(re.findall(r'je\.field\(F\("([^"]+)"\)', self.device_info_fn))


def load_driver_module():
    spec = importlib.util.spec_from_file_location("heap_soak_driver", DRIVER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# --- the stub device ----------------------------------------------------------

DEFAULT_SCENARIO = dict(
    reboot_at=None,          # device/info request number at which the device restarts
    keep_identity=False,     # restart keeps bootcount and lastreset (reboot-count write failed)
    reboot_dark_sec=0.3,     # no answers while it boots
    outage_at=None,          # device/info request number at which it goes dark
    outage_sec=0.0,
    low_heap_at=None,        # device/info request number from which device/info answers 503
    low_heap_sec=0.0,
    sim_off_at=None,         # device/info request number at which the replay flag drops
    start_refused=False,     # simulate/start answers 409 (LittleFS not mounted)
    dark_from_start=False,   # nothing answers at all
    mqtt_connected=True,
    pending_ids=30,          # count marked pending by a republish
    drain_per_id_sec=0.01,   # the drip publishes one id per this many seconds
    stall_drain=False,       # the drip never publishes
    cooldown_sec=0.5,        # the firmware uses 60 s; shortened so a test can republish often
)


class StubDevice:
    """Stand-in for the firmware's REST and telnet contract, formats from FirmwareContract."""

    def __init__(self, fw, **scenario):
        unknown = set(scenario) - set(DEFAULT_SCENARIO)
        if unknown:
            raise TypeError(f"unknown scenario keys: {sorted(unknown)}")
        self.fw = fw
        self.sc = dict(DEFAULT_SCENARIO, **scenario)
        self.lock = threading.Lock()
        self.inflight = 0
        self.peak_inflight = 0
        self.devinfo_count = 0
        self.reset_base = 0
        self.dark_until = float("inf") if self.sc["dark_from_start"] else 0.0
        self.low_heap_until = 0.0
        self.telnet_keys = []
        self.posts = []
        self.bootcount = 41
        self.lastreset = "Software reset"
        self.fwversion = "2.0.0-alpha.377+stub"
        self.boot_mono = time.monotonic()
        self.uptime_base_s = 3 * 3600 + 15 * 60
        self.sim = False
        self.pending_mark = None
        self.last_republish = None
        self.stopping = False

    # --- lifecycle ---
    def __enter__(self):
        stub = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.0"  # one request per connection, like the firmware

            def log_message(self, *args):
                pass

            def do_GET(self):
                stub.handle(self, "GET")

            def do_POST(self):
                stub.handle(self, "POST")

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.httpd.daemon_threads = True
        self.http_port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, kwargs={"poll_interval": 0.05},
                         daemon=True).start()
        self.telnet_sock = socket.create_server(("127.0.0.1", 0))
        self.telnet_sock.settimeout(0.1)
        self.telnet_port = self.telnet_sock.getsockname()[1]
        threading.Thread(target=self.telnet_loop, daemon=True).start()
        return self

    def __exit__(self, *exc):
        self.stopping = True
        self.httpd.shutdown()
        self.httpd.server_close()
        self.telnet_sock.close()

    # --- telnet: 'z' zeroes the heap-diag window, 's' toggles the replay ---
    def telnet_loop(self):
        while not self.stopping:
            try:
                conn, _ = self.telnet_sock.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            with conn:
                conn.settimeout(2.0)
                try:
                    conn.sendall(b"OTGW stub debug console\r\n")
                    while True:
                        data = conn.recv(64)
                        if not data:
                            break
                        for ch in data.decode(errors="replace"):
                            with self.lock:
                                self.telnet_keys.append(ch)
                                if ch == "z":
                                    self.reset_base = self.devinfo_count
                                elif ch in "sS":
                                    self.sim = not self.sim
                except OSError:
                    pass

    # --- HTTP ---
    def handle(self, req, method):
        if method == "POST":
            length = int(req.headers.get("Content-Length") or 0)
            if length:
                req.rfile.read(length)
        with self.lock:
            self.inflight += 1
            self.peak_inflight = max(self.peak_inflight, self.inflight)
        time.sleep(0.005)  # a concurrent second request would overlap here and be counted
        with self.lock:
            result = self.route(method, req.path)
            self.inflight -= 1  # before the answer goes out, so a sequential client never overlaps
        if result is None:  # dark: close the connection without an answer
            req.close_connection = True
            return
        status, body, ctype = result
        data = body.encode()
        req.send_response(status)
        req.send_header("Content-Type", ctype)
        req.send_header("Content-Length", str(len(data)))
        req.end_headers()
        req.wfile.write(data)

    def route(self, method, path):
        now = time.monotonic()
        if method == "GET" and path == "/api/v2/device/info":
            self.devinfo_count += 1
            self.fire_events(now)
        if now < self.dark_until:
            return None
        if method == "GET" and path == "/api/v2/device/info":
            if now < self.low_heap_until:
                return self.api_error(503, self.fw.low_heap_msg)
            return 200, json.dumps({"device": self.device_fields(now)}), JSON
        if method == "GET" and path == "/api/v2/simulate":
            return 200, self.sim_status(), JSON
        if method == "POST" and path == "/api/v2/simulate/start":
            self.posts.append("start")
            if self.sc["start_refused"]:
                return self.api_error(409, self.fw.no_fs_msg)
            self.sim = True
            return 200, self.sim_status(), JSON
        if method == "POST" and path == "/api/v2/simulate/stop":
            self.posts.append("stop")
            self.sim = False
            return 200, self.sim_status(), JSON
        if method == "POST" and path == "/api/v2/discovery/republish":
            self.posts.append("republish")
            return self.republish(now)
        return 200, "ok", "text/plain"  # web assets, /api/v2/health, /api/v2/sat/status

    def fire_events(self, now):
        sc, n = self.sc, self.devinfo_count
        if n == sc["reboot_at"]:
            if not sc["keep_identity"]:
                self.bootcount += 1
                self.lastreset = "Exception/panic"
            self.boot_mono, self.uptime_base_s = now, 0
            self.sim = False
            self.reset_base = n
            self.pending_mark = self.last_republish = None
            self.dark_until = now + sc["reboot_dark_sec"]
        if n == sc["outage_at"]:
            self.dark_until = now + sc["outage_sec"]
        if n == sc["low_heap_at"]:
            self.low_heap_until = now + sc["low_heap_sec"]
        if n == sc["sim_off_at"]:
            self.sim = False

    def api_error(self, status, message):
        return status, self.fw.api_error_fmt % (status, message), JSON

    def sim_status(self):
        return self.fw.sim_status_fmt % ("true" if self.sim else "false", "/otgw_simulation.log", 1000)

    def pending(self, now):
        if self.pending_mark is None:
            return 0
        marked_at, count = self.pending_mark
        if self.sc["stall_drain"]:
            return count
        return max(0, count - int((now - marked_at) / self.sc["drain_per_id_sec"]))

    def republish(self, now):
        if not self.sc["mqtt_connected"]:
            return self.api_error(503, self.fw.no_mqtt_msg)
        if self.last_republish is not None and now - self.last_republish < self.sc["cooldown_sec"]:
            remaining = int(self.sc["cooldown_sec"] - (now - self.last_republish) + 0.999)
            return 429, self.fw.republish_429_fmt % remaining, JSON
        self.pending_mark = (now, self.sc["pending_ids"])
        self.last_republish = now
        return 200, self.fw.republish_200_fmt % self.sc["pending_ids"], JSON

    def device_fields(self, now):
        up = int(self.uptime_base_s + (now - self.boot_mono))
        k = self.devinfo_count - self.reset_base  # device/info calls since boot or 'z'
        return {
            "fwversion": self.fwversion,
            "bootcount": self.bootcount,
            "lastreset": self.lastreset,
            # the same arguments upTime() passes, with C integer division
            "uptime": self.fw.uptime_fmt % ((up // 86400) % 365, (up // 3600) % 24, (up // 60) % 60),
            "otgwsimulation": self.sim,
            "freeheap": 180000,
            "maxfreeblock": 110000,
            "hd_min_max_block": 12000 - 10 * k,
            "hd_min_free_heap": 150000,
            "hd_max_loop_gap_ms": 40 + 2 * k,
            "hd_enter_low": 0,
            "hd_enter_warning": 0,
            "hd_enter_critical": 0,
            "hd_drip_slowmode": 0,
            "hd_ws_drops": 0,
            "hd_mqtt_drops": 0,
            "hd_rest_503": 0,
            "hd_webfile_503": 0,
            "disc_pending_ids": self.pending(now),
        }


# --- running the driver -------------------------------------------------------

class SoakResult:
    def __init__(self, proc, records, stub):
        self.proc, self.records, self.stub = proc, records, stub
        self.rc = proc.returncode
        self.anomalies = [r for r in records if r["phase"] == "anomaly"]
        self.kinds = [r["kind"] for r in self.anomalies]
        self.summary = next((r for r in records if r["phase"] == "summary"), None)
        self.snapshots = [r for r in records if r["phase"] in ("baseline", "running", "final")]
        self.stdout_lines = proc.stdout.splitlines()

    def debug(self):
        return f"rc={self.rc}\nstdout:\n{self.proc.stdout}\nstderr tail:\n{self.proc.stderr[-3000:]}"


def run_soak(fw, scenario=None, extra=(), duration_s=4.0, poll=0.1, max_unreachable=4.5):
    # One loop iteration with a 4-asset reload (0.3 s pauses) takes about 1.5 s.
    # A short outage between two such iterations leaves no complete snapshot for
    # about 1.5 + outage + 1.5 s, so 4.5 s keeps a 0.3 s outage below the threshold.
    with tempfile.TemporaryDirectory() as tmp, StubDevice(fw, **(scenario or {})) as stub:
        log = Path(tmp) / "soak.ndjson"
        cmd = [sys.executable, str(DRIVER), "--host", "127.0.0.1",
               "--port", str(stub.http_port), "--telnet-port", str(stub.telnet_port),
               "--duration-hours", f"{duration_s / 3600:.8f}",
               "--poll-interval-sec", str(poll), "--max-unreachable-sec", str(max_unreachable),
               "--snapshot-log", str(log), *extra]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=duration_s + 60)
        text = log.read_text(encoding="utf-8") if log.exists() else ""
        records = [json.loads(line) for line in text.splitlines() if line.strip()]
    return SoakResult(proc, records, stub)


class ContractTests(unittest.TestCase):
    """The stub and the driver agree with the firmware source."""

    @classmethod
    def setUpClass(cls):
        cls.fw = FirmwareContract()
        cls.driver = load_driver_module()

    def test_device_info_keys_exist_in_firmware(self):
        self.assertIn('je.beginObject(F("device"))', self.fw.device_info_fn)
        stub_keys = set(StubDevice(self.fw).device_fields(time.monotonic()))
        self.assertLessEqual(set(self.driver.SNAPSHOT_KEYS), stub_keys)
        self.assertLessEqual(stub_keys, self.fw.device_info_keys,
                             sorted(stub_keys - self.fw.device_info_keys))

    def test_uptime_parser_reads_the_firmware_format(self):
        self.assertEqual(self.fw.uptime_fmt % (0, 0, 0), "0(d)-00:00(H:m)")
        self.assertEqual(self.driver.uptime_minutes(self.fw.uptime_fmt % (2, 3, 4)), (2 * 24 + 3) * 60 + 4)
        self.assertIsNone(self.driver.uptime_minutes("garbage"))

    def test_republish_cooldown_matches_firmware(self):
        self.assertEqual(self.fw.republish_cooldown_ms, self.driver.FIRMWARE_REPUBLISH_COOLDOWN_SEC * 1000)

    def test_simulate_and_republish_routes_exist(self):
        self.assertRegex(self.fw.rest_src, r'kRoute\w+\[\]\s+PROGMEM = "simulate"')
        self.assertRegex(self.fw.rest_src, r'kRoute\w+\[\]\s+PROGMEM = "discovery"')
        self.assertIn('PSTR("start")', self.fw.simulate_fn)
        self.assertIn('PSTR("stop")', self.fw.simulate_fn)
        self.assertIn('PSTR("republish")', self.fw.discovery_fn)

    def test_telnet_z_resets_the_heap_window(self):
        self.assertRegex(self.fw.debug_src, r"case 'z':\s*resetHeapWatermark\(\);")

    def test_rest_republish_never_counts_as_republish_triggered(self):
        # Why the driver does not use disc_republish_triggered.
        branch = self.fw.discovery_fn[self.fw.discovery_fn.index('PSTR("republish")'):]
        self.assertIn("markAllMQTTConfigPending();", branch)
        for text in (branch, self.fw.mark_all_fn):
            self.assertNotIn("iRepublishTriggeredCount", text)
            self.assertNotIn("IncRepublishTriggeredCount", text)


class DriverTests(unittest.TestCase):
    """The real driver against the stub."""

    @classmethod
    def setUpClass(cls):
        cls.fw = FirmwareContract()

    def assert_started_run(self, r):
        self.assertEqual(len(r.stdout_lines), 1, r.debug())
        self.assertTrue(r.stdout_lines[0].startswith("SUMMARY verdict="), r.debug())
        fields = dict(tok.split("=", 1) for tok in shlex.split(r.stdout_lines[0])[1:])
        self.assertEqual(fields["verdict"], r.summary["verdict"])
        self.assertEqual(r.stub.peak_inflight, 1, "the driver must send one request at a time")
        self.assertEqual(r.stub.telnet_keys, ["z"], "telnet is used for 'z' only, never 's'")
        self.assertEqual(r.stub.posts[0], "start")
        self.assertEqual(r.stub.posts[-1], "stop")
        self.assertFalse(r.stub.sim, "the replay must be off after the run")
        self.assertTrue(r.snapshots, r.debug())
        for snap in r.snapshots:
            for key in ("bootcount", "lastreset", "uptime", "fwversion", "sim_active"):
                self.assertIn(key, snap)

    def test_clean_run_exits_0(self):
        r = run_soak(self.fw)
        self.assertEqual(r.rc, 0, r.debug())
        self.assert_started_run(r)
        self.assertEqual(r.kinds, [])
        self.assertEqual(r.summary["verdict"], "CLEAN")
        self.assertEqual(r.summary["z_reset"], "ok")
        self.assertEqual(r.summary["snapshots_failed"], 0)
        self.assertEqual(r.summary["hd_min_max_block_min"], min(s["hd_min_max_block"] for s in r.snapshots))
        self.assertEqual(r.summary["hd_max_loop_gap_ms_max"], max(s["hd_max_loop_gap_ms"] for s in r.snapshots))
        self.assertTrue(all(s["sim_active"] for s in r.snapshots))

    def test_reboot_is_flagged_even_when_the_gap_is_short(self):
        r = run_soak(self.fw, dict(reboot_at=4), duration_s=5.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        self.assertIn("reboot", r.kinds)
        self.assertNotIn("unreachable", r.kinds)  # 0.3 s gap: only the identity keys can see it
        signals = " ".join(next(a for a in r.anomalies if a["kind"] == "reboot")["signals"])
        for name in ("bootcount", "lastreset", "uptime"):
            self.assertIn(name, signals)
        self.assertIn("sim_inactive", r.kinds)  # the reboot cleared the replay flag
        self.assertEqual(r.kinds.count("sim_inactive"), 1, "one inactive stretch, one anomaly")
        self.assertEqual(r.summary["verdict"], "ANOMALY")
        self.assertIn("->", r.summary["bootcount"])

    def test_uptime_regression_alone_is_a_reboot(self):
        # bootcount and lastreset unchanged, as when /reboot_count.txt could not be written
        r = run_soak(self.fw, dict(reboot_at=4, keep_identity=True), duration_s=5.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        reboot = next(a for a in r.anomalies if a["kind"] == "reboot")
        self.assertEqual(len(reboot["signals"]), 1)
        self.assertTrue(reboot["signals"][0].startswith("uptime"), reboot)

    def test_outage_longer_than_threshold_is_flagged(self):
        r = run_soak(self.fw, dict(outage_at=4, outage_sec=5.5), duration_s=10.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        self.assertEqual(r.kinds.count("unreachable"), 1, "one outage, one anomaly")
        self.assertNotIn("reboot", r.kinds)
        gap = next(a for a in r.anomalies if a["kind"] == "unreachable")
        self.assertTrue(gap["last_error"].startswith("device/info: no response"), gap)
        self.assertGreater(r.summary["longest_gap_s"], 4.5)
        self.assertGreater(r.summary["snapshots_ok"], 2, "the run must also see the device come back")

    def test_outage_shorter_than_threshold_is_tolerated(self):
        r = run_soak(self.fw, dict(outage_at=4, outage_sec=0.3), duration_s=5.0)
        self.assertEqual(r.rc, 0, r.debug())
        self.assert_started_run(r)
        self.assertGreaterEqual(r.summary["snapshots_failed"], 1)
        self.assertEqual(r.summary["verdict"], "CLEAN")

    def test_device_info_low_heap_503_is_told_apart(self):
        r = run_soak(self.fw, dict(low_heap_at=4, low_heap_sec=5.5), duration_s=10.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        gap = next(a for a in r.anomalies if a["kind"] == "unreachable")
        self.assertIn("HTTP 503 low heap", gap["last_error"])
        self.assertGreaterEqual(r.summary["devinfo_503"], 1)

    def test_replay_switching_off_is_flagged(self):
        r = run_soak(self.fw, dict(sim_off_at=4), duration_s=5.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        self.assertEqual(r.kinds, ["sim_inactive"])
        self.assertGreaterEqual(r.summary["sim_inactive_snapshots"], 1)

    def test_republish_that_drains_is_clean(self):
        r = run_soak(self.fw, extra=("--republish-every-min", "0.02"), duration_s=5.0)
        self.assertEqual(r.rc, 0, r.debug())
        self.assert_started_run(r)
        self.assertGreaterEqual(r.summary["republish_200"], 1)
        self.assertEqual(r.summary["republish_drained"], r.summary["republish_200"])
        self.assertEqual(r.summary["republish_sent"], r.summary["republish_200"])
        statuses = [x["status"] for x in r.records if x["phase"] == "republish"]
        self.assertEqual(statuses, [200] * len(statuses))

    def test_republish_without_mqtt_is_flagged(self):
        r = run_soak(self.fw, dict(mqtt_connected=False), extra=("--republish-every-min", "0.02"),
                     duration_s=5.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        self.assertGreaterEqual(r.summary["republish_503"], 1)
        self.assertEqual(r.summary["republish_503"], r.summary["republish_sent"])
        self.assertEqual(set(r.kinds), {"republish_failed"})
        failed = next(a for a in r.anomalies if a["kind"] == "republish_failed")
        self.assertEqual(failed["status"], 503)
        self.assertIn(self.fw.no_mqtt_msg, failed["error"])

    def test_republish_cooldown_429_is_flagged(self):
        # 6.5 s leaves room for a second POST even after two slow asset-reload iterations
        r = run_soak(self.fw, dict(cooldown_sec=60), extra=("--republish-every-min", "0.02"),
                     duration_s=6.5)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        self.assertEqual(r.summary["republish_200"], 1)
        self.assertGreaterEqual(r.summary["republish_429"], 1)
        self.assertEqual(r.summary["republish_drained"], 1)
        self.assertEqual({a["status"] for a in r.anomalies if a["kind"] == "republish_failed"}, {429})

    def test_republish_that_never_drains_is_flagged(self):
        r = run_soak(self.fw, dict(stall_drain=True), extra=("--republish-every-min", "0.02"),
                     duration_s=5.0)
        self.assertEqual(r.rc, 1, r.debug())
        self.assert_started_run(r)
        self.assertIn("republish_not_drained", r.kinds)
        self.assertEqual(r.summary["republish_drained"], 0)

    def test_refused_replay_start_does_not_run(self):
        r = run_soak(self.fw, dict(start_refused=True))
        self.assertEqual(r.rc, 2, r.debug())
        self.assertEqual(len(r.stdout_lines), 1, r.debug())
        self.assertTrue(r.stdout_lines[0].startswith("SUMMARY verdict=NOT_STARTED"), r.debug())
        self.assertIn("409", r.stdout_lines[0])
        self.assertEqual(r.stub.posts, ["start"])

    def test_unreachable_device_does_not_run(self):
        r = run_soak(self.fw, dict(dark_from_start=True))
        self.assertEqual(r.rc, 2, r.debug())
        self.assertTrue(r.stdout_lines[0].startswith("SUMMARY verdict=NOT_STARTED"), r.debug())
        self.assertEqual(r.stub.telnet_keys, [])
        self.assertEqual(r.stub.posts, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
