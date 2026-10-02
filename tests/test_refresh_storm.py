"""Self-test for scripts/tests/refresh_storm.py (TASK-1124) against a local stub.

StubDevice below is a test double of the device's web stack, written for this
test. It models only what the tool has to tell apart. A green run proves the
tool's mechanics and classification. It proves nothing about the firmware:
that takes the bench run.

Every detector is shown firing both ways:
- upload discriminator: a stub that closes the handle on abort gives
  prefix_current and PASS. One that keeps it open until the next upload starts
  (the static File of alpha.359) gives stale_previous and FAIL.
- rst rule: a stub whose RST drops the queued data before the file is opened
  also reads back stale_previous. Without a stale fin or stall readback that is
  INCONCLUSIVE, not FAIL. The alpha.359 leak in a mixed run still FAILs.
- control: a failed control upload stops the run as INCONCLUSIVE.
- password: a 401 stops either mode as INCONCLUSIVE before any load.
- TASK-1162 guard: a short 200 is classified short and retried, never judged.
- gate balance: a clean stub passes. One leaked REST slot at cap 2 passes the
  sequential GETs and fails the overlapping pair. A storm on the labels route
  FAILs on a stub that leaks its slot (alpha.224 to alpha.378, TASK-1172) and
  PASSes on one that does not.
- abort on the wire: the stub records how each client left (FIN, RST or its own
  RX timeout), so the three abort modes are proven distinct on this OS.

Run: python tests/test_refresh_storm.py
"""
import base64
import contextlib
import hashlib
import importlib.util
import io
import json
import os
import select
import shutil
import socket
import struct
import sys
import tempfile
import threading
import time
import types
import unittest
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = ROOT / "scripts" / "tests" / "refresh_storm.py"


def _load_tool():
    spec = importlib.util.spec_from_file_location("refresh_storm", TOOL_PATH)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


rs = _load_tool()

try:
    import websocket  # noqa: F401  (websocket-client)
    HAVE_WS = True
except ImportError:
    HAVE_WS = False

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
PIECE = 1460
HELPER_HTML = b"<br>You first need to upload these two files:\n<ul><li>FSexplorer.html</li></ul>\n"
SMALL_ASSET = b"a" * 3000


