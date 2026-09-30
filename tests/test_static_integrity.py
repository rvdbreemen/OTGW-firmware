#!/usr/bin/env python3
"""Self-test for scripts/tests/static_integrity.py (stdlib unittest, no pytest).

A raw-socket stub device runs on 127.0.0.1 inside the test. It answers the way
a healthy or a failing OTGW32 would:
- a whole file
- a body cut short under a longer Content-Length, then a FIN (right away or late)
- a head that arrives late, then a cut body and a FIN at once
- a reset (RST) mid-body
- a stall, and a slow drip
- a close before any header
- a 503
- a size the listing disagrees with, and a file rewritten between listing and GET
- chunked bodies, whole and cut
- an ETag that names a different served file
The golden captures in scripts/json-golden/ supply the device/info and listing
bodies, so the parsers meet the real response shapes. The default idle timeout
is checked against the AsyncTCP ack timeout in platformio.ini.

The tests import the harness and call its real functions: fetch(), classify(),
the parsers, build_parser(), and main() end to end with its CSV output and its
pacing.

Run: python tests/test_static_integrity.py
"""
import contextlib
import csv
import io
import json
import os
import re
import socket
import struct
import sys
import tempfile
import threading
import time
import unittest
from datetime import datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts", "tests"))
import static_integrity as si  # noqa: E402

GOLDEN = os.path.join(ROOT, "scripts", "json-golden")
PLATFORMIO_INI = os.path.join(ROOT, "platformio.ini")
TS_FORMAT = "%Y-%m-%dT%H:%M:%S.%fZ"  # si.utc_now()

FILE_OK = bytes(range(256)) * 8   # 2048 B, served whole
SETTINGS_LEN = 5754               # settings.ini in the golden listing
FIRST_FLIGHT_BODY = 4172          # bench cut: 136 B head + 4172 B = 3 x 1436 B MSS
PART = 3000                       # bytes sent before a cut
LATE_FIN_S = 0.8
SLOW_HEAD_S = 0.8


def _head(status, reason, headers=()):
    lines = ["HTTP/1.1 %d %s" % (status, reason)] + ["%s: %s" % kv for kv in headers]
    return ("\r\n".join(lines) + "\r\n\r\n").encode("latin-1")


def _chunked(payload, size):
    """Chunked encoding as ESPAsyncWebServer writes it: 4-hex-digit sizes and a
    closing "0000\\r\\n\\r\\n" (WebResponses.cpp:449-472)."""
    out = b""
    for i in range(0, len(payload), size):
        part = payload[i:i + size]
        out += b"%04x\r\n" % len(part) + part + b"\r\n"
    return out + b"0000\r\n\r\n"


