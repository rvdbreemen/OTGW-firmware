#!/usr/bin/env python3
"""Request-storm and upload-abort tool for the 2.0.0 web stack (TASK-1124).

Two modes. Each run does one of them.

  --upload-abort N   AC#1. Does the async upload path close its file handle when
                     the client leaves mid-body? N uploads are cut off with a FIN,
                     an RST or a stall, and after each one the file is read back.
  (no flag)          AC#2 and AC#4. A rapid-refresh storm in arms of W workers,
                     next to persistent /ws subscribers. Every request outcome is
                     classified, device/info is snapshotted around each arm, and
                     a gate-balance check runs after each arm and at the end.

HTTP uses raw sockets (stdlib only), so the tool decides exactly how a
connection ends. The /ws leg reuses ws_subscriber() from test_ws_liveload.py and
needs websocket-client (pip install websocket-client). Use --ws-subs 0 without it.

Exit code: 0 PASS, 1 FAIL, 2 INCONCLUSIVE (also for usage errors).
Output: progress on stdout, plus one NDJSON record per event in --log (default
%LOCALAPPDATA%/OTGW-capture/refresh-storm/, else the system temp dir).

Usage:
  python scripts/tests/refresh_storm.py --host 192.168.88.61 --upload-abort 20 --abort-mode fin
  python scripts/tests/refresh_storm.py --host 192.168.88.61 --upload-abort 20 --abort-mode stall
  python scripts/tests/refresh_storm.py --host 192.168.88.61 --workers 2,4,6,8 --duration 45

Preconditions: no HTTP password on the device, and no other tool loading it at
the same time. With a password, / and /api/v2/settings answer 401
(FSexplorer.ino:155, restAPI.ino:387), and so does an upload (FSexplorer.ino:315).
Both modes then stop before any abort or storm load, and the 401 counts as
INCONCLUSIVE.

What the firmware does, checked against the code at dev b2c418e43 (alpha.382).
Other revisions have other line numbers. Library files are ESPAsyncWebServer
3.11.0 and AsyncTCP 3.4.10, pinned at platformio.ini:211-212. sdkconfig.h is the
esp32s3 qio_qspi one in framework-arduinoespressif32-libs.

Upload abort (AC#1)
- The UI uploads with POST /upload?path=<dir>, multipart field "upload"
  (data/FSexplorer.html:88, :384-385). The route answers 303 to FSexplorer.html
  on success and 507 when the open or a write failed (FSexplorer.ino:306-326).
- The handle is per request. The handler's first call (index 0) runs
  request->_tempFile = LittleFS.open(name, "w") (FSexplorer.ino:665, :695).
  ~AsyncWebServerRequest closes it (WebRequest.cpp:117-119) when the connection
  goes.
- The multipart parser buffers file bytes. It calls the handler when 1460 bytes
  are buffered, or at the last byte of each receive callback (WebRequest.cpp:195,
  :580-590). AsyncTCP makes one callback per received pbuf (AsyncTCP.cpp:1056-1068),
  and the device's MSS is 1436 (CONFIG_LWIP_TCP_MSS in sdkconfig.h). So the
  handler runs about once per TCP segment. When the handle is closed at the
  abort, the file keeps every byte the device processed before the connection
  ended. For a FIN or a stall that should be every byte sent. This is read from
  the code, not measured: the stored_len of each readback shows it.
- A FIN is queued behind the data in the same FIFO (AsyncTCP.cpp:454-476), so the
  data are written before AsyncTCP closes the connection (AsyncTCP.cpp:1043-1046).
- An RST removes the client's queued events (AsyncTCP.cpp:494-500), and data
  still in the queue are never written. If that includes the first segment, the
  handler never runs, and the file keeps an earlier upload: a stale_previous
  readback without any leak. --pre-abort-delay (0.5 s) gives the device time to
  process the data first. "missing" cannot follow a good control upload, and
  "empty" needs a failed write.
- A stall ends at the 3 s RX timeout set per connection (WebServer.cpp:49),
  checked on the AsyncTCP poll (AsyncTCP.cpp:1106-1109).
- Every line of the uploaded content names its upload ("t1124 iter=0007 off=..."),
  so a readback shows which upload it came from. A handle closed at the abort
  shows a prefix of this upload (prefix_current). A handle left open shows what
  an earlier upload committed (stale_previous). This relies on LittleFS publishing
  written data only on close or sync, which this tool does not prove.
- A stale_previous from a fin or stall iteration is a FAIL. One from an rst
  iteration counts as a FAIL only when a fin or stall iteration of the same run
  is stale_previous too; otherwise it counts as INCONCLUSIVE. So an rst-only run
  never FAILs on a stale_previous readback. To tell a leak from a dropped queue,
  rerun with --abort-mode fin, or with a longer --pre-abort-delay: a leak stays
  stale, a dropped queue does not.
- The control upload at the start proves the readback path works. If it fails,
  no abort runs, and the failed control counts as INCONCLUSIVE.
- Readback is GET /<name>: the onNotFound catch-all, handleFile() and webSendFile()
  (FSexplorer.ino:341-361, :583-617). It shares the file gate, which refuses with
  503 and Retry-After: 1 (webServerCompat.h:346-356). If /FSexplorer.html is
  missing, the catch-all serves the upload help page as a 200 text/html for any
  path (FSexplorer.ino:593). So the tool requires text/plain (a .bin file,
  FSexplorer.ino:751), checks the body against Content-Length (TASK-1162: the
  bench served a 5752-byte file as a 4172-byte 200) and retries.
- Cleanup is GET /api/listfiles?delete=<path>, which answers 200 "File deleted" or
  404 "File not found" (FSexplorer.ino:301, :478-502). A final GET must then give
  404. GET /?delete=<path> deletes nothing: "/" goes to sendIndex() (:194), or to
  sendFSexplorerFallback() when /index.html is missing (:184-185), and neither
  reads the argument. The only other reader is the catch-all (:585-592): any
  unrouted path with ?delete= removes the file and answers 303 whether or not it
  existed.

Storm (AC#2, AC#4)
- The device answers every request with Connection: close (WebResponses.cpp:270,
  :340), so every request is one TCP connection.
- REST gate: at most REST_MAX_INFLIGHT (2) requests in processAPI, 1 while
  maxfreeblock < 16000 (restAPI.ino:43-64). A refusal is a JSON 503 "Server busy"
  without Retry-After (restAPI.ino:2724-2730). A request holds one disconnect
  callback (WebRequest.cpp:277-279). webArmSlotRelease() arms it for every gate
  slot the request holds (webServerCompat.h:326-335), after processAPI takes its
  slot (restAPI.ino:2731-2734) and after webSendFile() takes one
  (webServerCompat.h:358-359).
- File gate: at most 2 static-file serves, 1 while maxfreeblock < 16000
  (restAPI.ino:81-100). A refusal is a 503 with an empty body and Retry-After: 1
  (webServerCompat.h:346-356).
- device/info answers 503 "low heap" while maxfreeblock < 8192 (restAPI.ino:3026,
  :3074-3077). /otgw/otmonitor, /otgw/telegraf and /device/time answer 429 with
  Retry-After when polled too fast (restAPI.ino:2585-2589, :2620-2648).
- /api/v2/settings, /device/info, /sat/status and /debug stream chunked
  (restSendChunked, jsonChunked.h:115; restAPI.ino:3124, :3718, :2167;
  SATcontrol.ino:2062).
- The probe uses device/info, never /api/v2/health: health writes /.health to
  LittleFS on every call (restAPI.ino:3344, helperStuff.ino:196-205).
- device/info emits its keys inside a "device" object (restAPI.ino:3126), from
  fwversion (:3130) to hd_ws_drops (:3297). uptime has minute resolution
  ("%d(d)-%02d:%02d(H:m)", helperStuff.ino:653-664).
  hd_tcp_active_pcbs is the length of lwIP's tcp_active_pcbs list
  (src/libraries/Platform/src/platform_esp32.h:281-289), sampled by
  sampleHeapWatermark() (helperStuff.ino:433) from doTaskEvery1s()
  (OTGW-firmware.ino:684-688), which the loop runs once a second. The pool is
  CONFIG_LWIP_MAX_ACTIVE_TCP = 16 (sdkconfig.h). It is shared with MQTT, telnet
  and any browser.
- /ws takes at most 3 clients. A 4th is accepted, then closed
  (webSocketStuff.ino:60, :163-168). ws_subscriber() reconnects 0.5 s later, so a
  subscriber above the cap flaps at about 2 Hz and adds connection churn.
- GET /api/v2/sensors/labels and /api/v2/sat/markers stream a file through
  webSendFile() (restAPI.ino:4258, :1767). From alpha.224 to alpha.378
  webSendFile() armed its own release over processAPI's, so every 200 from these
  routes leaked a REST slot until reboot (TASK-1172, fixed in alpha.379 by
  d57d9e2d9). They are not in the default mix. With --paths
  /api/v2/sensors/labels,/ the storm is TASK-1172's old-vs-fix: FAIL on
  alpha.224 to alpha.378, PASS on alpha.379 or newer. The route streams only
  while /dallas_labels.ini exists; without it the answer is "{}"
  (restAPI.ino:4251-4254). Reboot an old build afterwards. On alpha.379 or newer
  the gate-balance check has no built-in positive control: an old build and the
  self-test stub are what remain.

Offered concurrency during an arm: W storm workers (one connection each at a
time) + 1 device/info probe every --probe-interval seconds + S persistent /ws
subscribers. The tool prints this line per arm. The default arm (--workers 2)
offers up to 3 HTTP connections, which can exceed a gate's cap of 2: that is the
point of a storm. For an arm within the cap, use --workers 1 or
--probe-interval 0. After an arm the tool runs sequentially: a time-to-first-200
poll, --quiet seconds of rest, then the gate-balance check. That check uses 1
connection, and 2 for its overlapping pair. Snapshots, readbacks and cleanup are
sequential too, so upload-abort mode never has more than 1 connection open.

Gate balance: after --quiet seconds, --gate-reps sequential GETs each of
/api/v2/settings and / must all return 200. A sequential GET fails only once the
leaked slots reach the cap. At cap 2 one leaked slot still passes, so an
overlapping pair follows. Connection A reads with a 1 KB receive window and only
peeks, so the device cannot finish its response and A keeps its slot. B is then
fetched in full. The pair counts only if A is still incomplete after B. Below
maxfreeblock 16000 the cap is 1 and the pair is skipped. A refused B counts as a
leak only at maxfreeblock >= 32000; below that it is inconclusive. Both gates run
at cap 2 everywhere from 16000 up (restAPI.ino:55-64, :85-89), so 32000 is a
margin, not a device threshold. The margin is an estimate, not a bench result.
A's queued response (up to TCP_SND_BUF 5744 B, sdkconfig.h), its request and
response objects, and B's request object, which exists before the gate runs
(WebServer.cpp:50), could take about 12 KB from the largest block and push the
cap to 1. Calibrate on a clean build (alpha.379 or newer, fresh boot): the pair
must pass there. A failed GET or pair is retried: a leaked slot fails every
retry, a request from another client (a browser, Home Assistant) does not.
"""
import argparse
import json
import math
import os
import random
import re
import select
import socket
import struct
import sys
import tempfile
import threading
import time
from collections import Counter, defaultdict
from urllib.parse import quote