class StubDevice:
    """Threaded HTTP stub. One thread per connection, Connection: close."""

    AUTH_PATHS = ("/", "/api/v2/settings", "/upload", "/api/listfiles")

    def __init__(self, upload_model="close_on_abort", rx_timeout=1.0, rest_cap=2, file_cap=2,
                 leaked_rest_slots=0, maxfreeblock=50000, big_body=200000, helper_page=False,
                 leak_route=None, ws_cap=3, rst_drops_queue=False, upload_fail=False, auth=False):
        self.upload_model = upload_model
        self.rst_drops_queue = rst_drops_queue    # an RST removes the data before the handler runs
        self.upload_fail = upload_fail            # a full filesystem: nothing stored, 507
        self.auth = auth                          # an HTTP password: 401 on AUTH_PATHS
        self.rx_timeout = rx_timeout
        self.rest_cap, self.file_cap = rest_cap, file_cap
        self.leaked_rest_slots = leaked_rest_slots
        self.maxfreeblock = maxfreeblock
        self.big_body = big_body
        self.helper_page = helper_page
        self.leak_route = leak_route
        self.ws_cap = ws_cap
        self.lock = threading.Lock()
        self.files = {}
        self.leaked = None
        self.upload_exits = []
        self.get_exits = []
        self.truncate_next = 0
        self.truncate_always = False
        self.rest_inflight = self.file_inflight = 0
        self.rest_503 = self.file_503 = 0
        self.ws_active = self.ws_opens = 0
        self.bootcount = 7
        self.t0 = time.time()
        self._stop = threading.Event()
        self.srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(128)
        self.hostport = "127.0.0.1:%d" % self.srv.getsockname()[1]
        threading.Thread(target=self._accept_loop, daemon=True).start()

    def close(self):
        self._stop.set()
        try:
            self.srv.close()
        except OSError:
            pass

    # -- plumbing --------------------------------------------------------------
    def _accept_loop(self):
        while not self._stop.is_set():
            try:
                c, _ = self.srv.accept()
            except OSError:
                return
            try:
                c.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
            except OSError:
                pass
            threading.Thread(target=self._handle, args=(c,), daemon=True).start()

    def _handle(self, c):
        try:
            c.settimeout(5.0)
            buf = bytearray()
            while b"\r\n\r\n" not in buf:
                d = c.recv(65536)
                if not d:
                    return
                buf += d
            end = buf.find(b"\r\n\r\n")
            lines = bytes(buf[:end]).decode("latin-1").split("\r\n")
            method, target, _ = lines[0].split(" ", 2)
            headers = {}
            for ln in lines[1:]:
                k, _, v = ln.partition(":")
                headers[k.strip().lower()] = v.strip()
            rest = bytes(buf[end + 4:])
            path, _, query = target.partition("?")
            c.settimeout(30.0)
            if self.auth and path in self.AUTH_PATHS and (path != "/api/listfiles" or "delete=" in query):
                self._unauthorized(c, headers, rest)
            elif method == "POST" and path == "/upload":
                self._upload(c, headers, rest)
            elif path == "/ws" and headers.get("upgrade", "").lower() == "websocket":
                self._ws(c, headers)
            elif method == "GET":
                self._get(c, path, query)
            else:
                self._simple(c, 405, b"")
        except OSError:
            pass
        finally:
            try:
                c.close()
            except OSError:
                pass

    @staticmethod
    def _head(status, ctype, length=None, chunked=False, extra=None):
        h = ["HTTP/1.1 %d X" % status, "Connection: close"]
        if ctype:
            h.append("Content-Type: %s" % ctype)
        if chunked:
            h.append("Transfer-Encoding: chunked")
        elif length is not None:
            h.append("Content-Length: %d" % length)
        for k, v in (extra or {}).items():
            h.append("%s: %s" % (k, v))
        return ("\r\n".join(h) + "\r\n\r\n").encode("latin-1")

    def _simple(self, c, status, body, ctype="text/plain", extra=None):
        c.sendall(self._head(status, ctype, len(body), extra=extra) + body)

    def _unauthorized(self, c, headers, rest):
        # Read the body first, like the device, whose /upload answer comes after it.
        need = int(headers.get("content-length", "0")) - len(rest)
        while need > 0:
            d = c.recv(65536)
            if not d:
                return
            need -= len(d)
        self._simple(c, 401, b"", None, {"WWW-Authenticate": 'Basic realm="stub"'})

    @staticmethod
    def _client_left(c):
        try:
            r, _, _ = select.select([c], [], [], 0)
        except (OSError, ValueError):
            return "rst"
        if not r:
            return None
        try:
            d = c.recv(4096)
        except OSError:
            return "rst"
        return "fin" if not d else None

    def _stream(self, c, status, body, ctype, chunked=False, extra=None, cut_at=None):
        """Send in 1460-byte pieces and notice a client that leaves between them.
        Returns (how it ended, longest gap between two pieces)."""
        c.sendall(self._head(status, ctype, None if chunked else len(body), chunked, extra))
        limit = len(body) if cut_at is None else cut_at
        pos, max_gap, last = 0, 0.0, time.monotonic()
        while pos < limit:
            left = self._client_left(c)
            if left:
                return left, max_gap
            piece = body[pos:min(pos + PIECE, limit)]
            data = (b"%x\r\n" % len(piece) + piece + b"\r\n") if chunked else piece
            try:
                c.sendall(data)
            except OSError:
                return "rst", max_gap
            now = time.monotonic()
            max_gap, last = max(max_gap, now - last), now
            pos += len(piece)
        if cut_at is not None:
            return "cut", max_gap
        if chunked:
            try:
                c.sendall(b"0\r\n\r\n")
            except OSError:
                return "rst", max_gap
        return "complete", max_gap

    def _note_get(self, path, exit_, gap):
        with self.lock:
            self.get_exits.append({"path": path, "exit": exit_, "max_gap_s": round(gap, 3)})

    # -- routes ------------------------------------------------------------------
    def _get(self, c, path, query):
        if path == "/api/listfiles":
            return self._listfiles(c, query)
        if path.startswith("/t/"):
            return self._special(c, path[3:])
        if path.startswith("/api/"):
            return self._rest(c, path)
        return self._file(c, path)

    def _rest_body(self, path):
        if path == "/api/v2/device/info":
            up = int((time.time() - self.t0) // 60) + 5
            with self.lock:
                dev = {"fwversion": "stub", "hardware_type": "stub", "bootcount": self.bootcount,
                       "lastreset": "stub power on",
                       "uptime": "%d(d)-%02d:%02d(H:m)" % (up // 1440, (up // 60) % 24, up % 60),
                       "freeheap": 100000, "maxfreeblock": self.maxfreeblock,
                       "internal_free": 100000, "internal_maxblk": self.maxfreeblock, "psram_found": 0,
                       "hd_min_max_block": self.maxfreeblock, "hd_min_free_heap": 90000,
                       "hd_max_loop_gap_ms": 5, "hd_tcp_active_pcbs": 2,
                       "hd_rest_503": self.rest_503, "hd_webfile_503": self.file_503,
                       "hd_rest_inflight_hwm": 2, "hd_webfile_inflight_hwm": 2,
                       "hd_fragmentation_pct": 10, "hd_ws_drops": 0, "mqttconnected": False}
            return json.dumps({"device": dev}).encode()
        if path == "/api/v2/settings":
            return json.dumps({"settings": {"pad": "x" * self.big_body}}).encode()
        if path == "/api/v2/sat/status":
            return json.dumps({"sat": {"enabled": False}}).encode()
        if path == "/api/v2/sat/ble/discovery":
            return json.dumps({"psram": 0, "ble_enable": False, "risk_ack": False, "active": False}).encode()
        if path == "/api/v2/sensors/labels":
            return b"{}"
        return None

    def _rest(self, c, path):
        with self.lock:
            busy = self.rest_inflight + self.leaked_rest_slots >= self.rest_cap
            if busy:
                self.rest_503 += 1
            else:
                self.rest_inflight += 1
        if busy:
            return self._simple(c, 503, b'{"error":{"status":503,"message":"Server busy: too many '
                                        b'concurrent requests, please retry"}}', "application/json")
        try:
            body = self._rest_body(path)
            if body is None:
                return self._simple(c, 404, b'{"error":{"status":404}}', "application/json")
            self._note_get(path, *self._stream(c, 200, body, "application/json", chunked=True))
        finally:
            with self.lock:
                self.rest_inflight -= 1
                if path == self.leak_route:
                    self.leaked_rest_slots += 1     # the slot never comes back

    def _file(self, c, path):
        with self.lock:
            busy = self.file_inflight >= self.file_cap
            if busy:
                self.file_503 += 1
            else:
                self.file_inflight += 1
        if busy:
            return self._simple(c, 503, b"", None, {"Retry-After": "1"})
        try:
            if self.helper_page:
                return self._simple(c, 200, HELPER_HTML, "text/html; charset=UTF-8")
            with self.lock:
                stored = self.files.get(path)
                cut = None
                if stored is not None and (self.truncate_always or self.truncate_next > 0):
                    self.truncate_next = max(0, self.truncate_next - 1)
                    cut = len(stored) // 2
            if stored is not None:
                self._note_get(path, *self._stream(c, 200, stored, "text/plain", cut_at=cut))
            elif path in rs.DEFAULT_PATHS:
                body = b"s" * self.big_body if path == "/" else SMALL_ASSET
                self._note_get(path, *self._stream(c, 200, body, "text/html"))
            else:
                self._simple(c, 404, b"FileNotFound\r\n")
        finally:
            with self.lock:
                self.file_inflight -= 1

    def _listfiles(self, c, query):
        params = dict(urllib.parse.parse_qsl(query))
        if "delete" not in params:
            return self._simple(c, 200, b"[]", "application/json")
        p = params["delete"]
        p = p if p.startswith("/") else "/" + p
        with self.lock:
            existed = self.files.pop(p, None) is not None
        if existed:
            self._simple(c, 200, b"File deleted")
        else:
            self._simple(c, 404, b"File not found")

    def _special(self, c, name):
        k3 = b"k" * 3000
        if name == "ok-cl":
            self._simple(c, 200, k3)
        elif name == "ok-chunked":
            self._stream(c, 200, k3, "application/json", chunked=True)
        elif name == "ok-close":
            c.sendall(b"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\n" + k3)
        elif name == "short":
            c.sendall(self._head(200, "text/plain", 3000) + k3[:1500])
        elif name == "chunk-cut":
            c.sendall(self._head(200, "application/json", chunked=True) + b"5dc\r\n" + k3[:1500] + b"\r\n")
        elif name == "304":
            c.sendall(b"HTTP/1.1 304 Not Modified\r\nConnection: close\r\nETag: \"x\"\r\n\r\n")
        elif name == "429":
            self._simple(c, 429, b'{"status":429,"retry_after":2}', "application/problem+json", {"Retry-After": "2"})
        elif name == "503-busy":
            self._simple(c, 503, b'{"error":{"status":503,"message":"Server busy: too many concurrent '
                                 b'requests, please retry"}}', "application/json")
        elif name == "503-lowheap":
            self._simple(c, 503, b'{"error":{"status":503,"message":"low heap"}}', "application/json")
        elif name == "503-gate":
            self._simple(c, 503, b"", None, {"Retry-After": "1"})
        elif name == "500":
            self._simple(c, 500, b"500: internal server error (low heap)\r\n")
        elif name == "reset":
            fmt = "HH" if sys.platform == "win32" else "ii"
            c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack(fmt, 1, 0))
            c.close()
        elif name == "hang":
            time.sleep(3.0)
        elif name == "big":
            self._note_get("/t/big", *self._stream(c, 200, b"b" * self.big_body, "application/octet-stream"))
        else:
            self._simple(c, 404, b"FileNotFound\r\n")

    # -- upload: a model of the multipart parser and the file handle -------------
    def _upload(self, c, headers, rest):
        cl = int(headers.get("content-length", "0"))
        boundary = headers.get("content-type", "").split("boundary=", 1)[-1].encode("latin-1")
        tail_len = len(b"\r\n--" + boundary + b"--\r\n")
        body = bytearray(rest)
        c.settimeout(self.rx_timeout)

        def more():
            try:
                d = c.recv(65536)
            except socket.timeout:
                return "rx_timeout"
            except OSError:
                return "rst"
            if not d:
                return "fin"
            body.extend(d)
            return None

        exit_ = None
        while body.find(b"\r\n\r\n") < 0:
            exit_ = more()
            if exit_:
                with self.lock:
                    self.upload_exits.append(exit_)
                return
        hend = body.find(b"\r\n\r\n") + 4
        fname = bytes(body[:hend]).decode("latin-1").split('filename="', 1)[1].split('"', 1)[0]
        fpath = "/" + fname
        content_len = cl - hend - tail_len
        handle = self._upload_start(fpath)
        delivered = 0
        while True:
            # The parser hands over what each receive brought: it flushes at the
            # last byte of every receive callback (WebRequest.cpp:195, :583).
            avail = min(len(body) - hend, content_len)
            if avail > delivered:
                handle.extend(body[hend + delivered:hend + avail])
                delivered = avail
            if len(body) >= cl:
                self._upload_end(fpath, handle, "complete")
                if self.upload_fail:
                    c.sendall(self._head(507, None, 0))
                else:
                    c.sendall(self._head(303, None, 0, extra={"Location": "FSexplorer.html"}))
                return
            exit_ = more()
            if exit_:
                self._upload_end(fpath, handle, exit_)
                return

    def _upload_start(self, fpath):
        with self.lock:
            self.files.setdefault(fpath, b"")        # a new file is visible at once, empty
            if self.upload_model == "leak_until_next_upload" and self.leaked is not None:
                # alpha.359: open() runs first, then assigning to the static File
                # closes the old handle, and that close publishes its data.
                old_path, old_data = self.leaked
                self.files[old_path] = bytes(old_data)
                self.leaked = None
        return bytearray()

    def _upload_end(self, fpath, handle, exit_):
        with self.lock:
            self.upload_exits.append(exit_)
            if self.upload_fail:
                return                                   # the open or a write failed: nothing stored
            if exit_ == "rst" and self.rst_drops_queue:
                return                                   # the RST removed the data: never opened
            if exit_ == "complete" or self.upload_model == "close_on_abort":
                self.files[fpath] = bytes(handle)        # close publishes the data
            else:
                self.leaked = (fpath, handle)            # left open: nothing published

    # -- /ws ---------------------------------------------------------------------
    def _ws(self, c, headers):
        key = headers.get("sec-websocket-key", "")
        acc = base64.b64encode(hashlib.sha1((key + WS_GUID).encode()).digest()).decode()
        c.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   "Sec-WebSocket-Accept: %s\r\n\r\n" % acc).encode())
        with self.lock:
            self.ws_active += 1
            self.ws_opens += 1
            over = self.ws_active > self.ws_cap
        try:
            if over:
                c.sendall(b"\x88\x00")           # accepted, then closed, like the device's cap
                time.sleep(0.05)
                return
            n = 0
            while not self._stop.is_set():
                r, _, _ = select.select([c], [], [], 0.1)
                if r:
                    d = c.recv(4096)
                    if not d or (d[0] & 0x0F) == 0x8:
                        return
                payload = b"stub ot line %d" % n
                n += 1
                c.sendall(bytes([0x81, len(payload)]) + payload)
        except OSError:
            return
        finally:
            with self.lock:
                self.ws_active -= 1