def _close_with_rst(conn):
    """SO_LINGER on with timeout 0 turns close() into a TCP RST."""
    fmt = "HH" if sys.platform == "win32" else "ii"
    conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack(fmt, 1, 0))
    conn.close()


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class StubDevice:
    """Raw-socket HTTP stub on 127.0.0.1 with one behaviour per path."""

    def __init__(self, devinfo_seq=None):
        with open(os.path.join(GOLDEN, "v2_device_info.json"), "rb") as fh:
            self.devinfo_doc = json.loads(fh.read().decode("utf-8"))
        with open(os.path.join(GOLDEN, "v2_filesystem_files.json"), "rb") as fh:
            self.golden_listing = json.loads(fh.read().decode("utf-8"))
        self.sizes = {e["name"]: e["size"] for e in self.golden_listing
                      if e.get("type") == "file"}
        # (bootcount, uptime) per device/info request; the last one repeats.
        self.devinfo_seq = list(devinfo_seq or [])
        self.listing_503_after = None
        self.listings = 0
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.routes = {
            si.LISTING_PATH: self._listing,
            si.TELEMETRY_PATH: self._devinfo,
            "/api/v2/settings": self._chunked_cut,
            "/ok.js": self._ok,
            "/index.html": self._index_serves_v2,
            "/settings.ini": self._first_flight_then_fin,
            "/late.css": self._late_fin,
            "/slow-head.txt": self._slow_head,
            "/rst.js": self._rst,
            "/stall.js": self._stall,
            "/drip.txt": self._drip,
            "/gate.css": self._gate,
            "/flushed.ini": self._flushed,
            "/short-cl.txt": self._short_cl,
            "/silent": self._silent,
        }
        self.srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(16)
        self.srv.settimeout(0.2)
        self.port = self.srv.getsockname()[1]
        self._thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._thread.start()

    def close(self):
        self.stop.set()
        self._thread.join(2)
        self.srv.close()

    def _accept_loop(self):
        while not self.stop.is_set():
            try:
                conn, _ = self.srv.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            threading.Thread(target=self._serve, args=(conn,), daemon=True).start()

    def _serve(self, conn):
        try:
            conn.settimeout(5.0)
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            req = b""
            while b"\r\n\r\n" not in req:
                data = conn.recv(4096)
                if not data:
                    return
                req += data
            path = req.split(b" ", 2)[1].decode("latin-1")
            self.routes.get(path, self._not_found)(conn)
        except OSError:
            pass
        finally:
            conn.close()

    # --- behaviours ---------------------------------------------------------

    def _listing(self, conn):
        with self.lock:
            self.listings += 1
            n = self.listings
        if self.listing_503_after is not None and n > self.listing_503_after:
            body = b'{"error":{"status":503,"message":"Server busy"}}'
            conn.sendall(_head(503, "Service Unavailable", [
                ("Content-Type", "application/json"), ("Content-Length", len(body)),
                ("Connection", "close")]) + body)
            return
        files = [e for e in self.golden_listing if "name" in e]
        summary = [e for e in self.golden_listing if "name" not in e]
        extra = {"ok.js": len(FILE_OK), "late.css": 5000, "rst.js": 5000,
                 "stall.js": 5000, "drip.txt": 1000, "gate.css": 999,
                 "short-cl.txt": 150,
                 # rewritten after the first listing, as a settings flush would
                 "flushed.ini": 119 if n == 1 else 120}
        files += [{"name": k, "size": v, "type": "file"} for k, v in extra.items()]
        body = json.dumps(files + summary).encode("utf-8")
        conn.sendall(_head(200, "OK", [("Content-Type", "application/json"),
                                       ("Content-Length", len(body)),
                                       ("Connection", "close")]) + body)

    def _devinfo(self, conn):
        with self.lock:
            if len(self.devinfo_seq) > 1:
                boot, up = self.devinfo_seq.pop(0)
            elif self.devinfo_seq:
                boot, up = self.devinfo_seq[0]
            else:
                boot, up = None, None
        doc = json.loads(json.dumps(self.devinfo_doc))
        if boot is not None:
            doc["device"]["bootcount"] = boot
            doc["device"]["uptime"] = up
        raw = _head(200, "OK", [("Content-Type", "application/json"),
                                ("Transfer-Encoding", "chunked"),
                                ("Connection", "close")]) + _chunked(json.dumps(doc).encode("utf-8"), 1000)
        # Odd-sized writes, so chunk and header boundaries land inside a read.
        for a, b in ((0, 7), (7, 180), (180, 1203), (1203, len(raw))):
            conn.sendall(raw[a:b])
            time.sleep(0.02)

    def _chunked_cut(self, conn):
        raw = _head(200, "OK", [("Content-Type", "application/json"),
                                ("Transfer-Encoding", "chunked"),
                                ("Connection", "close")])
        conn.sendall(raw + _chunked(b"a" * 3000, 1000)[:2 * (6 + 1000 + 2)])

    def _ok(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", len(FILE_OK)),
                                       ("Connection", "close")]) + FILE_OK)

    def _index_serves_v2(self, conn):
        body = b"v" * self.sizes["v2.html"]
        conn.sendall(_head(200, "OK", [("ETag", '"deadbeef-/v2.html"'),
                                       ("Content-Length", len(body)),
                                       ("Connection", "close")]) + body)

    def _first_flight_then_fin(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", SETTINGS_LEN),
                                       ("Connection", "close")]) + b"s" * FIRST_FLIGHT_BODY)

    def _late_fin(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", 5000),
                                       ("Connection", "close")]) + b"c" * PART)
        time.sleep(LATE_FIN_S)

    def _slow_head(self, conn):
        time.sleep(SLOW_HEAD_S)
        conn.sendall(_head(200, "OK", [("Content-Length", 5000),
                                       ("Connection", "close")]) + b"h" * PART)

    def _rst(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", 5000),
                                       ("Connection", "close")]) + b"r" * PART)
        time.sleep(0.3)  # let the client read the partial body first
        _close_with_rst(conn)

    def _stall(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", 5000),
                                       ("Connection", "close")]) + b"t" * PART)
        self.stop.wait(10)

    def _drip(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", 1000), ("Connection", "close")]))
        for _ in range(1000):
            if self.stop.is_set():
                return
            conn.sendall(b"d")
            time.sleep(0.1)

    def _gate(self, conn):
        conn.sendall(_head(503, "Service Unavailable", [("Retry-After", "1"),
                                                        ("Content-Length", "0"),
                                                        ("Connection", "close")]))

    def _flushed(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", 120),
                                       ("Connection", "close")]) + b"f" * 120)

    def _short_cl(self, conn):
        conn.sendall(_head(200, "OK", [("Content-Length", 100),
                                       ("Connection", "close")]) + b"x" * 100)

    def _silent(self, conn):
        return  # _serve() closes: a FIN before any byte of a response

    def _not_found(self, conn):
        body = b"FileNotFound\r\n"
        conn.sendall(_head(404, "Not Found", [("Content-Length", len(body)),
                                              ("Connection", "close")]) + body)