HERE = os.path.dirname(os.path.abspath(__file__))

MULTIPART_PIECE = 1460          # the parser's upload buffer, WebRequest.cpp:580-590
CAP1_BELOW_MAXBLOCK = 16000     # restAPI.ino:61 and :88
PAIR_DEFINITE_MAXBLOCK = 32000  # an estimated margin, see the gate-balance note in the docstring
PAIR_RCVBUF = 1024              # receive window for pair connection A
STALL_RCVBUF = 2048             # receive window for a stalled storm GET
MAX_RETRY_AFTER_S = 5.0         # cap on honoured Retry-After during readback/snapshots
PATTERN_LINE = 64

DEFAULT_PATHS = [
    # the classic shell and the assets its loader fetches (data/index.html:46-47, :93-94)
    "/", "/ds-tokens.css", "/components.css", "/index.js", "/graph.js", "/sat.js",
    "/echarts-theme.js", "/theme-toggle.js", "/sat-slider.js",
    # the v2 shell
    "/v2.html", "/v2-bundle.css", "/v2.js",
    # chunked REST routes
    "/api/v2/settings", "/api/v2/device/info", "/api/v2/sat/status",
]
GATE_PATHS = ("/api/v2/settings", "/")    # one per gate: REST and static file
# each 200 leaked a REST slot on alpha.224 to alpha.378 (TASK-1172)
LEAKY_ROUTES = ("/api/v2/sensors/labels", "/api/v2/sat/markers")

SNAP_KEYS = (
    "fwversion", "hardware_type", "bootcount", "lastreset", "uptime",
    "freeheap", "maxfreeblock", "internal_free", "internal_maxblk", "psram_found",
    "hd_min_max_block", "hd_min_free_heap", "hd_max_loop_gap_ms",
    "hd_tcp_active_pcbs", "hd_rest_503", "hd_webfile_503",
    "hd_rest_inflight_hwm", "hd_webfile_inflight_hwm", "hd_fragmentation_pct",
    "hd_ws_drops", "mqttconnected",
)
PROBE_KEYS = ("freeheap", "maxfreeblock", "hd_tcp_active_pcbs", "bootcount",
              "hd_rest_503", "hd_webfile_503")

_print_lock = threading.Lock()


def say(msg):
    with _print_lock:
        print(msg, flush=True)


# ---------------------------------------------------------------------------
# Raw-socket HTTP
# ---------------------------------------------------------------------------
def split_hostport(hostport):
    host, sep, port = hostport.rpartition(":")
    if sep and port.isdigit():
        return host, int(port)
    return hostport, 80


def host_header(hostport):
    host, port = split_hostport(hostport)
    return host if port == 80 else "%s:%d" % (host, port)


def _ms(t0):
    return round((time.monotonic() - t0) * 1000.0, 1)


def _errname(exc):
    if isinstance(exc, ConnectionRefusedError):
        return "refused"
    if isinstance(exc, (ConnectionResetError, ConnectionAbortedError, BrokenPipeError)):
        return "reset"
    if isinstance(exc, socket.timeout):
        return "timeout"
    code = getattr(exc, "winerror", None) or getattr(exc, "errno", None)
    return "oserror:%s" % (code if code is not None else type(exc).__name__)


def _set_linger_zero(sock):
    # struct linger is two u_short on Windows and two int on POSIX.
    fmt = "HH" if sys.platform == "win32" else "ii"
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack(fmt, 1, 0))


def _close_quietly(sock):
    try:
        sock.close()
    except OSError:
        pass


def fin_close(sock, drain_s=1.0):
    """End with a FIN: shut down our write side, drain until the device closes
    too, then close. A plain close() with unread data in the receive buffer can
    go out as an RST instead. Returns seconds until the device's EOF, or None."""
    t0 = time.monotonic()
    eof_after = None
    try:
        sock.shutdown(socket.SHUT_WR)
    except OSError:
        pass
    sock.settimeout(0.1)
    while time.monotonic() - t0 < drain_s:
        try:
            d = sock.recv(65536)
        except socket.timeout:
            continue
        except OSError:
            break
        if not d:
            eof_after = round(time.monotonic() - t0, 3)
            break
    _close_quietly(sock)
    return eof_after


def rst_close(sock):
    try:
        _set_linger_zero(sock)
    except OSError:
        pass
    _close_quietly(sock)


class Dechunker:
    """Incremental Transfer-Encoding: chunked decoder."""

    def __init__(self):
        self.buf = bytearray()
        self.state = "size"
        self.remaining = 0
        self.done = False
        self.error = False

    def feed(self, data):
        self.buf += data
        out = bytearray()
        while not self.done and not self.error:
            if self.state == "size":
                i = self.buf.find(b"\r\n")
                if i < 0:
                    break
                field = bytes(self.buf[:i]).split(b";")[0].strip()
                del self.buf[:i + 2]
                try:
                    n = int(field, 16)
                except ValueError:
                    self.error = True
                    break
                self.state = "trailer" if n == 0 else "data"
                self.remaining = n
            elif self.state == "data":
                if not self.buf:
                    break
                take = min(self.remaining, len(self.buf))
                out += self.buf[:take]
                del self.buf[:take]
                self.remaining -= take
                if self.remaining == 0:
                    self.state = "crlf"
            elif self.state == "crlf":
                if len(self.buf) < 2:
                    break
                if bytes(self.buf[:2]) != b"\r\n":
                    self.error = True
                    break
                del self.buf[:2]
                self.state = "size"
            else:  # trailer section, ends with an empty line
                i = self.buf.find(b"\r\n")
                if i < 0:
                    break
                line = bytes(self.buf[:i])
                del self.buf[:i + 2]
                if line == b"":
                    self.done = True
        return bytes(out)


def _parse_head(head, rec):
    lines = head.split(b"\r\n")
    m = re.match(rb"HTTP/1\.[01] (\d{3})", lines[0])
    if not m:
        return False
    rec["status"] = int(m.group(1))
    hdrs = {}
    for ln in lines[1:]:
        k, sep, v = ln.partition(b":")
        if sep:
            hdrs[k.strip().lower().decode("latin-1")] = v.strip().decode("latin-1")
    rec["chunked"] = "chunked" in hdrs.get("transfer-encoding", "").lower()
    if not rec["chunked"] and "content-length" in hdrs:
        try:
            rec["content_length"] = int(hdrs["content-length"])
        except ValueError:
            pass
    rec["retry_after"] = hdrs.get("retry-after")
    rec["content_type"] = hdrs.get("content-type")
    rec["etag"] = hdrs.get("etag")
    rec["location"] = hdrs.get("location")
    return True


def _request_bytes(hostport, method, path, headers=None, body=b""):
    lines = ["%s %s HTTP/1.1" % (method, path),
             "Host: %s" % host_header(hostport),
             "User-Agent: refresh_storm (TASK-1124)",
             "Accept: */*",
             "Connection: close"]
    for k, v in (headers or {}).items():
        lines.append("%s: %s" % (k, v))
    if body:
        lines.append("Content-Length: %d" % len(body))
    return ("\r\n".join(lines) + "\r\n\r\n").encode("latin-1") + (body or b"")