# ---------------------------------------------------------------------------
def run_tool(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = rs.main(argv)
    return rc, out.getvalue()


def read_log(path):
    with open(path, encoding="utf-8") as fh:
        return [json.loads(line) for line in fh if line.strip()]


def wait_for(pred, timeout=5.0):
    t_end = time.monotonic() + timeout
    while time.monotonic() < t_end:
        if pred():
            return True
        time.sleep(0.05)
    return pred()


class StubCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="t1124_")
        self.log = os.path.join(self.tmp, "run.ndjson")
        self._stubs = []

    def tearDown(self):
        for s in self._stubs:
            s.close()
        shutil.rmtree(self.tmp, ignore_errors=True)

    def stub(self, **kw):
        s = StubDevice(**kw)
        self._stubs.append(s)
        return s

    def opts(self, stub, *extra):
        return rs.parse_opts(["--host", stub.hostport, "--ws-subs", "0", "--log", self.log] + list(extra))


# ---------------------------------------------------------------------------
class TestPureFunctions(unittest.TestCase):
    def test_pattern_names_its_upload_on_every_line(self):
        p = rs.pattern(7, 16384)
        self.assertEqual(len(p), 16384)
        self.assertEqual(p, rs.pattern(7, 16384))
        lines = p.split(b"\n")[:-1]
        self.assertTrue(all(ln.startswith(b"t1124 iter=0007 off=") for ln in lines))
        self.assertNotEqual(rs.pattern(6, 64), rs.pattern(7, 64))

    def test_judge_readback(self):
        size = 16384
        cur, prev = rs.pattern(5, size), rs.pattern(4, size)
        self.assertEqual(rs.judge_readback(cur[:2920], 5, size), ("prefix_current", 5))
        self.assertEqual(rs.judge_readback(cur, 5, size), ("complete_current", 5))
        self.assertEqual(rs.judge_readback(prev[:2920], 5, size), ("stale_previous", 4))
        self.assertEqual(rs.judge_readback(rs.pattern(0, size), 5, size), ("stale_previous", 0))
        corrupt = bytearray(cur[:2920])
        corrupt[2000] ^= 0xFF
        self.assertEqual(rs.judge_readback(bytes(corrupt), 5, size)[0], "foreign")
        self.assertEqual(rs.judge_readback(b"", 5, size), ("empty", None))
        self.assertEqual(rs.judge_readback(b"t1124", 5, size)[0], "ambiguous")
        self.assertEqual(rs.judge_readback(None, 5, size), ("unreadable", None))
        self.assertEqual(rs.judge_readback(rs.pattern(6, size)[:1460], 5, size)[0], "foreign")

    def test_dechunker_byte_by_byte(self):
        wire = b"5\r\nhello\r\n6;ext=1\r\n world\r\n0\r\n\r\n"
        d = rs.Dechunker()
        out = b"".join(d.feed(wire[i:i + 1]) for i in range(len(wire)))
        self.assertEqual(out, b"hello world")
        self.assertTrue(d.done)
        cut = rs.Dechunker()
        cut.feed(wire[:12])
        self.assertFalse(cut.done)

    def test_parse_uptime(self):
        self.assertEqual(rs.parse_uptime("0(d)-01:23(H:m)"), 83)
        self.assertEqual(rs.parse_uptime("2(d)-00:00(H:m)"), 2880)
        self.assertIsNone(rs.parse_uptime("garbage"))

    def test_compare_snaps(self):
        a = {"bootcount": 4, "uptime_min": 100, "freeheap": 90000, "maxfreeblock": 40000,
             "hd_tcp_active_pcbs": 3, "hd_rest_503": 0, "hd_webfile_503": 0, "lastreset": "x"}
        same = dict(a, uptime_min=101, freeheap=89000)
        self.assertEqual(rs.compare_snaps(a, same, 4096)["reboot"], "ok")
        self.assertEqual(rs.compare_snaps(a, dict(same, bootcount=5), 4096)["reboot"], "FAIL")
        self.assertEqual(rs.compare_snaps(a, dict(same, uptime_min=2), 4096)["reboot"], "FAIL")
        self.assertEqual(rs.compare_snaps(a, dict(same, freeheap=80000), 4096)["heap"], "WARN")
        self.assertEqual(rs.compare_snaps(a, dict(same, hd_tcp_active_pcbs=5), 4096)["pcbs"], "WARN")
        self.assertEqual(rs.compare_snaps(a, dict(same, hd_tcp_active_pcbs=4), 4096)["pcbs"], "ok")
        self.assertEqual(rs.compare_snaps(a, None, 4096)["reboot"], "unknown")