class ChunkedDecoderTests(unittest.TestCase):
    def test_byte_by_byte(self):
        payload = bytes(range(256)) * 11  # chunks of 1000, 1000 and 816 B
        raw = _chunked(payload, 1000)
        d = si.ChunkedDecoder(keep=True)
        for i in range(len(raw)):
            self.assertFalse(d.done, "done before byte %d of %d" % (i, len(raw)))
            d.feed(raw[i:i + 1])
        self.assertTrue(d.done)
        self.assertEqual((d.payload, bytes(d.data), d.error), (len(payload), payload, ""))

    def test_cut_before_the_last_chunk_is_not_done(self):
        d = si.ChunkedDecoder()
        d.feed(_chunked(b"x" * 2500, 1000)[:-len(b"0000\r\n\r\n")])
        self.assertFalse(d.done)
        self.assertEqual(d.payload, 2500)

    def test_malformed_size_line(self):
        d = si.ChunkedDecoder()
        d.feed(b"zz\r\nabc\r\n0000\r\n\r\n")
        self.assertFalse(d.done)
        self.assertEqual(d.error, "bad-chunk-size")

    def test_missing_crlf_after_data(self):
        d = si.ChunkedDecoder()
        d.feed(b"0003\r\nabcXX0000\r\n\r\n")
        self.assertFalse(d.done)
        self.assertEqual(d.error, "bad-chunk-crlf")