def http_exchange(hostport, method, path, headers=None, body=b"", abort=None,
                  keep_body=False, connect_timeout=5.0, read_timeout=10.0, rcvbuf=None):
    """One request on a fresh connection. Returns a dict of raw facts; classify()
    turns them into an outcome class.

    abort: None, or {"mode": "fin"|"rst"|"stall", "after": body_bytes, "stall_s": s}.
    The abort fires once `after` body bytes arrived and the response is still
    incomplete. A response that completes first is not aborted."""
    host, port = split_hostport(hostport)
    rec = {"method": method, "path": path, "t": round(time.time(), 3), "status": None,
           "error": None, "error_phase": None, "aborted": None, "complete": False,
           "chunked": False, "content_length": None, "body_len": 0, "retry_after": None,
           "content_type": None, "ttfb_ms": None, "eof": False}
    t0 = time.monotonic()
    body_buf = bytearray() if keep_body else None
    head_snip = bytearray()
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    closed = False
    try:
        if rcvbuf:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
        sock.settimeout(connect_timeout)
        try:
            sock.connect((host, port))
        except OSError as e:
            rec["error"], rec["error_phase"] = _errname(e), "connect"
            return rec
        rec["connect_ms"] = _ms(t0)
        sock.settimeout(read_timeout)
        try:
            sock.sendall(_request_bytes(hostport, method, path, headers, body))
        except OSError as e:
            rec["error"], rec["error_phase"] = _errname(e), "send"
            return rec
        buf = bytearray()
        head_done = False
        dechunk = None
        while True:
            if (head_done and abort and not rec["complete"]
                    and rec["body_len"] >= abort.get("after", 0)):
                closed = True
                rec["aborted"] = abort["mode"]
                rec["abort_at"] = rec["body_len"]
                if abort["mode"] == "rst":
                    rst_close(sock)
                elif abort["mode"] == "stall":
                    time.sleep(abort.get("stall_s", 5.0))
                    rec["stalled_s"] = abort.get("stall_s", 5.0)
                    fin_close(sock)
                else:
                    fin_close(sock)
                break
            try:
                data = sock.recv(65536)
            except OSError as e:
                rec["error"], rec["error_phase"] = _errname(e), ("body" if head_done else "headers")
                break
            if not data:
                rec["eof"] = True
                break
            if rec["ttfb_ms"] is None:
                rec["ttfb_ms"] = _ms(t0)
            if not head_done:
                buf += data
                end = buf.find(b"\r\n\r\n")
                if end < 0:
                    if len(buf) > 16384:
                        rec["error"], rec["error_phase"] = "bad_response", "headers"
                        break
                    continue
                if not _parse_head(bytes(buf[:end]), rec):
                    rec["error"], rec["error_phase"] = "bad_response", "headers"
                    break
                head_done = True
                data = bytes(buf[end + 4:])
                st = rec["status"]
                if method == "HEAD" or st in (204, 304) or 100 <= st < 200:
                    rec["complete"] = True
                    break
                if rec["chunked"]:
                    dechunk = Dechunker()
                elif rec["content_length"] == 0:
                    rec["complete"] = True
                    break
                if not data:
                    continue
            if dechunk is not None:
                piece = dechunk.feed(data)
            else:
                cl = rec["content_length"]
                piece = data if cl is None else data[:max(0, cl - rec["body_len"])]
            rec["body_len"] += len(piece)
            if len(head_snip) < 256:
                head_snip += piece[:256 - len(head_snip)]
            if body_buf is not None:
                body_buf += piece
            if dechunk is not None:
                if dechunk.error:
                    rec["error"], rec["error_phase"] = "bad_chunking", "body"
                    break
                if dechunk.done:
                    rec["complete"] = True
            elif rec["content_length"] is not None and rec["body_len"] >= rec["content_length"]:
                rec["complete"] = True
            if rec["complete"]:
                break
        if (head_done and not rec["complete"] and dechunk is None
                and rec["content_length"] is None and rec["eof"]
                and not rec["error"] and not rec["aborted"]):
            rec["complete"] = True          # close-delimited body: cannot be checked
            rec["close_delimited"] = True
    finally:
        if not closed:
            _close_quietly(sock)
    rec["total_ms"] = _ms(t0)
    rec["body_head"] = bytes(head_snip)
    if body_buf is not None:
        rec["body"] = bytes(body_buf)
    return rec


def classify(rec):
    """Outcome class of one exchange. Pure function of the recorded facts."""
    if rec.get("aborted"):
        return "aborted_" + rec["aborted"]
    err = rec.get("error")
    if err in ("bad_response", "bad_chunking"):
        return "bad_response"
    if err and rec.get("status") is None:
        if err == "refused":
            return "refused"
        if err == "reset":
            return "reset"
        if err == "timeout":
            return "timeout"
        return "error"
    st = rec.get("status")
    if st is None:
        return "bad_response"
    if st == 304:
        return "not_modified"
    if st == 503:
        return "503_retry_after" if rec.get("retry_after") is not None else "503_no_retry_after"
    if st == 429:
        return "429"
    if 200 <= st < 300:
        # short: the TASK-1162 symptom (fewer bytes than Content-Length), or a
        # chunked body cut before its terminator, or a reset/timeout mid-body.
        return "ok" if rec.get("complete") else "short"
    if 300 <= st < 400:
        return "redirect"
    if 400 <= st < 500:
        return "client_error"
    return "server_error"


def reason_of(rec):
    """Which refusal a 503 is, from its body (see the docstring for the sources)."""
    b = rec.get("body_head") or b""
    if b"Server busy" in b:
        return "rest_busy"
    if b"low heap" in b.lower() or b"Heap too low" in b:
        return "low_heap"
    if not b:
        return "empty_body"
    return "other"


def slim(rec):
    """JSON-safe copy of an exchange record (drops the byte buffers)."""
    return {k: v for k, v in rec.items() if k not in ("body", "body_head")}


def retry_delay(rec, default=1.0):
    ra = rec.get("retry_after")
    try:
        return min(max(float(ra), 0.2), MAX_RETRY_AFTER_S)
    except (TypeError, ValueError):
        return default


def pct(values, p):
    if not values:
        return None
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(math.ceil(p / 100.0 * len(s))) - 1))
    return s[k]


# ---------------------------------------------------------------------------
# NDJSON log
# ---------------------------------------------------------------------------
class NdjsonLog:
    def __init__(self, path):
        d = os.path.dirname(path)
        if d:
            os.makedirs(d, exist_ok=True)
        self.path = path
        self._fh = open(path, "a", encoding="utf-8")
        self._lock = threading.Lock()

    def write(self, rec):
        line = json.dumps(rec, default=lambda o: o.decode("latin-1") if isinstance(o, bytes) else str(o))
        with self._lock:
            self._fh.write(line + "\n")
            self._fh.flush()

    def close(self):
        with self._lock:
            self._fh.close()


def default_log_path(hostport, mode):
    base = os.environ.get("LOCALAPPDATA")
    d = (os.path.join(base, "OTGW-capture", "refresh-storm") if base
         else os.path.join(tempfile.gettempdir(), "refresh-storm"))
    safe = re.sub(r"[^A-Za-z0-9.-]", "_", hostport)
    return os.path.join(d, "refresh_storm-%s-%s-%s.ndjson" % (safe, mode, time.strftime("%Y%m%d-%H%M%S")))


# ---------------------------------------------------------------------------
# device/info snapshots
# ---------------------------------------------------------------------------
_UPTIME_RE = re.compile(r"(\d+)\(d\)-(\d+):(\d+)")


def parse_uptime(s):
    m = _UPTIME_RE.match(s or "")
    if not m:
        return None
    return int(m.group(1)) * 1440 + int(m.group(2)) * 60 + int(m.group(3))


def _get_json(opts, path):
    r = http_exchange(opts.host, "GET", path, keep_body=True,
                      connect_timeout=opts.connect_timeout, read_timeout=opts.read_timeout)
    c = classify(r)
    if c == "ok" and r["status"] == 200:
        try:
            return r, c, json.loads(r["body"].decode("utf-8", "replace"))
        except ValueError:
            return r, "bad_json", None
    return r, c, None


def device_snapshot(opts, log, label, attempts=6):
    """GET /api/v2/device/info with retries. Returns the SNAP_KEYS subset or None."""
    last = None
    for k in range(attempts):
        r, c, doc = _get_json(opts, "/api/v2/device/info")
        dev = doc.get("device") if isinstance(doc, dict) else None
        if isinstance(dev, dict):
            snap = {key: dev.get(key) for key in SNAP_KEYS}
            snap["uptime_min"] = parse_uptime(dev.get("uptime"))
            snap["t"] = round(time.time(), 3)
            log.write({"type": "snapshot", "label": label, "attempt": k + 1, "device": snap})
            return snap
        last = c if doc is None else "no_device_key"
        if r["status"] == 503:
            last += "/" + reason_of(r)
        time.sleep(retry_delay(r))
    log.write({"type": "snapshot", "label": label, "device": None, "last": last})
    say("  snapshot %s: device/info unavailable (%s)" % (label, last))
    return None


def context_snapshot(opts, log):
    """Board context worth recording with every run (BLE policy, SATble.ino:1079-1082)."""
    r, c, doc = _get_json(opts, "/api/v2/sat/ble/discovery")
    ctx = {"ble_discovery": c}
    if isinstance(doc, dict):
        ctx.update({k: doc.get(k) for k in ("psram", "ble_enable", "risk_ack", "active")})
    log.write({"type": "context", **ctx})
    return ctx


def compare_snaps(before, after, heap_noise, pcb_tol=1):
    """Reboot, heap and PCB checks between two snapshots."""
    out = {"reboot": "unknown", "heap": "unknown", "pcbs": "unknown"}
    if not before or not after:
        return out

    def delta(key):
        a, b = after.get(key), before.get(key)
        return (a - b) if isinstance(a, (int, float)) and isinstance(b, (int, float)) else None

    boot_b, boot_a = before.get("bootcount"), after.get("bootcount")
    up_b, up_a = before.get("uptime_min"), after.get("uptime_min")
    rebooted = ((boot_b is not None and boot_a is not None and boot_a != boot_b)
                or (up_b is not None and up_a is not None and up_a < up_b))
    out["reboot"] = "FAIL" if rebooted else "ok"
    out["bootcount"] = [boot_b, boot_a]
    out["uptime_min"] = [up_b, up_a]
    out["lastreset"] = [before.get("lastreset"), after.get("lastreset")]
    for key in ("freeheap", "maxfreeblock", "hd_rest_503", "hd_webfile_503", "hd_tcp_active_pcbs"):
        out["d_" + key] = delta(key)
    dh = out["d_freeheap"]
    out["heap"] = "unknown" if dh is None else ("WARN" if -dh > heap_noise else "ok")
    pb, pa = before.get("hd_tcp_active_pcbs"), after.get("hd_tcp_active_pcbs")
    if isinstance(pb, int) and isinstance(pa, int):
        out["pcbs"] = "WARN" if pa > pb + pcb_tol else "ok"
    return out