class TestClassifyOnTheWire(StubCase):
    CASES = [
        ("/t/ok-cl", "ok"), ("/t/ok-chunked", "ok"), ("/t/ok-close", "ok"),
        ("/t/short", "short"), ("/t/chunk-cut", "short"), ("/t/304", "not_modified"),
        ("/t/429", "429"), ("/t/503-busy", "503_no_retry_after"),
        ("/t/503-lowheap", "503_no_retry_after"), ("/t/503-gate", "503_retry_after"),
        ("/t/500", "server_error"), ("/t/nope", "client_error"), ("/t/reset", "reset"),
    ]

    def test_classes(self):
        s = self.stub()
        for path, want in self.CASES:
            with self.subTest(path=path):
                r = rs.http_exchange(s.hostport, "GET", path, keep_body=True, read_timeout=3.0)
                self.assertEqual(rs.classify(r), want, rs.slim(r))
        r = rs.http_exchange(s.hostport, "GET", "/t/ok-close")
        self.assertTrue(r.get("close_delimited"))
        r = rs.http_exchange(s.hostport, "GET", "/t/ok-chunked", keep_body=True)
        self.assertEqual((r["chunked"], r["body"]), (True, b"k" * 3000))
        reasons = {p: rs.reason_of(rs.http_exchange(s.hostport, "GET", p))
                   for p in ("/t/503-busy", "/t/503-lowheap", "/t/503-gate")}
        self.assertEqual(reasons, {"/t/503-busy": "rest_busy", "/t/503-lowheap": "low_heap",
                                   "/t/503-gate": "empty_body"})
        self.assertEqual(rs.http_exchange(s.hostport, "GET", "/t/503-gate")["retry_after"], "1")

    def test_timeout_and_refused(self):
        s = self.stub()
        r = rs.http_exchange(s.hostport, "GET", "/t/hang", read_timeout=1.0)
        self.assertEqual(rs.classify(r), "timeout")
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()
        r = rs.http_exchange("127.0.0.1:%d" % port, "GET", "/", connect_timeout=5.0)
        self.assertEqual(rs.classify(r), "refused")