class ParserTests(unittest.TestCase):
    def test_listing_from_the_golden_capture(self):
        with open(os.path.join(GOLDEN, "v2_filesystem_files.json"), "rb") as fh:
            sizes, summary = si.parse_listing(fh.read())
        self.assertEqual(sizes["settings.ini"], 5754)
        self.assertEqual(sizes["sat-slider.js"], 1741)
        self.assertEqual(sizes["reboot_log.txt"], 107)
        self.assertNotIn("fonts", sizes)  # directories are skipped
        self.assertIs(summary["truncated"], False)

    def test_listing_must_be_an_array(self):
        with self.assertRaises(ValueError):
            si.parse_listing(b'{"name": "x"}')

    def test_telemetry_from_the_golden_capture(self):
        with open(os.path.join(GOLDEN, "v2_device_info.json"), "rb") as fh:
            raw = fh.read()
        # Every key the probe reads is in the captured response. A misspelt key
        # would not raise: parse_telemetry() would return None for it.
        device = json.loads(raw.decode("utf-8"))["device"]
        self.assertEqual(set(si.TELEMETRY_KEYS) - set(device), set())
        tel = si.parse_telemetry(raw)
        self.assertEqual(
            (tel["fwversion"], tel["freeheap"], tel["maxfreeblock"], tel["hd_min_max_block"],
             tel["hd_webfile_503"], tel["hd_rest_503"], tel["hd_tcp_active_pcbs"],
             tel["bootcount"], tel["uptime"]),
            ("2.0.0-alpha.333+d16ee2f", 91036, 45044, 31732, 0, 0, 1, 2, "0(d)-00:07(H:m)"))

    def test_uptime_minutes(self):
        self.assertEqual(si.uptime_minutes("0(d)-00:07(H:m)"), 7)
        self.assertEqual(si.uptime_minutes("2(d)-03:04(H:m)"), 2 * 1440 + 3 * 60 + 4)
        self.assertIsNone(si.uptime_minutes("garbage"))
        self.assertIsNone(si.uptime_minutes(None))

    def test_detect_reboot(self):
        base = {"bootcount": 2, "uptime": "0(d)-01:00(H:m)", "reboot_log_size": 107}
        self.assertEqual(si.detect_reboot(base, dict(base, uptime="0(d)-01:05(H:m)")), "")
        self.assertIn("bootcount 2->3", si.detect_reboot(base, dict(base, bootcount=3)))
        # An FS image flash restarts the count, so a drop counts too.
        self.assertIn("bootcount 2->1", si.detect_reboot(base, dict(base, bootcount=1)))
        self.assertIn("uptime 0(d)-01:00(H:m)->0(d)-00:01(H:m)",
                      si.detect_reboot(base, dict(base, uptime="0(d)-00:01(H:m)")))
        self.assertIn("reboot_log.txt 107->214 B",
                      si.detect_reboot(base, dict(base, reboot_log_size=214)))
        self.assertEqual(si.detect_reboot({}, base), "")
        self.assertEqual(si.detect_reboot(base, {"uptime": "0(d)-01:06(H:m)"}), "")

    def test_served_path_follows_the_etag(self):
        self.assertEqual(si.served_path("/index.html", '"a1b2c3d-/v2.html"'), "/v2.html")
        self.assertEqual(si.served_path("/index.html", 'W/"a1b2c3d-/index.html"'), "/index.html")
        self.assertEqual(si.served_path("/settings.ini", ""), "/settings.ini")
        self.assertEqual(si.served_path("/x.js?v=1", ""), "/x.js")

    def test_gap_bucket(self):
        self.assertEqual([si.gap_bucket(v) for v in (12.0, 1500, 5000, 10000, None)],
                         ["gap<1s", "gap1-4s", "gap4-7s", "gap>=7s", "-"])


class DefaultsTests(unittest.TestCase):
    def test_defaults_fit_the_firmware_ack_timeout(self):
        # AsyncTCP closes a connection whose data stays unacked for
        # CONFIG_ASYNC_TCP_MAX_ACK_TIME ms. The default idle timeout must outlast
        # that, or such a close reads as a stall. The ack time must also fall in
        # the gap4-7s bucket that the summary sorts it into.
        with open(PLATFORMIO_INI, encoding="utf-8") as fh:
            acks = [int(v) for v in re.findall(r"CONFIG_ASYNC_TCP_MAX_ACK_TIME=(\d+)", fh.read())]
        self.assertTrue(acks, "no CONFIG_ASYNC_TCP_MAX_ACK_TIME in platformio.ini")
        args = si.build_parser().parse_args([])
        self.assertGreater(args.idle_timeout * 1000.0, max(acks))
        self.assertEqual({si.gap_bucket(a) for a in acks}, {"gap4-7s"})
        # Pacing is on by default: a gate slot is freed only on disconnect.
        self.assertGreater(args.gap, 0)