# ---------------------------------------------------------------------------
# Upload abort (AC#1)
# ---------------------------------------------------------------------------
def pattern(i, size):
    """Upload content for iteration i. Every 64-byte line names its upload and
    offset, so any readback shows where it came from."""
    out = bytearray()
    off = 0
    while len(out) < size:
        head = b"t1124 iter=%04d off=%08d " % (i, off)
        fill = bytes(65 + ((i * 7 + off // PATTERN_LINE + k) % 26)
                     for k in range(PATTERN_LINE - len(head) - 1))
        out += head + fill + b"\n"
        off += PATTERN_LINE
    return bytes(out[:size])


_ITER_RE = re.compile(rb"t1124 iter=(\d{4}) off=")


def judge_readback(body, i, size):
    """(class, source_iteration) for a readback of upload i."""
    if body is None:
        return "unreadable", None
    if len(body) == 0:
        return "empty", None
    m = _ITER_RE.match(body)
    if not m:
        return ("ambiguous" if len(body) < 32 else "foreign"), None
    j = int(m.group(1))
    if not pattern(j, size).startswith(body):
        return "foreign", j                  # not byte-exact: corruption
    if j == i:
        return ("complete_current" if len(body) == size else "prefix_current"), j
    if j < i:
        return "stale_previous", j
    return "foreign", j


def _wait_server(sock, timeout):
    """Wait for the device to act on an open upload connection."""
    t0 = time.monotonic()
    sock.settimeout(timeout)
    try:
        d = sock.recv(4096)
    except socket.timeout:
        return {"kind": "none", "after_s": round(timeout, 3)}
    except (ConnectionResetError, ConnectionAbortedError):
        return {"kind": "rst", "after_s": round(time.monotonic() - t0, 3)}
    except OSError as e:
        return {"kind": "error", "error": _errname(e), "after_s": round(time.monotonic() - t0, 3)}
    if not d:
        return {"kind": "fin", "after_s": round(time.monotonic() - t0, 3)}
    first = d.split(b"\r\n", 1)[0].decode("latin-1", "replace")
    return {"kind": "data", "after_s": round(time.monotonic() - t0, 3), "first_line": first}


def upload(opts, name, content, abort_mode=None, send_bytes=None):
    """POST /upload?path=/ as multipart/form-data (field "upload", like the UI).
    With abort_mode, send send_bytes of the content and then end the connection."""
    host, port = split_hostport(opts.host)
    boundary = "----t1124" + os.urandom(8).hex()
    part_head = ("--%s\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"%s\"\r\n"
                 "Content-Type: application/octet-stream\r\n\r\n" % (boundary, name)).encode("latin-1")
    part_tail = ("\r\n--%s--\r\n" % boundary).encode("latin-1")
    total = len(part_head) + len(content) + len(part_tail)
    req = ("POST /upload?path=%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: refresh_storm (TASK-1124)\r\n"
           "Content-Type: multipart/form-data; boundary=%s\r\nContent-Length: %d\r\n"
           "Connection: close\r\n\r\n" % (quote("/", safe=""), host_header(opts.host), boundary, total)
           ).encode("latin-1")
    rec = {"mode": abort_mode or "complete", "declared_content": len(content), "status": None,
           "error": None}
    t0 = time.monotonic()
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.settimeout(opts.connect_timeout)
        try:
            sock.connect((host, port))
        except OSError as e:
            rec["error"] = "connect:" + _errname(e)
            _close_quietly(sock)
            return rec
        sock.settimeout(opts.read_timeout)
        if abort_mode is None:
            try:
                sock.sendall(req + part_head + content + part_tail)
            except OSError as e:
                rec["error"] = "send:" + _errname(e)
                _close_quietly(sock)
                return rec
            buf = bytearray()
            while b"\r\n" not in buf:
                try:
                    d = sock.recv(4096)
                except OSError as e:
                    rec["error"] = "recv:" + _errname(e)
                    break
                if not d:
                    break
                buf += d
            m = re.match(rb"HTTP/1\.[01] (\d{3})", bytes(buf))
            rec["status"] = int(m.group(1)) if m else None
            rec["sent_content"] = len(content)
            fin_close(sock)
        else:
            n = max(0, min(send_bytes, len(content) - 1))
            try:
                sock.sendall(req + part_head + content[:n])
            except OSError as e:
                rec["error"] = "send:" + _errname(e)
                _close_quietly(sock)
                return rec
            rec["sent_content"] = n
            if abort_mode == "stall":
                ev = _wait_server(sock, opts.stall_seconds)
                rec["server_event"] = ev
                if ev["kind"] in ("none", "data"):
                    fin_close(sock)       # the device did not end it; we do
                else:
                    _close_quietly(sock)
            else:
                ev = _wait_server(sock, opts.pre_abort_delay)
                if ev["kind"] != "none":
                    rec["server_early"] = ev   # unexpected: the device acted mid-body
                if abort_mode == "rst":
                    rst_close(sock)
                else:
                    rec["server_eof_after_fin_s"] = fin_close(sock, drain_s=2.0)
    finally:
        rec["total_ms"] = _ms(t0)
    return rec


SETTLED = ("prefix_current", "complete_current", "foreign", "ambiguous", "helper_page", "wrong_type")


def readback(opts, path, i, size):
    """Read the uploaded file until the result settles.

    Retries a short 200 (TASK-1162), a 503 (file gate, honouring Retry-After),
    transport errors, and results that a late teardown could still change
    (stale_previous, empty, missing). Only a complete 200 is ever judged."""
    history = []
    verdict, src, body_len = "unreadable", None, None
    for attempt in range(1, opts.settle_attempts + 1):
        r = http_exchange(opts.host, "GET", path, keep_body=True,
                          connect_timeout=opts.connect_timeout, read_timeout=opts.read_timeout)
        c = classify(r)
        h = {"attempt": attempt, "class": c, "status": r["status"], "body_len": r["body_len"],
             "content_length": r["content_length"], "content_type": r["content_type"]}
        wait = opts.settle_wait
        src, body_len = None, None
        if c == "ok" and r["status"] == 200:
            ctype = (r["content_type"] or "").lower()
            if ctype.startswith("text/plain"):
                verdict, src = judge_readback(r["body"], i, size)
                body_len = len(r["body"])
            else:
                # the upload help page (no /FSexplorer.html) is a 200 text/html for any path
                verdict = "helper_page" if ctype.startswith("text/html") else "wrong_type"
            h["judge"], h["source_iter"] = verdict, src
        elif r["status"] == 404:
            verdict = "missing"
        else:
            verdict = "unreadable"          # short 200, 503, reset, timeout, ...
            if r["status"] == 503:
                h["reason"] = reason_of(r)
                wait = retry_delay(r)
        history.append(h)
        if verdict in SETTLED:
            break
        if attempt < opts.settle_attempts:
            time.sleep(wait)
    return {"class": verdict, "source_iter": src, "stored_len": body_len,
            "attempts": len(history), "history": history}


def delete_file(opts, path):
    """Delete through the real API, then prove it with a 404."""
    r = http_exchange(opts.host, "GET", "/api/listfiles?delete=" + quote(path, safe=""),
                      keep_body=True, connect_timeout=opts.connect_timeout,
                      read_timeout=opts.read_timeout)
    out = {"delete_status": r["status"], "delete_class": classify(r),
           "delete_body": (r.get("body") or b"")[:40].decode("latin-1", "replace")}
    g = None
    for _ in range(3):
        g = http_exchange(opts.host, "GET", path, connect_timeout=opts.connect_timeout,
                          read_timeout=opts.read_timeout)
        if g["status"] in (200, 404):
            break
        time.sleep(retry_delay(g))
    out["after_status"], out["after_class"] = g["status"], classify(g)
    if g["status"] == 404:
        out["gone"] = True
    elif g["status"] == 200 and (g["content_type"] or "").lower().startswith("text/html"):
        out["gone"] = None      # help page: this GET cannot tell
    else:
        out["gone"] = False
    return out


class _StopRun(Exception):
    """Ends the upload sequence early; cleanup still runs."""


def run_upload_abort(opts, log):
    name = opts.upload_name.lstrip("/")
    path = "/" + name
    size, n_iter = opts.upload_size, opts.upload_abort
    if opts.abort_mode == "mixed":
        modes = [("fin", "rst", "stall")[(k - 1) % 3] for k in range(1, n_iter + 1)]
    else:
        modes = [opts.abort_mode] * n_iter
    say("upload-abort: %d aborts (%s) of %s on %s; %d of %d content bytes sent per abort, "
        "pre-abort delay %.2fs, stall window %.1fs"
        % (n_iter, opts.abort_mode, path, opts.host, opts.upload_send, size,
           opts.pre_abort_delay, opts.stall_seconds))
    fail, inconclusive, warn = [], [], []
    base = device_snapshot(opts, log, "upload_before")
    if base is None:
        say("VERDICT: INCONCLUSIVE - device/info unreachable before the run")
        return "INCONCLUSIVE", {"reason": "no baseline"}
    context_snapshot(opts, log)
    say("baseline: boot=%s uptime=%s lastreset=%s heap=%s maxblk=%s pcbs=%s"
        % (base["bootcount"], base["uptime"], base["lastreset"], base["freeheap"],
           base["maxfreeblock"], base["hd_tcp_active_pcbs"]))
    classes = Counter()
    iters = []
    control = final = None
    interrupted = False
    cleanup = None
    try:
        # Control: a complete upload must read back byte-exact. It also leaves a
        # known upload 0 behind, so a leak shows up as stale from iteration 1.
        up = upload(opts, name, pattern(0, size))
        rb = readback(opts, path, 0, size)
        control = {"upload": up, "readback": rb}
        log.write({"type": "upload", "iter": 0, "role": "control", **up})
        log.write({"type": "readback", "iter": 0, "role": "control", **rb})
        say("control   upload %s | readback %s %s B" % (up["status"], rb["class"], rb["stored_len"]))
        if up["status"] == 401:
            inconclusive.append("HTTP password set: upload refused with 401 (clear it for this test)")
            raise _StopRun()
        if up["status"] != 303 or rb["class"] != "complete_current":
            # Without a validated readback path every later readback is noise. A
            # 507 (full filesystem) next to a leftover of a killed run would
            # otherwise read back foreign or stale and report a FAIL.
            inconclusive.append("control failed (upload %s, readback %s): the readback path is not "
                                "validated, so no abort was run" % (up["status"], rb["class"]))
            raise _StopRun()
        for i in range(1, n_iter + 1):
            mode = modes[i - 1]
            up = upload(opts, name, pattern(i, size), abort_mode=mode, send_bytes=opts.upload_send)
            time.sleep(opts.readback_wait)
            rb = readback(opts, path, i, size)
            classes[rb["class"]] += 1
            iters.append({"iter": i, "mode": mode, "upload": up, "readback": rb})
            log.write({"type": "upload", "iter": i, **up})
            log.write({"type": "readback", "iter": i, "mode": mode, **rb})
            ev = up.get("server_event") or up.get("server_early") or {}
            if ev:
                evs = "server %s@%ss" % (ev.get("kind"), ev.get("after_s"))
            elif mode == "fin":
                evs = "server EOF %ss after our FIN" % up.get("server_eof_after_fin_s")
            else:
                evs = "server -"
            say("iter %03d %-5s sent %d %s | readback %s %s B%s (%d attempt%s)"
                % (i, mode, up.get("sent_content", 0), evs, rb["class"], rb["stored_len"],
                   (" from iter %s" % rb["source_iter"]) if rb["source_iter"] not in (None, i) else "",
                   rb["attempts"], "" if rb["attempts"] == 1 else "s"))
        # Final: the upload path still works after the aborts.
        fin_i = n_iter + 1
        up = upload(opts, name, pattern(fin_i, size))
        rb = readback(opts, path, fin_i, size)
        final = {"upload": up, "readback": rb}
        log.write({"type": "upload", "iter": fin_i, "role": "final", **up})
        log.write({"type": "readback", "iter": fin_i, "role": "final", **rb})
        say("final     upload %s | readback %s %s B" % (up["status"], rb["class"], rb["stored_len"]))
        if up["status"] != 303:
            fail.append("final complete upload returned %s, expected 303" % up["status"])
        if rb["class"] != "complete_current":
            (inconclusive if rb["class"] in ("unreadable", "helper_page") else fail).append(
                "final readback %s, expected complete_current" % rb["class"])
    except _StopRun:
        pass
    except KeyboardInterrupt:
        interrupted = True
        inconclusive.append("interrupted")
    finally:
        cleanup = delete_file(opts, path)
        log.write({"type": "cleanup", **cleanup})
        say("cleanup   delete %s (%s) | GET afterwards %s"
            % (cleanup["delete_status"], cleanup["delete_body"].strip(), cleanup.get("after_status")))
    if cleanup["delete_status"] == 401:
        inconclusive.append("cleanup refused with 401 (HTTP password): delete %s by hand" % path)
    elif cleanup["gone"] is False:
        fail.append("test file still present after delete (GET gave %s)" % cleanup.get("after_status"))
    elif cleanup["gone"] is None:
        inconclusive.append("cannot confirm the delete: GET afterwards returned the help page")
    stale = Counter(it["mode"] for it in iters if it["readback"]["class"] == "stale_previous")
    if stale["fin"] or stale["stall"]:
        fail.append("%d readback(s) stale_previous %s: the aborted upload's handle stayed open"
                    % (sum(stale.values()), dict(stale)))
    elif stale["rst"]:
        # An RST can remove the queued data before the handler ever opens the file
        # (AsyncTCP.cpp:494-500). That reads back exactly like a leaked handle.
        inconclusive.append("%d rst readback(s) stale_previous and no fin or stall one: a leaked handle, "
                            "or an RST that dropped the queued data before the file was opened. rst "
                            "alone cannot tell them apart. Rerun with --abort-mode fin, or with a "
                            "longer --pre-abort-delay: a leak stays stale" % stale["rst"])
    if classes["foreign"]:
        fail.append("%d readback(s) foreign: content matches no upload" % classes["foreign"])
    unsure = {k: v for k, v in classes.items()
              if k in ("empty", "missing", "unreadable", "ambiguous", "helper_page", "wrong_type")}
    if unsure:
        inconclusive.append("readbacks that prove nothing: %s" % dict(unsure))
    stalls = [it["upload"].get("server_event", {}) for it in iters if it["mode"] == "stall"]
    kept_open = [s for s in stalls if s.get("kind") in ("none", "data")]
    if kept_open:
        warn.append("%d stall(s): the device did not end the connection within %.1fs"
                    % (len(kept_open), opts.stall_seconds))
    after = device_snapshot(opts, log, "upload_after")
    cmp_ = compare_snaps(base, after, opts.heap_noise)
    if after is None:
        inconclusive.append("device/info unreachable after the run")
    if cmp_["reboot"] == "FAIL":
        fail.append("device rebooted: bootcount %s, uptime_min %s" % (cmp_["bootcount"], cmp_["uptime_min"]))
    if cmp_["heap"] == "WARN":
        warn.append("free heap dropped %d B (noise %d B)" % (-cmp_["d_freeheap"], opts.heap_noise))
    per_abort = (cmp_["d_freeheap"] / len(iters)) if (cmp_.get("d_freeheap") is not None and iters) else None
    verdict = "FAIL" if fail else ("INCONCLUSIVE" if inconclusive else "PASS")
    summary = {"type": "summary", "mode": "upload_abort", "verdict": verdict, "fail": fail,
               "inconclusive": inconclusive, "warn": warn, "readback_classes": dict(classes),
               "stale_by_mode": dict(stale), "aborts_run": len(iters),
               "compare": cmp_, "heap_delta_per_abort": per_abort, "interrupted": interrupted}
    log.write(summary)
    say("\n== UPLOAD-ABORT RESULT ==")
    say("readbacks: %s" % dict(classes))
    if stalls:
        say("stall server events: %s" % [(s.get("kind"), s.get("after_s")) for s in stalls])
    say("snapshots: bootcount %s  uptime_min %s  lastreset %s"
        % (cmp_.get("bootcount"), cmp_.get("uptime_min"), cmp_.get("lastreset")))
    say("heap: d_freeheap %s  d_maxfreeblock %s  per abort %s B (a leaked handle holds at least the "
        "512 B LittleFS file cache)" % (cmp_.get("d_freeheap"), cmp_.get("d_maxfreeblock"),
                                         None if per_abort is None else round(per_abort)))
    for w in warn:
        say("WARN: " + w)
    for x in inconclusive:
        say("INCONCLUSIVE: " + x)
    for f in fail:
        say("FAIL: " + f)
    say("VERDICT: " + verdict)
    return verdict, summary


# ---------------------------------------------------------------------------
# Storm (AC#2, AC#4)
# ---------------------------------------------------------------------------
class WsLeg:
    """Persistent /ws subscribers, reusing test_ws_liveload.ws_subscriber()."""

    def __init__(self, hostport, n):
        if HERE not in sys.path:
            sys.path.insert(0, HERE)
        import test_ws_liveload as wsl
        self.wsl = wsl
        wsl._stop = threading.Event()
        with wsl._lock:
            for k in wsl._stats:
                wsl._stats[k] = 0
        self.threads = [threading.Thread(target=wsl.ws_subscriber, args=(hostport, i), daemon=True)
                        for i in range(n)]

    def start(self):
        for t in self.threads:
            t.start()

    def stats(self):
        # the shared dict also holds test_ws_liveload's own HTTP flood counters
        with self.wsl._lock:
            return {k: v for k, v in self.wsl._stats.items() if k.startswith("ws_")}

    def stop(self):
        self.wsl._stop.set()
        for t in self.threads:
            t.join(timeout=12)


class ArmState:
    def __init__(self):
        self.lock = threading.Lock()
        self.requests = []
        self.probes = []

    def add(self, rec):
        with self.lock:
            self.requests.append(rec)

    def add_probe(self, rec):
        with self.lock:
            self.probes.append(rec)

    def counts(self):
        with self.lock:
            return Counter(r["class"] for r in self.requests)


def storm_worker(opts, log, label, wid, stop, st):
    rng = random.Random(opts.seed * 1009 + wid)
    paths = opts.path_list
    i = wid
    etags = {}
    while not stop.is_set():
        path = paths[i % len(paths)]
        i += 1
        abort = None
        if rng.random() < opts.abort_ratio:
            mode = opts.abort_mode
            if mode == "mixed":
                mode = "fin" if rng.random() < 0.5 else "rst"
            abort = {"mode": mode, "after": opts.abort_after, "stall_s": opts.stall_seconds}
        hdrs = {"If-None-Match": etags[path]} if (opts.conditional and path in etags) else None
        r = http_exchange(opts.host, "GET", path, headers=hdrs, abort=abort,
                          connect_timeout=opts.connect_timeout, read_timeout=opts.read_timeout,
                          rcvbuf=STALL_RCVBUF if (abort and abort["mode"] == "stall") else None)
        c = classify(r)
        rec = slim(r)
        rec.update({"type": "request", "arm": label, "worker": wid, "class": c,
                    "planned_abort": abort["mode"] if abort else None})
        if r["status"] == 503:
            rec["reason"] = reason_of(r)
        if r.get("etag"):
            etags[path] = r["etag"]
        log.write(rec)
        st.add(rec)
        if r["error_phase"] == "connect":
            stop.wait(0.1)   # a dead device must not turn into a busy loop


def probe_record(label, t0, r, c):
    """One probe line. An exchange that failed to connect or send returns before
    total_ms is set (http_exchange), so read it with .get(): r["total_ms"] raised
    KeyError and ended the probe thread in the middle of an overloaded arm."""
    return {"type": "probe", "arm": label, "t_rel_s": round(time.monotonic() - t0, 1),
            "class": c, "status": r.get("status"), "total_ms": r.get("total_ms")}


def probe_loop(opts, log, label, stop, st, t0):
    while not stop.wait(opts.probe_interval):
        r, c, doc = _get_json(opts, "/api/v2/device/info")
        p = probe_record(label, t0, r, c)
        dev = doc.get("device") if isinstance(doc, dict) else None
        if isinstance(dev, dict):
            p.update({k: dev.get(k) for k in PROBE_KEYS})
            p["uptime_min"] = parse_uptime(dev.get("uptime"))
        if r["status"] == 503:
            p["reason"] = reason_of(r)
        log.write(p)
        st.add_probe(p)
        cnt = st.counts()
        if isinstance(dev, dict):
            say("  +%5.1fs probe 200 boot=%s heap=%s maxblk=%s pcbs=%s/%s | ok=%d 503=%d aborted=%d err=%d"
                % (p["t_rel_s"], p["bootcount"], p["freeheap"], p["maxfreeblock"],
                   p["hd_tcp_active_pcbs"], opts.pcb_pool, cnt["ok"],
                   cnt["503_retry_after"] + cnt["503_no_retry_after"],
                   sum(v for k, v in cnt.items() if k.startswith("aborted_")),
                   cnt["refused"] + cnt["reset"] + cnt["timeout"] + cnt["error"]))
        else:
            say("  +%5.1fs probe %s %s" % (p["t_rel_s"], c, p.get("reason", "")))


def time_to_first_200(opts, window):
    t0 = time.monotonic()
    attempts = 0
    while time.monotonic() - t0 < max(window, 1.0):
        attempts += 1
        r = http_exchange(opts.host, "GET", "/api/v2/device/info",
                          connect_timeout=opts.connect_timeout, read_timeout=opts.read_timeout)
        if classify(r) == "ok" and r["status"] == 200:
            return _ms(t0), attempts
        time.sleep(0.25)
    return None, attempts


def _peek_until(sock, marker, timeout):
    """Wait until `marker` is in the receive buffer without consuming it: reading
    would reopen the window and let the device finish its response."""
    t_end = time.monotonic() + timeout
    while time.monotonic() < t_end:
        r, _, _ = select.select([sock], [], [], max(0.0, t_end - time.monotonic()))
        if not r:
            break
        try:
            data = sock.recv(65536, socket.MSG_PEEK)
        except OSError:
            return None
        if not data:
            return None
        if marker in data:
            return data
        time.sleep(0.01)
    return None


def _peeked_complete(data):
    end = data.find(b"\r\n\r\n")
    if end < 0:
        return False
    rec = {"content_length": None, "chunked": False}
    if not _parse_head(data[:end], rec):
        return False
    body = data[end + 4:]
    if rec["content_length"] is not None:
        return len(body) >= rec["content_length"]
    if rec["chunked"]:
        d = Dechunker()
        d.feed(body)
        return d.done
    return False


def _peek_now(sock):
    # Large enough for any response the device sends (index.js is under 400 KB),
    # so a fully buffered response is never mistaken for an incomplete one.
    try:
        sock.setblocking(False)
        return sock.recv(1 << 22, socket.MSG_PEEK)
    except OSError:
        return b""
    finally:
        sock.setblocking(True)


def overlap_pair(opts, path):
    """One pair attempt. A holds a slot mid-response: its 1 KB receive window is
    only peeked, so the device cannot finish sending. B is then fetched in full.
    The overlap is proven only if A's response is still incomplete after B is
    done: until the device has sent everything it cannot release A's slot."""
    host, port = split_hostport(opts.host)
    out = {"path": path, "result": None}
    a = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        a.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, PAIR_RCVBUF)
        a.settimeout(opts.connect_timeout)
        try:
            a.connect((host, port))
            a.sendall(_request_bytes(opts.host, "GET", path))
        except OSError as e:
            out["result"] = "a_error:" + _errname(e)
            return out
        head = _peek_until(a, b"\r\n", opts.read_timeout)
        m = re.match(rb"HTTP/1\.[01] (\d{3})", head or b"")
        out["a_status"] = int(m.group(1)) if m else None
        if out["a_status"] != 200:
            out["result"] = "a_refused" if out["a_status"] else "a_no_response"
            return out
        b = http_exchange(opts.host, "GET", path,
                          connect_timeout=opts.connect_timeout, read_timeout=opts.read_timeout)
        out["b_class"], out["b_status"] = classify(b), b["status"]
        if b["status"] == 503:
            out["b_reason"] = reason_of(b)
        a_done = _peeked_complete(_peek_now(a))
        out["a_complete_after_b"] = a_done
        if b["status"] == 503:
            out["result"] = "fail"
        elif out["b_class"] == "ok" and b["status"] == 200:
            out["result"] = "not_overlapping" if a_done else "pass"
        else:
            out["result"] = "error"
        return out
    finally:
        fin_close(a, drain_s=opts.read_timeout)


def gate_balance(opts, log, label, pair_attempts=3):
    """Sequential GETs, then the overlapping pair. A failed GET or pair is retried:
    a leaked slot fails every retry, a request from another client (a browser,
    Home Assistant) does not."""
    seq = []
    for p in GATE_PATHS:
        for _ in range(opts.gate_reps):
            tries = []
            for _t in range(3):
                r = http_exchange(opts.host, "GET", p, connect_timeout=opts.connect_timeout,
                                  read_timeout=opts.read_timeout)
                e = {"class": classify(r), "status": r["status"]}
                if r["status"] == 503:
                    e["reason"] = reason_of(r)
                tries.append(e)
                if e["class"] == "ok" and r["status"] == 200:
                    break
                time.sleep(retry_delay(r))
            seq.append({"path": p, "ok": tries[-1]["class"] == "ok" and tries[-1]["status"] == 200,
                        "tries": tries})
    seq_ok = all(e["ok"] for e in seq)
    pre = device_snapshot(opts, log, label + "_pair_precheck", attempts=3)
    mb = pre.get("maxfreeblock") if pre else None
    pairs = {}
    if not isinstance(mb, int):
        pair_result = "skipped_no_snapshot"
    elif mb < CAP1_BELOW_MAXBLOCK:
        pair_result = "skipped_cap1"
    else:
        for p in GATE_PATHS:
            attempts = []
            for _ in range(pair_attempts):
                attempts.append(overlap_pair(opts, p))
                if attempts[-1]["result"] == "pass":
                    break
                time.sleep(1.0)
            pairs[p] = attempts
        per_path = []
        for attempts in pairs.values():
            results = [x["result"] for x in attempts]
            if "pass" in results:
                per_path.append("pass")
            elif all(x == "fail" for x in results):
                per_path.append("fail")
            else:
                per_path.append("inconclusive")
        if all(x == "pass" for x in per_path):
            pair_result = "pass"
        elif "fail" in per_path:
            pair_result = "fail" if mb >= PAIR_DEFINITE_MAXBLOCK else "inconclusive_near_threshold"
        else:
            pair_result = "inconclusive"
    verdict = "FAIL" if (not seq_ok or pair_result == "fail") else ("PASS" if pair_result == "pass" else "WARN")
    rec = {"type": "gate_balance", "label": label, "sequential": seq, "sequential_ok": seq_ok,
           "maxfreeblock": mb, "pairs": pairs, "pair_result": pair_result, "verdict": verdict}
    log.write(rec)
    bad = [(e["path"], e["tries"][-1]["class"], e["tries"][-1].get("reason")) for e in seq if not e["ok"]]
    retried = sum(1 for e in seq if len(e["tries"]) > 1)
    say("  gate balance %s: sequential %d/%d ok (%d needed a retry)%s | pair %s (maxfreeblock %s)"
        % (verdict, len(seq) - len(bad), len(seq), retried, (" bad=%s" % bad) if bad else "",
           pair_result, mb))
    return rec


def summarize_arm(label, workers, st, before, after, ws_delta, first_200, gb, opts, interrupted):
    with st.lock:
        reqs = list(st.requests)
        probes = list(st.probes)
    by_class = Counter(r["class"] for r in reqs)
    by_path = defaultdict(Counter)
    for r in reqs:
        by_path[r["path"]][r["class"]] += 1
    ok_ms = [r["total_ms"] for r in reqs if r["class"] == "ok"]
    ttfb = [r["ttfb_ms"] for r in reqs if r["class"] == "ok" and r.get("ttfb_ms") is not None]
    r503 = Counter("%s/%s" % ("retry_after" if r.get("retry_after") is not None else "no_retry_after",
                              r.get("reason")) for r in reqs if r.get("status") == 503)
    planned = Counter(r["planned_abort"] for r in reqs if r.get("planned_abort"))
    done_first = sum(1 for r in reqs if r.get("planned_abort") and not r.get("aborted"))
    pcbs = [p["hd_tcp_active_pcbs"] for p in probes if isinstance(p.get("hd_tcp_active_pcbs"), int)]
    blks = [p["maxfreeblock"] for p in probes if isinstance(p.get("maxfreeblock"), int)]
    heaps = [p["freeheap"] for p in probes if isinstance(p.get("freeheap"), int)]
    cmp_ = compare_snaps(before, after, opts.heap_noise)
    fail, warn, inconclusive = [], [], []
    if after is None:
        fail.append("device/info did not answer 200 after the arm and --quiet (see its snapshot line)")
    if cmp_["reboot"] == "FAIL":
        fail.append("reboot: bootcount %s uptime_min %s" % (cmp_["bootcount"], cmp_["uptime_min"]))
    if gb["verdict"] == "FAIL":
        fail.append("gate balance failed (sequential_ok=%s, pair=%s)" % (gb["sequential_ok"], gb["pair_result"]))
    elif gb["verdict"] == "WARN":
        warn.append("gate-balance pair check %s" % gb["pair_result"])
    if cmp_["pcbs"] == "WARN":
        warn.append("hd_tcp_active_pcbs %s -> %s after %ss quiet"
                    % (before.get("hd_tcp_active_pcbs"), after.get("hd_tcp_active_pcbs"), opts.quiet))
    if cmp_["heap"] == "WARN":
        warn.append("free heap dropped %d B" % -cmp_["d_freeheap"])
    if by_class["short"]:
        warn.append("%d short 200(s): body shorter than declared (TASK-1162 shape)" % by_class["short"])
    if before is None:
        inconclusive.append("no snapshot before the arm")
    if interrupted:
        inconclusive.append("interrupted")
    verdict = "FAIL" if fail else ("INCONCLUSIVE" if inconclusive else "PASS")
    return {
        "type": "arm_summary", "arm": label, "workers": workers, "duration_s": opts.duration,
        "requests": len(reqs), "classes": dict(by_class),
        "by_path": {p: dict(c) for p, c in by_path.items()},
        "ok_total_ms_p50": pct(ok_ms, 50), "ok_total_ms_p95": pct(ok_ms, 95),
        "ok_ttfb_ms_p50": pct(ttfb, 50), "ok_ttfb_ms_p95": pct(ttfb, 95),
        "503": dict(r503), "planned_aborts": dict(planned), "planned_abort_completed_first": done_first,
        "probes": len(probes), "probe_classes": dict(Counter(p["class"] for p in probes)),
        "probe_pcbs_max": max(pcbs) if pcbs else None, "pcb_pool": opts.pcb_pool,
        "probe_maxfreeblock_min": min(blks) if blks else None,
        "probe_freeheap_min": min(heaps) if heaps else None,
        "ws": ws_delta, "first_200_after_stop_ms": first_200[0], "first_200_attempts": first_200[1],
        "before": before, "after": after, "compare": cmp_,
        "gate_balance": {"verdict": gb["verdict"], "sequential_ok": gb["sequential_ok"],
                         "pair_result": gb["pair_result"], "maxfreeblock": gb["maxfreeblock"]},
        "verdict": verdict, "fail": fail, "warn": warn, "inconclusive": inconclusive,
    }


def print_arm(s):
    say("  arm %s: %d requests  %s" % (s["arm"], s["requests"],
                                     " ".join("%s=%d" % kv for kv in sorted(s["classes"].items()))))
    say("    ok latency p50/p95 %s/%s ms (ttfb %s/%s)  503 %s  aborts planned %s (completed first %d)"
        % (s["ok_total_ms_p50"], s["ok_total_ms_p95"], s["ok_ttfb_ms_p50"], s["ok_ttfb_ms_p95"],
           s["503"], s["planned_aborts"], s["planned_abort_completed_first"]))
    b, a = s["before"] or {}, s["after"] or {}
    say("    boot %s->%s  uptime %s->%s  lastreset %r  heap %s->%s (min %s)  maxblk %s->%s (min %s)"
        % (b.get("bootcount"), a.get("bootcount"), b.get("uptime"), a.get("uptime"), a.get("lastreset"),
           b.get("freeheap"), a.get("freeheap"), s["probe_freeheap_min"],
           b.get("maxfreeblock"), a.get("maxfreeblock"), s["probe_maxfreeblock_min"]))
    say("    pcbs %s->%s (probe max %s of %s)  rest_503 +%s  webfile_503 +%s  hd_min_max_block %s  "
        "first 200 after stop %s ms  ws %s"
        % (b.get("hd_tcp_active_pcbs"), a.get("hd_tcp_active_pcbs"), s["probe_pcbs_max"], s["pcb_pool"],
           s["compare"].get("d_hd_rest_503"), s["compare"].get("d_hd_webfile_503"),
           a.get("hd_min_max_block"), s["first_200_after_stop_ms"], s["ws"]))
    for w in s["warn"]:
        say("    WARN: " + w)
    for f in s["fail"]:
        say("    FAIL: " + f)
    say("    arm verdict: " + s["verdict"])


def run_arm(opts, log, label, workers, ws):
    before = device_snapshot(opts, log, label + "_before")
    ws0 = ws.stats() if ws else None
    st = ArmState()
    stop = threading.Event()
    probe = 1 if opts.probe_interval > 0 else 0
    say("\n-- arm %s: %d workers x %.0fs, abort %.0f%% %s after %d body bytes --"
        % (label, workers, opts.duration, opts.abort_ratio * 100, opts.abort_mode, opts.abort_after))
    say("   offered: %d storm + %d probe (every %.1fs) + %d /ws = up to %d TCP connections from this host"
        " (device pool %d, shared with MQTT, telnet and other clients)"
        % (workers, probe, opts.probe_interval, opts.ws_subs, workers + probe + opts.ws_subs, opts.pcb_pool))
    log.write({"type": "arm_start", "arm": label, "workers": workers, "probe": probe,
               "ws_subs": opts.ws_subs, "offered_max_connections": workers + probe + opts.ws_subs})
    t0 = time.monotonic()
    threads = [threading.Thread(target=storm_worker, args=(opts, log, label, w, stop, st), daemon=True)
               for w in range(workers)]
    if probe:
        threads.append(threading.Thread(target=probe_loop, args=(opts, log, label, stop, st, t0), daemon=True))
    for t in threads:
        t.start()
    interrupted = False
    try:
        while time.monotonic() - t0 < opts.duration:
            time.sleep(0.2)
    except KeyboardInterrupt:
        interrupted = True
        say("^C - stopping the arm")
    finally:
        stop.set()
        for t in threads:
            t.join(timeout=opts.read_timeout + opts.stall_seconds + 5)
    t_stop = time.monotonic()
    first = time_to_first_200(opts, opts.quiet)
    rest = opts.quiet - (time.monotonic() - t_stop)
    if rest > 0 and not interrupted:
        time.sleep(rest)
    gb = gate_balance(opts, log, label)
    after = device_snapshot(opts, log, label + "_after")
    ws_delta = None
    if ws:
        ws1 = ws.stats()
        ws_delta = {k: ws1[k] - ws0.get(k, 0) for k in ws1}
    s = summarize_arm(label, workers, st, before, after, ws_delta, first, gb, opts, interrupted)
    log.write(s)
    print_arm(s)
    return s, interrupted


def auth_required(opts, path):
    """True when path answers 401: an HTTP password is set (checkHttpAuth)."""
    r = http_exchange(opts.host, "GET", path, connect_timeout=opts.connect_timeout,
                      read_timeout=opts.read_timeout)
    return r["status"] == 401


def run_storm(opts, log):
    for p in opts.path_list:
        if any(p.startswith(x) for x in LEAKY_ROUTES):
            say("WARNING: %s streams a file through webSendFile(). On alpha.224 to alpha.378 every 200 "
                "from it leaks a REST slot until reboot (TASK-1172): expect FAIL there, and reboot "
                "afterwards. On alpha.379 or newer expect PASS. It streams only while its file exists." % p)
        if p.startswith("/api/v2/health"):
            say("WARNING: /api/v2/health writes /.health to LittleFS on every call.")
    say("storm on %s: arms %s x %.0fs, paths %d, ws subscribers %d, seed %d"
        % (opts.host, opts.worker_list, opts.duration, len(opts.path_list), opts.ws_subs, opts.seed))
    base = device_snapshot(opts, log, "storm_before")
    if base is None:
        say("VERDICT: INCONCLUSIVE - device/info unreachable before the storm")
        return "INCONCLUSIVE", {"reason": "no baseline"}
    # With a password every gate-balance GET gets 401, which would read as a leak.
    locked = [p for p in GATE_PATHS if auth_required(opts, p)]
    if locked:
        msg = "HTTP password set: %s answer(s) 401, clear it for this test" % ", ".join(locked)
        log.write({"type": "summary", "mode": "storm", "verdict": "INCONCLUSIVE", "fail": [], "warn": [],
                   "inconclusive": [msg], "arms": []})
        say("INCONCLUSIVE: " + msg)
        say("VERDICT: INCONCLUSIVE")
        return "INCONCLUSIVE", {"reason": msg}
    ctx = context_snapshot(opts, log)
    say("baseline: fw=%s boot=%s uptime=%s lastreset=%s heap=%s maxblk=%s pcbs=%s/%s mqtt=%s psram=%s "
        "ble_enable=%s risk_ack=%s"
        % (base["fwversion"], base["bootcount"], base["uptime"], base["lastreset"], base["freeheap"],
           base["maxfreeblock"], base["hd_tcp_active_pcbs"], opts.pcb_pool, base["mqttconnected"],
           ctx.get("psram", base.get("psram_found")), ctx.get("ble_enable"), ctx.get("risk_ack")))
    ws = None
    arms = []
    interrupted = False
    final_gb = None
    try:
        if opts.ws_subs > 0:
            ws = WsLeg(opts.host, opts.ws_subs)
            ws.start()
            time.sleep(3.0)
            say("/ws leg: %d subscribers %s" % (opts.ws_subs, ws.stats()))
        for k, w in enumerate(opts.worker_list, 1):
            s, interrupted = run_arm(opts, log, "arm%d_w%d" % (k, w), w, ws)
            arms.append(s)
            if interrupted:
                break
    except KeyboardInterrupt:
        interrupted = True
    finally:
        if ws:
            ws.stop()
            say("/ws leg stopped: %s" % ws.stats())
    time.sleep(min(opts.quiet, 3.0))
    final_gb = gate_balance(opts, log, "final")
    final = device_snapshot(opts, log, "storm_after")
    cmp_ = compare_snaps(base, final, opts.heap_noise)
    fail = ["%s: %s" % (a["arm"], f) for a in arms for f in a["fail"]]
    warn = ["%s: %s" % (a["arm"], w) for a in arms for w in a["warn"]]
    inconclusive = ["%s: %s" % (a["arm"], x) for a in arms for x in a["inconclusive"]]
    if final is None:
        fail.append("device/info did not answer 200 at the end (see its snapshot line)")
    if cmp_["reboot"] == "FAIL":
        fail.append("reboot across the run: bootcount %s uptime_min %s" % (cmp_["bootcount"], cmp_["uptime_min"]))
    if final_gb["verdict"] == "FAIL":
        fail.append("final gate balance failed")
    elif final_gb["verdict"] == "WARN":
        warn.append("final gate-balance pair check %s" % final_gb["pair_result"])
    if cmp_["pcbs"] == "WARN":
        warn.append("hd_tcp_active_pcbs %s -> %s over the run"
                    % (base.get("hd_tcp_active_pcbs"), final.get("hd_tcp_active_pcbs")))
    if interrupted:
        inconclusive.append("interrupted")
    verdict = "FAIL" if fail else ("INCONCLUSIVE" if inconclusive else "PASS")
    summary = {"type": "summary", "mode": "storm", "verdict": verdict, "fail": fail, "warn": warn,
               "inconclusive": inconclusive, "arms": [a["arm"] for a in arms], "context": ctx,
               "before": base, "after": final, "compare": cmp_,
               "final_gate_balance": {"verdict": final_gb["verdict"], "pair_result": final_gb["pair_result"]},
               "ws_final": ws.stats() if ws else None}
    log.write(summary)
    say("\n== STORM RESULT ==")
    say("bootcount %s  uptime_min %s  lastreset %s  d_freeheap %s  d_maxfreeblock %s  pcbs %s->%s"
        % (cmp_.get("bootcount"), cmp_.get("uptime_min"), cmp_.get("lastreset"), cmp_.get("d_freeheap"),
           cmp_.get("d_maxfreeblock"), base.get("hd_tcp_active_pcbs"),
           (final or {}).get("hd_tcp_active_pcbs")))
    for w in warn:
        say("WARN: " + w)
    for x in inconclusive:
        say("INCONCLUSIVE: " + x)
    for f in fail:
        say("FAIL: " + f)
    say("VERDICT: " + verdict)
    return verdict, summary


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def build_parser():
    ap = argparse.ArgumentParser(
        description="TASK-1124 request storm and upload-abort tool (2.0.0 web stack).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="The module docstring lists what the firmware does and where (file:line).")
    ap.add_argument("--host", required=True,
                    help="device IP or host[:port]. Required: a storm is never aimed at a default host")
    ap.add_argument("--upload-abort", type=int, default=0, metavar="N",
                    help="run N aborted uploads instead of the storm")
    ap.add_argument("--abort-mode", choices=("fin", "rst", "stall", "mixed"), default="mixed",
                    help="uploads: mixed rotates fin, rst, stall. storm: mixed picks fin or rst; "
                         "stall only when asked (default mixed)")
    g = ap.add_argument_group("storm")
    g.add_argument("--workers", default="2", help="comma list, one arm per value, e.g. 2,4,6,8 (default 2)")
    g.add_argument("--duration", type=float, default=45.0, help="seconds per arm (default 45)")
    g.add_argument("--abort-ratio", type=float, default=0.33, help="share of storm requests aborted (0.33)")
    g.add_argument("--abort-after", type=int, default=1024,
                   help="body bytes read before a storm abort (default 1024)")
    g.add_argument("--paths", default=",".join(DEFAULT_PATHS), help="comma list of GET paths")
    g.add_argument("--conditional", action="store_true",
                   help="send If-None-Match with the last ETag per path, like a browser F5")
    g.add_argument("--ws-subs", type=int, default=2,
                   help="persistent /ws subscribers for the whole run (default 2; the device takes 3)")
    g.add_argument("--probe-interval", type=float, default=2.0,
                   help="seconds between device/info probes during an arm; 0 disables (default 2)")
    g.add_argument("--quiet", type=float, default=10.0,
                   help="seconds of rest after an arm before the gate-balance check (default 10)")
    g.add_argument("--gate-reps", type=int, default=5, help="sequential GETs per gate-balance path (5)")
    g.add_argument("--pcb-pool", type=int, default=16, help="lwIP TCP PCB pool size (16)")
    u = ap.add_argument_group("upload abort")
    u.add_argument("--upload-name", default="t1124_abort.bin", help="file name in / (t1124_abort.bin)")
    u.add_argument("--upload-size", type=int, default=16384, help="declared content bytes (16384)")
    u.add_argument("--upload-send", type=int, default=4096,
                   help="content bytes sent before an abort (4096); a fin or stall abort should store all of them")
    u.add_argument("--pre-abort-delay", type=float, default=0.5,
                   help="seconds between the last byte and a fin/rst abort (0.5)")
    u.add_argument("--stall-seconds", type=float, default=8.0,
                   help="upload stall: seconds to wait for the device to end it (8). "
                        "storm stall: seconds a reader stops reading")
    u.add_argument("--readback-wait", type=float, default=0.5, help="seconds from abort to readback (0.5)")
    u.add_argument("--settle-attempts", type=int, default=4, help="readback attempts (4)")
    u.add_argument("--settle-wait", type=float, default=0.75, help="seconds between readback attempts (0.75)")
    c = ap.add_argument_group("common")
    c.add_argument("--heap-noise", type=int, default=4096, help="free-heap drop still counted as noise (4096)")
    c.add_argument("--connect-timeout", type=float, default=5.0)
    c.add_argument("--read-timeout", type=float, default=10.0)
    c.add_argument("--seed", type=int, default=1124)
    c.add_argument("--log", default=None, help="NDJSON log path")
    c.add_argument("--note", default="", help="free text stored in the log, e.g. 'BLE off, emulator on'")
    return ap


def parse_opts(argv):
    ap = build_parser()
    opts = ap.parse_args(argv)
    try:
        opts.worker_list = [int(x) for x in opts.workers.split(",") if x.strip()]
    except ValueError:
        ap.error("--workers takes a comma list of integers")
    if not opts.worker_list or min(opts.worker_list) < 1:
        ap.error("--workers needs at least one value >= 1")
    opts.path_list = [p.strip() for p in opts.paths.split(",") if p.strip()]
    if not opts.path_list or any(not p.startswith("/") for p in opts.path_list):
        ap.error("--paths takes absolute paths")
    if not 0.0 <= opts.abort_ratio <= 1.0:
        ap.error("--abort-ratio must be between 0 and 1")
    if opts.upload_abort < 0:
        ap.error("--upload-abort must be >= 0")
    if opts.upload_abort and not MULTIPART_PIECE < opts.upload_send < opts.upload_size:
        # above one full parser buffer, so the handler runs whatever the segment sizes
        ap.error("--upload-send must be above %d (one parser buffer) and below --upload-size" % MULTIPART_PIECE)
    if not opts.upload_abort and opts.ws_subs > 0:
        try:
            import websocket  # noqa: F401  (websocket-client, used by test_ws_liveload)
        except ImportError:
            ap.error("--ws-subs %d needs websocket-client (pip install websocket-client); "
                     "pass --ws-subs 0 to run without the /ws leg" % opts.ws_subs)
    return opts


def main(argv=None):
    opts = parse_opts(argv)
    mode = "upload_abort" if opts.upload_abort else "storm"
    log = NdjsonLog(opts.log or default_log_path(opts.host, mode))
    say("log: %s" % log.path)
    log.write({"type": "run_start", "tool": "refresh_storm", "task": "TASK-1124", "mode": mode,
               "host": opts.host, "note": opts.note,
               "args": {k: v for k, v in vars(opts).items() if k not in ("path_list", "worker_list")}})
    try:
        if opts.upload_abort:
            verdict, _ = run_upload_abort(opts, log)
        else:
            verdict, _ = run_storm(opts, log)
    finally:
        log.close()
    return {"PASS": 0, "FAIL": 1}.get(verdict, 2)


if __name__ == "__main__":
    sys.exit(main())