class TestProbeRecord(unittest.TestCase):
    def test_probe_record_survives_a_failed_connect(self):
        """A refused or timed-out connect returns before total_ms is set. The probe
        read r["total_ms"] and its thread died mid-arm (2026-10-02 bench storm)."""
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()
        r = rs.http_exchange("127.0.0.1:%d" % port, "GET", "/api/v2/device/info", connect_timeout=5.0)
        self.assertNotIn("total_ms", r)
        p = rs.probe_record("arm4_w8", time.monotonic(), r, rs.classify(r))
        self.assertEqual(p["class"], "refused")
        self.assertIsNone(p["status"])
        self.assertIsNone(p["total_ms"])

    def test_probe_loop_keeps_probing_after_a_failed_connect(self):
        """Drives the real probe_loop against a closed port: it must log a probe line for
        every failed probe and keep going. The old loop raised KeyError('total_ms') on the
        first failed probe and its thread ended, so the rest of the arm had no probes."""
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()
        opts = types.SimpleNamespace(host="127.0.0.1:%d" % port, probe_interval=0.05,
                                     connect_timeout=5.0, read_timeout=5.0, pcb_pool=16)
        written, failure, stop = [], [], threading.Event()
        log = types.SimpleNamespace(write=written.append)
        st = types.SimpleNamespace(add_probe=lambda p: None, counts=lambda: {})

        def run():
            try:
                rs.probe_loop(opts, log, "arm4_w8", stop, st, time.monotonic())
            except Exception as e:
                failure.append(e)

        t = threading.Thread(target=run, daemon=True)
        with contextlib.redirect_stdout(io.StringIO()):
            t.start()
            deadline = time.monotonic() + 30
            while len(written) < 2 and t.is_alive() and time.monotonic() < deadline:
                time.sleep(0.05)
            stop.set()
            t.join(15)
        self.assertEqual(failure, [])
        self.assertGreaterEqual(len(written), 2)
        self.assertEqual({p["class"] for p in written}, {"refused"})
        self.assertTrue(all(p["total_ms"] is None for p in written))