class FetchTests(unittest.TestCase):
    """fetch() and classify() against each stub behaviour."""

    @classmethod
    def setUpClass(cls):
        cls.dev = StubDevice()

    @classmethod
    def tearDownClass(cls):
        cls.dev.close()

    def get(self, path, **kw):
        opts = dict(connect_timeout=2.0, idle_timeout=2.0, max_time=5.0)
        opts.update(kw)
        return si.fetch("127.0.0.1", self.dev.port, path, **opts)

    def test_complete(self):
        r = self.get("/ok.js")
        self.assertEqual((r["close"], r["status"], r["framing"]), ("complete", 200, "length"))
        self.assertEqual((r["content_length"], r["body_bytes"]), (len(FILE_OK), len(FILE_OK)))
        self.assertEqual(r["wire_bytes"], r["head_bytes"] + r["body_bytes"])
        self.assertEqual(si.classify(r, len(FILE_OK)), "ok")

    def test_fin_right_after_a_short_body(self):
        r = self.get("/settings.ini")
        self.assertEqual((r["close"], r["status"]), ("FIN-short", 200))
        self.assertEqual((r["content_length"], r["body_bytes"]), (SETTINGS_LEN, FIRST_FLIGHT_BODY))
        self.assertLess(r["close_gap_ms"], 500)
        self.assertEqual(si.classify(r, SETTINGS_LEN), "short")

    def test_late_fin_gap_is_measured(self):
        r = self.get("/late.css")
        self.assertEqual((r["close"], r["body_bytes"]), ("FIN-short", PART))
        self.assertGreater(r["close_gap_ms"], LATE_FIN_S * 1000 * 0.75)
        self.assertLess(r["close_gap_ms"], LATE_FIN_S * 1000 + 1000)

    def test_slow_head_times_each_phase(self):
        # The head comes late; the cut body and the FIN come together. So ttfb and
        # elapsed are long while the close gap, counted from the last byte, is short.
        r = self.get("/slow-head.txt")
        self.assertEqual((r["close"], r["status"], r["body_bytes"]), ("FIN-short", 200, PART))
        self.assertLess(r["connect_ms"], 300)
        self.assertIsNotNone(r["ttfb_ms"])
        self.assertGreater(r["ttfb_ms"], SLOW_HEAD_S * 1000 * 0.75)
        self.assertGreater(r["elapsed_ms"], SLOW_HEAD_S * 1000 * 0.75)
        self.assertLessEqual(r["ttfb_ms"], r["elapsed_ms"])
        self.assertLess(r["close_gap_ms"], 300)
        self.assertEqual(si.gap_bucket(r["close_gap_ms"]), "gap<1s")

    def test_rst_mid_body(self):
        r = self.get("/rst.js")
        self.assertEqual((r["close"], r["body_bytes"]), ("RST", PART))
        self.assertIn("Connection", r["detail"])  # ConnectionResetError[...]
        self.assertEqual(si.classify(r, 5000), "short")

    def test_stall_times_out_on_idle(self):
        r = self.get("/stall.js", idle_timeout=0.5)
        self.assertEqual((r["close"], r["detail"], r["body_bytes"]), ("timeout", "idle", PART))
        self.assertGreater(r["close_gap_ms"], 400)
        self.assertEqual(si.classify(r, 5000), "short")

    def test_slow_drip_hits_max_time(self):
        # Bytes keep arriving, so only the total deadline can end the request.
        r = self.get("/drip.txt", idle_timeout=0.5, max_time=1.0)
        self.assertEqual((r["close"], r["detail"]), ("timeout", "max-time"))
        self.assertTrue(0 < r["body_bytes"] < 1000)
        self.assertLess(r["elapsed_ms"], 1500)

    def test_stall_past_max_time(self):
        # No byte arrives and max-time is shorter than idle: the read itself times out.
        r = self.get("/stall.js", idle_timeout=2.0, max_time=0.5)
        self.assertEqual((r["close"], r["detail"], r["body_bytes"]), ("timeout", "max-time", PART))
        self.assertLess(r["elapsed_ms"], 1500)

    def test_close_before_headers(self):
        r = self.get("/silent")
        self.assertEqual((r["close"], r["status"], r["wire_bytes"]), ("FIN-short", None, 0))
        self.assertEqual(si.classify(r), "no-response")

    def test_connect_fail(self):
        r = si.fetch("127.0.0.1", free_port(), "/ok.js",
                     connect_timeout=0.5, idle_timeout=1.0, max_time=2.0)
        self.assertEqual((r["close"], r["status"]), ("connect-fail", None))
        self.assertTrue(r["detail"])  # refused or timed out, depending on the OS
        self.assertEqual(si.classify(r), "no-connect")

    def test_gate_503(self):
        r = self.get("/gate.css")
        self.assertEqual((r["close"], r["status"], r["content_length"]), ("complete", 503, 0))
        self.assertEqual(si.classify(r, 999), "gate")

    def test_chunked_whole(self):
        r = self.get(si.TELEMETRY_PATH, keep_body=True)
        self.assertEqual((r["close"], r["framing"], r["content_length"]),
                         ("complete", "chunked", None))
        self.assertEqual(si.parse_telemetry(r["body"])["freeheap"], 91036)
        self.assertEqual(si.classify(r), "ok")

    def test_chunked_cut(self):
        r = self.get("/api/v2/settings")
        self.assertEqual((r["close"], r["status"], r["framing"], r["body_bytes"]),
                         ("FIN-short", 200, "chunked", 2000))
        self.assertEqual(si.classify(r), "short")

    def test_complete_but_smaller_than_listed(self):
        r = self.get("/short-cl.txt")
        self.assertEqual((r["close"], r["content_length"]), ("complete", 100))
        self.assertEqual(si.classify(r, 150), "size-mismatch")
        self.assertEqual(si.classify(r, 100), "ok")