class TestAbortOnTheWire(StubCase):
    """The stub must see a different exit for each mode, or the modes are untested."""

    def _abort(self, s, mode, **kw):
        n0 = len(s.get_exits)
        r = rs.http_exchange(s.hostport, "GET", "/t/big",
                             abort={"mode": mode, "after": 1024, "stall_s": kw.get("stall_s", 1.5)},
                             rcvbuf=kw.get("rcvbuf"))
        self.assertEqual(r["aborted"], mode)
        self.assertEqual(rs.classify(r), "aborted_" + mode)
        self.assertTrue(wait_for(lambda: len(s.get_exits) > n0))
        return s.get_exits[-1]

    def test_fin(self):
        s = self.stub()
        self.assertEqual(self._abort(s, "fin")["exit"], "fin")

    def test_rst(self):
        s = self.stub()
        self.assertEqual(self._abort(s, "rst")["exit"], "rst")

    def test_stall_closes_the_window(self):
        s = self.stub()
        e = self._abort(s, "stall", stall_s=1.5, rcvbuf=rs.STALL_RCVBUF)
        self.assertEqual(e["exit"], "fin")
        self.assertGreater(e["max_gap_s"], 1.0, "the stub never had to wait: the stall did not reach it")

    def test_upload_exits(self):
        s = self.stub(rx_timeout=1.0)
        o = self.opts(s, "--stall-seconds", "4", "--pre-abort-delay", "0.3")
        content = rs.pattern(1, 16384)
        rec = {m: rs.upload(o, "wire.bin", content, abort_mode=m, send_bytes=4096) for m in ("fin", "rst", "stall")}
        self.assertTrue(wait_for(lambda: len(s.upload_exits) == 3))
        self.assertEqual(s.upload_exits, ["fin", "rst", "rx_timeout"])
        ev = rec["stall"]["server_event"]
        self.assertEqual(ev["kind"], "fin")
        self.assertLess(ev["after_s"], 3.0)
        self.assertNotIn("server_early", rec["fin"])
        self.assertEqual([rec[m]["sent_content"] for m in ("fin", "rst", "stall")], [4096] * 3)
        self.assertEqual(s.files["/wire.bin"], content[:4096])   # every byte sent reached the handle


class TestUploadAbort(StubCase):
    def run_ua(self, stub, mode, n, *extra):
        rc, out = run_tool(["--host", stub.hostport, "--upload-abort", str(n), "--abort-mode", mode,
                            "--log", self.log, "--pre-abort-delay", "0.3", "--readback-wait", "0.2",
                            "--settle-wait", "0.2", "--stall-seconds", "4"] + list(extra))
        recs = read_log(self.log)
        rbs = {r["iter"]: r for r in recs if r["type"] == "readback"}
        summ = [r for r in recs if r["type"] == "summary"][-1]
        return rc, out, rbs, summ, recs

    def test_close_on_abort_passes(self):
        for mode in ("fin", "rst", "stall"):
            with self.subTest(mode=mode):
                s = self.stub(rx_timeout=1.0)
                rc, out, rbs, summ, recs = self.run_ua(s, mode, 2)
                self.assertEqual(rc, 0, out)
                self.assertEqual(summ["verdict"], "PASS")
                self.assertEqual(rbs[0]["class"], "complete_current")
                for i in (1, 2):
                    self.assertEqual((rbs[i]["class"], rbs[i]["stored_len"]), ("prefix_current", 4096))
                self.assertEqual(rbs[3]["class"], "complete_current")
                want = {"fin": "fin", "rst": "rst", "stall": "rx_timeout"}[mode]
                self.assertEqual(s.upload_exits, ["complete", want, want, "complete"])
                clean = [r for r in recs if r["type"] == "cleanup"][0]
                self.assertEqual((clean["delete_status"], clean["after_status"], clean["gone"]), (200, 404, True))
                self.assertEqual(s.files, {})

    def test_mixed_rotates_modes(self):
        s = self.stub(rx_timeout=1.0)
        rc, out, rbs, summ, recs = self.run_ua(s, "mixed", 3)
        self.assertEqual(rc, 0, out)
        self.assertEqual(s.upload_exits, ["complete", "fin", "rst", "rx_timeout", "complete"])

    def test_leaked_handle_fails(self):
        s = self.stub(upload_model="leak_until_next_upload")
        rc, out, rbs, summ, recs = self.run_ua(s, "fin", 3)
        self.assertEqual(rc, 1, out)
        self.assertEqual(summ["verdict"], "FAIL")
        for i in (1, 2, 3):
            self.assertEqual((rbs[i]["class"], rbs[i]["source_iter"]), ("stale_previous", i - 1))
        self.assertEqual(rbs[4]["class"], "complete_current")   # the upload path itself still works

    def test_leaked_handle_fails_in_a_mixed_run(self):
        # fin and stall agree with rst, so the stale readbacks are a leak
        s = self.stub(upload_model="leak_until_next_upload", rx_timeout=1.0)
        rc, out, rbs, summ, recs = self.run_ua(s, "mixed", 3)
        self.assertEqual(rc, 1, out)
        self.assertEqual(summ["stale_by_mode"], {"fin": 1, "rst": 1, "stall": 1})

    def test_rst_that_drops_the_queue_is_not_a_leak(self):
        # An RST removes the queued data before the handler opens the file
        # (AsyncTCP.cpp:494-500). The file keeps the control upload, as with a leak.
        s = self.stub(rst_drops_queue=True)
        rc, out, rbs, summ, recs = self.run_ua(s, "rst", 2)
        self.assertEqual(rc, 2, out)
        self.assertEqual(summ["verdict"], "INCONCLUSIVE")
        self.assertEqual([(rbs[i]["class"], rbs[i]["source_iter"]) for i in (1, 2)], [("stale_previous", 0)] * 2)
        self.assertTrue(any("rst alone cannot tell" in x for x in summ["inconclusive"]), summ["inconclusive"])

    def test_rst_stale_next_to_clean_fin_and_stall_is_not_a_fail(self):
        s = self.stub(rx_timeout=1.0, rst_drops_queue=True)
        rc, out, rbs, summ, recs = self.run_ua(s, "mixed", 3)
        self.assertEqual(rc, 2, out)
        self.assertEqual([rbs[i]["class"] for i in (1, 2, 3)], ["prefix_current", "stale_previous", "prefix_current"])

    def test_leak_in_an_rst_only_run_is_inconclusive(self):
        # the documented limit: rst alone cannot tell a leak from a dropped queue
        s = self.stub(upload_model="leak_until_next_upload")
        rc, out, rbs, summ, recs = self.run_ua(s, "rst", 2)
        self.assertEqual(rc, 2, out)
        self.assertEqual(summ["stale_by_mode"], {"rst": 2})

    def test_failed_control_stops_the_run(self):
        # A 507 (full filesystem) next to a leftover of a killed run must not become a FAIL.
        s = self.stub(upload_fail=True)
        s.files["/t1124_abort.bin"] = rs.pattern(5, 16384)[:3000]
        rc, out, rbs, summ, recs = self.run_ua(s, "fin", 3)
        self.assertEqual(rc, 2, out)
        self.assertEqual(summ["verdict"], "INCONCLUSIVE")
        self.assertEqual((sorted(rbs), rbs[0]["class"], summ["aborts_run"]), ([0], "foreign", 0))
        self.assertEqual(s.upload_exits, ["complete"])
        self.assertEqual(s.files, {})                    # cleanup still ran

    def test_password_stops_the_run(self):
        s = self.stub(auth=True)
        rc, out, rbs, summ, recs = self.run_ua(s, "fin", 2)
        self.assertEqual(rc, 2, out)
        self.assertEqual(summ["verdict"], "INCONCLUSIVE")
        self.assertEqual(summ["aborts_run"], 0)

    def test_short_200_is_retried_not_judged(self):
        s = self.stub()
        s.truncate_next = 1                  # the control readback's first attempt is cut in half
        rc, out, rbs, summ, recs = self.run_ua(s, "fin", 1)
        self.assertEqual(rc, 0, out)
        hist = rbs[0]["history"]
        self.assertEqual(hist[0]["class"], "short")
        self.assertNotIn("judge", hist[0])
        self.assertEqual(rbs[0]["class"], "complete_current")

    def test_persistent_short_200_is_inconclusive(self):
        s = self.stub()
        s.truncate_always = True
        rc, out, rbs, summ, recs = self.run_ua(s, "fin", 2)
        self.assertEqual(rc, 2, out)
        self.assertEqual(summ["verdict"], "INCONCLUSIVE")
        self.assertTrue(all(r["class"] == "unreadable" for r in rbs.values()))

    def test_help_page_is_not_a_readback(self):
        s = self.stub(helper_page=True)
        rc, out, rbs, summ, recs = self.run_ua(s, "fin", 1)
        self.assertEqual(rc, 2, out)
        self.assertEqual(rbs[0]["class"], "helper_page")


class TestGateBalance(StubCase):
    def gb(self, stub):
        log = rs.NdjsonLog(self.log)
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                return rs.gate_balance(self.opts(stub, "--gate-reps", "3"), log, "t")
        finally:
            log.close()

    def test_clean(self):
        r = self.gb(self.stub())
        self.assertEqual((r["sequential_ok"], r["pair_result"], r["verdict"]), (True, "pass", "PASS"))

    def test_one_leaked_slot_only_the_pair_sees(self):
        r = self.gb(self.stub(leaked_rest_slots=1))
        self.assertTrue(r["sequential_ok"])
        self.assertEqual((r["pair_result"], r["verdict"]), ("fail", "FAIL"))
        attempts = r["pairs"]["/api/v2/settings"]
        self.assertTrue(all(a["result"] == "fail" and a["b_reason"] == "rest_busy" for a in attempts))
        self.assertEqual([a["result"] for a in r["pairs"]["/"]], ["pass"])   # the file gate is clean

    def test_two_leaked_slots_fail_sequential(self):
        r = self.gb(self.stub(leaked_rest_slots=2))
        self.assertFalse(r["sequential_ok"])
        self.assertEqual(r["verdict"], "FAIL")

    def test_cap1_region_skips_the_pair(self):
        r = self.gb(self.stub(leaked_rest_slots=1, maxfreeblock=12000))
        self.assertEqual((r["pair_result"], r["verdict"]), ("skipped_cap1", "WARN"))

    def test_near_threshold_is_inconclusive(self):
        # below the 32000 margin A's own buffers could have pushed the cap to 1
        for mb in (20000, 31000):
            with self.subTest(maxfreeblock=mb):
                r = self.gb(self.stub(leaked_rest_slots=1, maxfreeblock=mb))
                self.assertEqual((r["pair_result"], r["verdict"]), ("inconclusive_near_threshold", "WARN"))

    def test_small_response_cannot_prove_overlap(self):
        # A's whole response fits in its window, so the device may release A's
        # slot before B arrives. That pair must not count as a pass.
        r = self.gb(self.stub(big_body=200))
        self.assertEqual((r["pair_result"], r["verdict"]), ("inconclusive", "WARN"))
        self.assertTrue(all(a["result"] == "not_overlapping" for a in r["pairs"]["/api/v2/settings"]))

    def test_big_window_cannot_prove_overlap(self):
        # The same guard at a realistic size: if the OS gave A a large window
        # anyway, A's 60 KB response lands in full before B and must be seen as
        # complete (the peek has to return everything buffered, not one segment).
        saved = rs.PAIR_RCVBUF
        rs.PAIR_RCVBUF = 1 << 20
        try:
            r = self.gb(self.stub(big_body=60000))
        finally:
            rs.PAIR_RCVBUF = saved
        for p in ("/api/v2/settings", "/"):
            self.assertTrue(all(a["result"] == "not_overlapping" for a in r["pairs"][p]), r["pairs"][p])
        self.assertEqual((r["pair_result"], r["verdict"]), ("inconclusive", "WARN"))