class CommandLineTests(unittest.TestCase):
    """main() end to end: batches, telemetry, listing, verdicts, CSV and exit code."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.prefix = os.path.join(tmp.name, "run")

    def run_cli(self, port, targets, batches=1, extra=()):
        argv = ["--host", "127.0.0.1", "--port", str(port),
                "--targets", ",".join(targets), "--batches", str(batches),
                "--gap", "0", "--idle-timeout", "1.5", "--max-time", "5",
                "--connect-timeout", "1", "--label", "selftest",
                "--csv", self.prefix] + list(extra)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            code = si.main(argv)
        with open(self.prefix + "-requests.csv", newline="", encoding="utf-8") as fh:
            requests = list(csv.DictReader(fh))
        with open(self.prefix + "-batches.csv", newline="", encoding="utf-8") as fh:
            batches = list(csv.DictReader(fh))
        return code, requests, batches, buf.getvalue()

    def stub(self, **kw):
        dev = StubDevice(**kw)
        self.addCleanup(dev.close)
        return dev

    def test_every_classification(self):
        dev = self.stub()
        # /flushed.ini must come before /short-cl.txt: the listing re-read that
        # /short-cl.txt triggers would otherwise already show the new size.
        targets = ["/ok.js", "/index.html", "/settings.ini", "/late.css", "/rst.js",
                   "/stall.js", "/gate.css", "/flushed.ini", "/short-cl.txt",
                   "/missing.txt", "/silent", si.TELEMETRY_PATH, "/api/v2/settings"]
        code, reqs, batches, text = self.run_cli(dev.port, targets)
        self.assertEqual({r["path"]: (r["verdict"], r["close"]) for r in reqs}, {
            "/ok.js": ("ok", "complete"),
            "/index.html": ("ok", "complete"),
            "/settings.ini": ("short", "FIN-short"),
            "/late.css": ("short", "FIN-short"),
            "/rst.js": ("short", "RST"),
            "/stall.js": ("short", "timeout"),
            "/gate.css": ("gate", "complete"),
            "/flushed.ini": ("size-changed", "complete"),
            "/short-cl.txt": ("size-mismatch", "complete"),
            "/missing.txt": ("http-error", "complete"),
            "/silent": ("no-response", "FIN-short"),
            si.TELEMETRY_PATH: ("ok", "complete"),
            "/api/v2/settings": ("short", "FIN-short"),
        })
        self.assertEqual(code, 1)
        row = {r["path"]: r for r in reqs}
        # /index.html is sized as the /v2.html its ETag names, not as index.html.
        self.assertEqual((row["/index.html"]["served"], row["/index.html"]["expected"]),
                         ("/v2.html", str(dev.sizes["v2.html"])))
        self.assertEqual((row["/settings.ini"]["content_length"], row["/settings.ini"]["body_bytes"],
                          row["/settings.ini"]["expected"]), ("5754", "4172", "5754"))
        self.assertEqual(row["/stall.js"]["detail"], "idle")
        b = batches[0]
        self.assertEqual((b["tel_status"], b["tel_close"], b["freeheap"], b["maxfreeblock"],
                          b["hd_min_max_block"], b["hd_webfile_503"], b["hd_rest_503"]),
                         ("200", "complete", "91036", "45044", "31732", "0", "0"))
        self.assertEqual((b["list_status"], b["list_truncated"], b["reboot_log_size"], b["reboot"]),
                         ("200", "False", "107", ""))
        # The signature table names the bench fingerprint: cut at 4172 of 5754 B, FIN under 1 s.
        self.assertRegex(text, r"1x /settings\.ini\s+short\s+FIN-short\s+200 wire=\d+ "
                               r"body=4172/5754 exp=5754 gap<1s")
        self.assertIn("VERDICT: FAILURES (8 of 13 requests, 0 reboots)", text)

    def test_clean_run_exits_zero(self):
        dev = self.stub()
        code, reqs, batches, text = self.run_cli(
            dev.port, ["/ok.js", "/index.html", "/gate.css", si.TELEMETRY_PATH], batches=2)
        self.assertEqual(code, 0)
        self.assertEqual(len(reqs), 8)
        self.assertEqual({r["verdict"] for r in reqs}, {"ok", "gate"})
        self.assertEqual([b["reboot"] for b in batches], ["", ""])
        self.assertIn("VERDICT: CLEAN (8 requests)", text)

    def test_reboot_between_batches_fails_the_run(self):
        dev = self.stub(devinfo_seq=[(2, "0(d)-00:07(H:m)"), (3, "0(d)-00:01(H:m)")])
        code, reqs, batches, text = self.run_cli(dev.port, ["/ok.js"], batches=2)
        self.assertEqual(code, 1)
        self.assertEqual([r["verdict"] for r in reqs], ["ok", "ok"])
        self.assertEqual(batches[0]["reboot"], "")
        self.assertIn("bootcount 2->3", batches[1]["reboot"])
        self.assertIn("uptime 0(d)-00:07(H:m)->0(d)-00:01(H:m)", batches[1]["reboot"])
        self.assertIn("*** REBOOT before batch 2", text)

    def test_minutes_limit_ends_an_unbounded_run(self):
        dev = self.stub()
        t0 = time.perf_counter()
        code, reqs, batches, text = self.run_cli(dev.port, ["/ok.js"], batches=0,
                                                 extra=["--minutes", "0.005", "--gap", "0.05"])
        self.assertLess(time.perf_counter() - t0, 5.0)
        self.assertEqual(code, 0)
        self.assertGreaterEqual(len(batches), 1)
        self.assertEqual(len(reqs), len(batches))

    def test_gap_paces_every_request(self):
        # One batch over two clean targets pauses four times: after the
        # telemetry, after the listing and after each target.
        dev = self.stub()
        gap = 0.3
        t0 = time.perf_counter()
        code, reqs, batches, text = self.run_cli(dev.port, ["/ok.js", "/gate.css"],
                                                 extra=["--gap", str(gap)])
        took = time.perf_counter() - t0
        self.assertEqual(code, 0)
        self.assertGreaterEqual(took, 4 * gap * 0.9)
        start = datetime.strptime(batches[0]["ts"], TS_FORMAT)
        done = [datetime.strptime(r["ts"], TS_FORMAT) for r in reqs]
        self.assertGreaterEqual((done[0] - start).total_seconds(), 2 * gap * 0.9)
        self.assertGreaterEqual((done[1] - done[0]).total_seconds(), gap * 0.9)

    def test_listing_outage_keeps_the_last_sizes(self):
        dev = self.stub()
        dev.listing_503_after = 1
        code, reqs, batches, text = self.run_cli(dev.port, ["/ok.js"], batches=2)
        self.assertEqual(code, 0)
        self.assertEqual([r["expected"] for r in reqs], [str(len(FILE_OK))] * 2)
        self.assertEqual([b["list_status"] for b in batches], ["200", "503"])
        self.assertIn("listing: 1 batches used a stale listing", text)

    def test_dead_device_is_recorded_not_fatal(self):
        code, reqs, batches, text = self.run_cli(
            free_port(), ["/ok.js", si.TELEMETRY_PATH], extra=["--connect-timeout", "0.3"])
        self.assertEqual(code, 1)
        self.assertEqual([(r["verdict"], r["close"]) for r in reqs],
                         [("no-connect", "connect-fail")] * 2)
        self.assertEqual((batches[0]["tel_close"], batches[0]["list_close"], batches[0]["freeheap"]),
                         ("connect-fail", "connect-fail", ""))
        self.assertIn("telemetry: 0 of 1 batches answered", text)


if __name__ == "__main__":
    unittest.main(verbosity=2)