class TestStorm(StubCase):
    def storm(self, stub, *extra):
        rc, out = run_tool(["--host", stub.hostport, "--workers", "2", "--duration", "2",
                            "--abort-ratio", "0.5", "--quiet", "1", "--probe-interval", "0.5",
                            "--gate-reps", "2", "--stall-seconds", "1", "--log", self.log] + list(extra))
        recs = read_log(self.log)
        return rc, out, recs

    def test_clean_storm_passes_and_classifies(self):
        s = self.stub(big_body=20000)
        rc, out, recs = self.storm(s, "--ws-subs", "0")
        self.assertEqual(rc, 0, out)
        reqs = [r for r in recs if r["type"] == "request"]
        classes = {r["class"] for r in reqs}
        self.assertIn("ok", classes)
        self.assertTrue(classes & {"aborted_fin", "aborted_rst"}, classes)
        self.assertTrue(all(r["class"] for r in reqs))
        arm = [r for r in recs if r["type"] == "arm_summary"][0]
        self.assertEqual(arm["verdict"], "PASS")
        self.assertEqual(arm["gate_balance"]["pair_result"], "pass")
        self.assertIsNotNone(arm["first_200_after_stop_ms"])
        self.assertGreater(arm["probes"], 0)
        start = [r for r in recs if r["type"] == "arm_start"][0]
        self.assertEqual(start["offered_max_connections"], 3)
        self.assertIn("offered: 2 storm + 1 probe", out)

    def test_leaking_route_is_caught(self):
        # TASK-1172 old side: on alpha.224 to alpha.378 every 200 from the labels
        # route kept its REST slot
        s = self.stub(big_body=20000, leak_route="/api/v2/sensors/labels")
        rc, out, recs = self.storm(s, "--ws-subs", "0", "--paths", "/api/v2/sensors/labels,/")
        self.assertEqual(rc, 1, out)
        self.assertIn("WARNING: /api/v2/sensors/labels", out)
        summ = [r for r in recs if r["type"] == "summary"][-1]
        self.assertEqual(summ["verdict"], "FAIL")

    def test_fixed_route_passes(self):
        # TASK-1172 fix side (alpha.379 or newer): the same storm, the slot comes back
        s = self.stub(big_body=20000)
        rc, out, recs = self.storm(s, "--ws-subs", "0", "--paths", "/api/v2/sensors/labels,/")
        self.assertEqual(rc, 0, out)
        self.assertIn("WARNING: /api/v2/sensors/labels", out)

    def test_password_is_inconclusive_before_any_load(self):
        # every gate-balance GET would get 401 and read as a leak
        s = self.stub(big_body=20000, auth=True)
        rc, out, recs = self.storm(s, "--ws-subs", "0")
        self.assertEqual(rc, 2, out)
        summ = [r for r in recs if r["type"] == "summary"][-1]
        self.assertEqual(summ["verdict"], "INCONCLUSIVE")
        self.assertEqual([r for r in recs if r["type"] in ("arm_start", "request", "gate_balance")], [])

    def test_reboot_is_caught(self):
        s = self.stub(big_body=20000)
        t = threading.Timer(1.0, lambda: setattr(s, "bootcount", 8))
        t.start()
        try:
            rc, out, recs = self.storm(s, "--ws-subs", "0")
        finally:
            t.cancel()
        self.assertEqual(rc, 1, out)
        summ = [r for r in recs if r["type"] == "summary"][-1]
        self.assertTrue(any("reboot" in f for f in summ["fail"]), summ["fail"])

    @unittest.skipUnless(HAVE_WS, "websocket-client not installed")
    def test_ws_leg(self):
        s = self.stub(big_body=20000)
        rc, out, recs = self.storm(s, "--ws-subs", "1")
        self.assertEqual(rc, 0, out)
        arm = [r for r in recs if r["type"] == "arm_summary"][0]
        self.assertGreater(arm["ws"]["ws_frames"], 0)
        self.assertEqual(arm["ws"]["ws_errors"], 0)

    @unittest.skipUnless(HAVE_WS, "websocket-client not installed")
    def test_ws_above_the_cap_flaps(self):
        s = self.stub(big_body=20000, ws_cap=1)
        rc, out, recs = self.storm(s, "--ws-subs", "2")
        summ = [r for r in recs if r["type"] == "summary"][-1]
        self.assertGreater(summ["ws_final"]["ws_reconnects"], 1)
        self.assertGreater(s.ws_opens, 3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
