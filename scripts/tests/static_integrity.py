#!/usr/bin/env python3
"""TASK-1162 download integrity probe: does the OTGW32 serve whole files, and
if not, how does a response end?

It makes strictly sequential GETs over raw sockets, one connection at a time,
each with "Connection: close". The device caps concurrent requests at 2
(ADR-165), and this tool never holds more than one socket. Per request it
records the HTTP status, the Content-Length header, the bytes received, how the
connection ended and the elapsed time.

Close types:
  complete      The body framing finished: all Content-Length bytes, or the
                terminating 0-size chunk of a chunked body.
  FIN-short     The device closed the connection (FIN) before that.
  RST           The connection was reset or aborted mid-response. The detail
                column names the exception. On Windows an incoming reset can
                discard bytes not yet read, so body_bytes is a lower bound here.
  timeout       No byte for --idle-timeout seconds (detail "idle"), or
                --max-time ran out (detail "max-time").
  connect-fail  The TCP connect failed: refused, unreachable or timed out.

Verdicts:
  ok             Complete and status 200. For a static file the Content-Length
                 and the bytes received also equal the size in the listing.
  gate           Status 503: the device refused on purpose. The static-file
                 gate (webSendFile(), webServerCompat.h:346-357) or the REST
                 gate (processAPI(), restAPI.ino:2707-2714). Not a failure. A
                 sequential client still meets it: the file gate frees its slot
                 only on disconnect (webServerCompat.h:358-359, through
                 webArmSlotRelease() at :326-335), and both caps drop to 1
                 below maxfreeblock 16000 (restAPI.ino:61, :88). A REST target
                 that is gate in every batch, while telemetry answers in none,
                 points at a wedged REST gate, not at backpressure.
  size-changed   Complete, the size differs from the listing read at batch
                 start but equals a listing re-read right after. The file was
                 rewritten in between, for example by a settings flush. Not a
                 failure.
  short          A status line arrived but the body did not finish.
  size-mismatch  Complete, but the size matches neither listing.
  http-error     Any other status: 404, 401, 500 and so on.
  no-response    The connection ended before a status line and headers.
  no-connect     The close type is connect-fail.

Every batch starts with telemetry from /api/v2/device/info (sendDeviceInfoV2(),
restAPI.ino:3056-3321; the keys sit at :3114 and :3202-3264) and the file
listing from /api/listfiles (apilistfiles(), FSexplorer.ino:466-570). The
listing is a JSON array of {name, size, type} objects plus one trailing summary
object, and it is re-read every batch. When the device does not answer, the run
goes on with the last good sizes. /api/listfiles is registered as deprecated
(FSexplorer.ino:301) and skips the REST gate. /api/v2/filesystem/files
(restAPI.ino:663) runs the same apilistfiles() behind that gate.
apilistfiles() answers 500 below 4096 B free heap (FSexplorer.ino:472-475).

Reboot markers, compared with the last telemetry that did arrive:
- bootcount changed. setup() counts boots in /reboot_count.txt
  (OTGW-firmware.ino:509, helperStuff.ino:137-169). The file is not part of the
  LittleFS image, so an FS image flash restarts the count: a change in either
  direction counts.
- uptime went back.
- reboot_log.txt changed size. setup() rewrites it at every boot
  (OTGW-firmware.ino:510, helperStuff.ino:296-375). It is capped at about 20
  lines (helperStuff.ino:300), so an unchanged size proves nothing.

Reading the close gap (the time from the last byte to the end of the request):
- FIN-short with a gap under 1 s: the response ran out of data early. That
  points at an early end of body in the application (ESPAsyncWebServer 3.11.0,
  WebResponses.cpp:484-486).
- FIN-short with a gap of 4-7 s: that matches the AsyncTCP ack timeout. After
  5 s without an ACK (CONFIG_ASYNC_TCP_MAX_ACK_TIME=5000, platformio.ini:69)
  AsyncTCP calls the request's timeout handler, which closes the connection
  (AsyncTCP 3.4.10 AsyncTCP.cpp:1094-1103, WebRequest.cpp:271-275).
- timeout: the device went silent. The gap equals --idle-timeout by
  construction, so keep --idle-timeout above 5 s (default 10 s) or an ack
  timeout close is reported as a stall.
wire_bytes is the head plus the raw body bytes. When one cut point repeats,
compare it with the first TCP flight: 3 x 1436 B = 4308 B. The MSS is 1436
(CONFIG_LWIP_TCP_MSS in the esp32s3 qio_qspi sdkconfig.h of the PlatformIO
Arduino libs) and lwIP starts cwnd at 4380 B for that MSS (tcp_in.c:69 in the
framework-espidf package).

Default targets: /settings.ini, /sat-slider.js (the control: its head plus
1741 B fits in the first flight), /index.html, /v2-bundle.css and /index.js,
plus the REST controls /api/v2/device/info and /api/v2/settings.
"/index.html" serves /v2.html when the v2 UI is the default
(FSexplorer.ino:160-164). Asset ETags read "<fshash>-<served path>"
(FSexplorer.ino:109), so the expected size is looked up for the file actually
served.

Assumes no admin password. With one set, /settings.ini, /index.html and
/api/v2/settings answer 401 (FSexplorer.ino:600-608, :155; restAPI.ino:386-387),
and so does /v2.html (FSexplorer.ino:211) when it is a target.

Usage:
  python scripts/tests/static_integrity.py --host 192.168.88.61 --batches 100 --csv cell-A
  python scripts/tests/static_integrity.py --host 192.168.88.61 --minutes 15 --batches 0 --label C --csv cell-C

The run stops at whichever of --batches and --minutes it reaches first. The
default is 100 batches, so a timed run needs --batches 0.

--csv PREFIX writes PREFIX-requests.csv and PREFIX-batches.csv, flushed per
row. The self-test is tests/test_static_integrity.py.

Exit code: 0 when every request was ok, gate or size-changed and no reboot was
seen, 1 otherwise.
"""
import argparse
import csv
import json
import os
import re
import socket
import sys
import time
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # scripts/ for _secrets
try:
    import _secrets
except Exception:
    _secrets = None

DEFAULT_TARGETS = [
    "/settings.ini",
    "/sat-slider.js",
    "/index.html",
    "/v2-bundle.css",
    "/index.js",
    "/api/v2/device/info",
    "/api/v2/settings",
]
LISTING_PATH = "/api/listfiles"
TELEMETRY_PATH = "/api/v2/device/info"
TELEMETRY_KEYS = ("fwversion", "freeheap", "maxfreeblock", "hd_min_max_block",
                  "hd_webfile_503", "hd_rest_503", "hd_tcp_active_pcbs",
                  "bootcount", "uptime")
REBOOT_LOG = "reboot_log.txt"

CLOSE_TYPES = ("complete", "FIN-short", "RST", "timeout", "connect-fail")
VERDICTS = ("ok", "gate", "size-changed", "short", "size-mismatch", "http-error",
            "no-response", "no-connect")
PASS_VERDICTS = ("ok", "gate", "size-changed")

REQUEST_FIELDS = ["ts", "label", "batch", "path", "served", "status",
                  "content_length", "expected", "body_bytes", "head_bytes",
                  "wire_bytes", "framing", "close", "detail", "connect_ms",
                  "ttfb_ms", "elapsed_ms", "close_gap_ms", "verdict"]
BATCH_FIELDS = (["ts", "label", "batch", "tel_status", "tel_close"]
                + list(TELEMETRY_KEYS)
                + ["list_status", "list_close", "list_truncated",
                   "reboot_log_size", "reboot"])


def utc_now():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def build_request(host, port, path):
    host_hdr = host if port == 80 else "%s:%d" % (host, port)
    return ("GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: otgw-static-integrity/1\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "\r\n" % (path, host_hdr)).encode("latin-1")


class ChunkedDecoder:
    """Incremental decoder for an HTTP/1.1 chunked body (RFC 9112 section 7.1).

    feed() takes raw bytes split at any point and counts payload bytes. done
    turns true after the last chunk and the empty line that closes the trailer
    section. The device writes 4-hex-digit sizes and ends with "0000\\r\\n\\r\\n".
    A malformed size line or a missing CRLF after the data sets error and stops
    the decoder, so such a body can never count as complete.
    """

    _HEX = re.compile(rb"^[0-9A-Fa-f]+$")

    def __init__(self, keep=False):
        self._buf = b""
        self._state = "size"  # size, data, data-crlf, trailer, done, error
        self._left = 0
        self.payload = 0
        self.error = ""
        self.data = bytearray() if keep else None

    @property
    def done(self):
        return self._state == "done"

    def _fail(self, why):
        self._state = "error"
        self.error = why

    def feed(self, data):
        if self._state in ("done", "error"):
            return
        self._buf += data
        while True:
            if self._state == "size":
                i = self._buf.find(b"\r\n")
                if i < 0:
                    return
                size_txt = self._buf[:i].split(b";", 1)[0].strip()
                self._buf = self._buf[i + 2:]
                if not self._HEX.match(size_txt):
                    self._fail("bad-chunk-size")
                    return
                size = int(size_txt, 16)
                if size == 0:
                    self._state = "trailer"
                else:
                    self._left = size
                    self._state = "data"
            elif self._state == "data":
                if not self._buf:
                    return
                n = min(self._left, len(self._buf))
                if self.data is not None:
                    self.data += self._buf[:n]
                self.payload += n
                self._left -= n
                self._buf = self._buf[n:]
                if self._left == 0:
                    self._state = "data-crlf"
            elif self._state == "data-crlf":
                if len(self._buf) < 2:
                    return
                if self._buf[:2] != b"\r\n":
                    self._fail("bad-chunk-crlf")
                    return
                self._buf = self._buf[2:]
                self._state = "size"
            elif self._state == "trailer":
                i = self._buf.find(b"\r\n")
                if i < 0:
                    return
                line = self._buf[:i]
                self._buf = self._buf[i + 2:]
                if not line:
                    self._state = "done"
                    return
            else:
                return


def parse_head(text):
    """Status code (or None) and a lower-case header dict from the response head."""
    lines = text.split("\r\n")
    m = re.match(r"HTTP/\d\.\d\s+(\d{3})", lines[0])
    status = int(m.group(1)) if m else None
    headers = {}
    for line in lines[1:]:
        name, sep, value = line.partition(":")
        if sep:
            headers[name.strip().lower()] = value.strip()
    return status, headers


def pick_framing(status, headers):
    """How the body ends: (framing, content_length, problem).

    framing is "length", "chunked", "none" (no body) or "eof" (read to close,
    which cannot detect a cut; the device always sends one of the others).
    """
    if status is None:
        return "eof", None, "bad-status-line"
    if 100 <= status < 200 or status in (204, 304):
        return "none", None, ""
    if "chunked" in headers.get("transfer-encoding", "").lower():
        return "chunked", None, ""
    cl = headers.get("content-length")
    if cl is None:
        return "eof", None, "no-length"
    if not cl.strip().isdigit():
        return "eof", None, "bad-content-length"
    return "length", int(cl), ""


def _err_name(exc):
    if isinstance(exc, socket.timeout):
        return "timeout"
    num = getattr(exc, "winerror", None) or getattr(exc, "errno", None)
    return "%s[%s]" % (type(exc).__name__, num) if num else type(exc).__name__


def fetch(host, port, path, connect_timeout=5.0, idle_timeout=10.0, max_time=60.0,
          keep_body=False):
    """One GET on a fresh socket. Returns a dict with the fields of REQUEST_FIELDS
    that the transport decides, plus "etag" and, with keep_body, "body" (the
    decoded payload)."""
    r = {"path": path, "status": None, "content_length": None, "framing": "",
         "head_bytes": 0, "body_bytes": 0, "wire_bytes": 0, "close": "",
         "detail": "", "connect_ms": None, "ttfb_ms": None, "elapsed_ms": None,
         "close_gap_ms": None, "etag": "", "body": b""}
    details = []
    t0 = time.perf_counter()

    def ms(t):
        return round((t - t0) * 1000.0, 1)

    try:
        sock = socket.create_connection((host, port), timeout=connect_timeout)
    except OSError as exc:
        r["close"], r["detail"] = "connect-fail", _err_name(exc)
        r["elapsed_ms"] = ms(time.perf_counter())
        return r

    t_last = time.perf_counter()  # time of the last byte, or of the connect
    r["connect_ms"] = ms(t_last)
    deadline = t0 + max_time
    head = b""
    head_done = False
    framing = ""
    decoder = None
    body = bytearray()
    try:
        sock.sendall(build_request(host, port, path))
        while True:
            left = deadline - time.perf_counter()
            if left <= 0:
                r["close"] = "timeout"
                details.append("max-time")
                break
            sock.settimeout(min(idle_timeout, left))
            try:
                data = sock.recv(65536)
            except socket.timeout:
                r["close"] = "timeout"
                details.append("idle" if idle_timeout < left else "max-time")
                break
            now = time.perf_counter()
            if not data:
                # FIN. Only a body without framing ends legitimately this way.
                r["close"] = "complete" if head_done and framing == "eof" else "FIN-short"
                break
            if r["ttfb_ms"] is None:
                r["ttfb_ms"] = ms(now)
            t_last = now
            r["wire_bytes"] += len(data)
            if not head_done:
                head += data
                end = head.find(b"\r\n\r\n")
                if end < 0:
                    continue
                data = head[end + 4:]
                r["head_bytes"] = end + 4
                status, headers = parse_head(head[:end].decode("latin-1"))
                head_done = True
                r["status"] = status
                r["etag"] = headers.get("etag", "")
                framing, r["content_length"], problem = pick_framing(status, headers)
                r["framing"] = framing
                if problem:
                    details.append(problem)
                if framing == "chunked":
                    decoder = ChunkedDecoder(keep=keep_body)
            if framing == "chunked":
                decoder.feed(data)
                r["body_bytes"] = decoder.payload
                finished = decoder.done
            else:
                r["body_bytes"] += len(data)
                if keep_body:
                    body += data
                finished = (framing == "none" or
                            (framing == "length" and r["body_bytes"] >= r["content_length"]))
            if finished:
                r["close"] = "complete"
                break
    except socket.timeout:
        # Only sendall() gets here: recv timeouts are handled inside the loop.
        r["close"] = "timeout"
        details.append("send")
    except OSError as exc:
        r["close"] = "RST"
        details.append(_err_name(exc))
    finally:
        sock.close()

    if decoder is not None and decoder.error:
        details.append(decoder.error)
    end_t = time.perf_counter()
    r["elapsed_ms"] = ms(end_t)
    r["close_gap_ms"] = round((end_t - t_last) * 1000.0, 1)
    r["detail"] = ";".join(details)
    if keep_body:
        r["body"] = bytes(decoder.data) if decoder is not None else bytes(body)
    return r


def classify(r, expected=None):
    """Verdict for one fetch() result. expected is the listed size of the served
    file, or None for a REST target or when no listing is known."""
    if r["close"] == "connect-fail":
        return "no-connect"
    if r["status"] is None:
        return "no-response"
    if r["close"] != "complete":
        return "short"
    if r["status"] == 503:
        return "gate"
    if r["status"] != 200:
        return "http-error"
    if r["framing"] == "length" and r["body_bytes"] != r["content_length"]:
        return "size-mismatch"
    # From here a Content-Length, when there is one, equals body_bytes, so this
    # one comparison covers both the header and the body.
    if expected is not None:
        if r["body_bytes"] != expected:
            return "size-mismatch"
    return "ok"


def served_path(url_path, etag):
    """Path of the file the device actually served. The asset ETag is
    "<fshash>-<path>" (FSexplorer.ino:109). Without one, the URL path."""
    v = (etag or "").strip()
    if v.startswith("W/"):
        v = v[2:]
    v = v.strip('"')
    i = v.find("-/")
    if i >= 0:
        return v[i + 1:]
    return url_path.split("?", 1)[0]


def parse_listing(body):
    """({name: size} for every file, summary dict) from an /api/listfiles body."""
    doc = json.loads(body.decode("utf-8"))
    if not isinstance(doc, list):
        raise ValueError("listing is not a JSON array")
    sizes, summary = {}, {}
    for entry in doc:
        if not isinstance(entry, dict):
            continue
        if "name" in entry:
            if entry.get("type") == "file":
                sizes[str(entry["name"])] = int(entry.get("size", -1))
        else:
            summary = entry
    return sizes, summary


def read_listing(r):
    """parse_listing() for a fetch() result, or None when it did not arrive whole."""
    if r["close"] != "complete" or r["status"] != 200:
        return None
    try:
        return parse_listing(r["body"])
    except (ValueError, TypeError):
        return None


def parse_telemetry(body):
    """The TELEMETRY_KEYS values from a /api/v2/device/info body."""
    doc = json.loads(body.decode("utf-8"))
    dev = doc.get("device") if isinstance(doc, dict) else None
    if not isinstance(dev, dict):
        raise ValueError("no device object")
    return {k: dev.get(k) for k in TELEMETRY_KEYS}


_UPTIME = re.compile(r"^\s*(\d+)\(d\)-(\d+):(\d+)")


def uptime_minutes(text):
    """Minutes from the device uptime string "D(d)-HH:MM(H:m)" (helperStuff.ino:653-664)."""
    m = _UPTIME.match(text or "")
    if not m:
        return None
    return int(m.group(1)) * 1440 + int(m.group(2)) * 60 + int(m.group(3))


def detect_reboot(prev, cur):
    """Reason text when cur shows a reboot since prev, else "". Both are dicts
    with bootcount, uptime and reboot_log_size; a missing value is skipped."""
    if not prev or not cur:
        return ""
    reasons = []
    b0, b1 = prev.get("bootcount"), cur.get("bootcount")
    if b0 is not None and b1 is not None and b0 != b1:
        reasons.append("bootcount %s->%s" % (b0, b1))
    u0, u1 = uptime_minutes(prev.get("uptime")), uptime_minutes(cur.get("uptime"))
    if u0 is not None and u1 is not None and u1 < u0:
        reasons.append("uptime %s->%s" % (prev.get("uptime"), cur.get("uptime")))
    s0, s1 = prev.get("reboot_log_size"), cur.get("reboot_log_size")
    if s0 is not None and s1 is not None and s0 != s1:
        reasons.append("%s %s->%s B" % (REBOOT_LOG, s0, s1))
    return "; ".join(reasons)


def gap_bucket(gap_ms):
    """Coarse close-gap label for the signature table (see the module docstring)."""
    if gap_ms is None or gap_ms == "":
        return "-"
    s = float(gap_ms) / 1000.0
    if s < 1.0:
        return "gap<1s"
    if s < 4.0:
        return "gap1-4s"
    if s < 7.0:
        return "gap4-7s"
    return "gap>=7s"


def _s(v):
    return "-" if v is None or v == "" else str(v)


def _ms(v):
    return "-" if v is None or v == "" else "%sms" % v


def format_request(row):
    return ("%s b%03d %-22s %4s cl=%-7s got=%-7s exp=%-7s %-12s %-13s ttfb=%s total=%s gap=%s%s"
            % (row["ts"], row["batch"], row["path"], _s(row["status"]),
               _s(row["content_length"]), row["body_bytes"], _s(row["expected"]),
               row["close"], row["verdict"], _ms(row["ttfb_ms"]), _ms(row["elapsed_ms"]),
               _ms(row["close_gap_ms"]),
               (" [" + row["detail"] + "]") if row["detail"] else ""))


def format_batch(row):
    tel = " ".join("%s=%s" % (k, _s(row.get(k))) for k in TELEMETRY_KEYS if k != "fwversion")
    return ("%s batch %d telemetry %s/%s %s | listing %s/%s %s=%sB"
            % (row["ts"], row["batch"], _s(row["tel_status"]), row["tel_close"], tel,
               _s(row["list_status"]), row["list_close"], REBOOT_LOG,
               _s(row.get("reboot_log_size"))))


def summarize(rows, batch_rows, reboots, stale_listings, out):
    """Print the per-target tables, the failure signatures and the telemetry range.
    Returns the number of requests with a failing verdict."""
    paths = []
    for row in rows:
        if row["path"] not in paths:
            paths.append(row["path"])
    short = {"ok": "ok", "gate": "gate", "size-changed": "chg", "short": "short",
             "size-mismatch": "size", "http-error": "http", "no-response": "noresp",
             "no-connect": "noconn"}
    out("")
    out("=== summary: %d requests in %d batches ===" % (len(rows), len(batch_rows)))
    out("%-22s %5s " % ("path", "n") + " ".join("%6s" % short[v] for v in VERDICTS) + "  max_ms")
    for p in paths:
        mine = [r for r in rows if r["path"] == p]
        counts = " ".join("%6d" % sum(1 for r in mine if r["verdict"] == v) for v in VERDICTS)
        worst = max((r["elapsed_ms"] or 0) for r in mine)
        out("%-22s %5d %s  %.0f" % (p, len(mine), counts, worst))
    out("  (chg=size-changed size=size-mismatch http=http-error noresp=no-response noconn=no-connect)")
    out("%-22s %5s " % ("close types", "n") + " ".join("%12s" % c for c in CLOSE_TYPES))
    for p in paths:
        mine = [r for r in rows if r["path"] == p]
        out("%-22s %5d " % (p, len(mine))
            + " ".join("%12d" % sum(1 for r in mine if r["close"] == c) for c in CLOSE_TYPES))

    failures = [r for r in rows if r["verdict"] not in PASS_VERDICTS]
    if failures:
        groups = {}
        for r in failures:
            key = (r["path"], r["verdict"], r["close"], _s(r["status"]), r["wire_bytes"],
                   r["body_bytes"], _s(r["content_length"]), _s(r["expected"]),
                   gap_bucket(r["close_gap_ms"]), r["detail"])
            groups[key] = groups.get(key, 0) + 1
        out("failure signatures (count path verdict close status wire body/content-length exp gap detail):")
        for key, n in sorted(groups.items(), key=lambda kv: -kv[1]):
            p, v, c, st, wire, body, cl, exp, gap, det = key
            out("  %4dx %-22s %-13s %-12s %4s wire=%s body=%s/%s exp=%s %s %s"
                % (n, p, v, c, st, wire, body, cl, exp, gap, det))
    changed = sum(1 for r in rows if r["verdict"] == "size-changed")
    if changed:
        out("size-changed: %d responses matched a listing re-read after the request, "
            "so the file changed between the two reads" % changed)

    answered = [b for b in batch_rows if b.get("freeheap") is not None]
    out("telemetry: %d of %d batches answered" % (len(answered), len(batch_rows)))
    if answered:
        def col(k):
            return [b[k] for b in answered if isinstance(b.get(k), (int, float))]
        for k in ("freeheap", "maxfreeblock", "hd_min_max_block"):
            vals = col(k)
            if vals:
                out("  %-20s min %d, max %d" % (k, min(vals), max(vals)))
        vals = col("hd_tcp_active_pcbs")
        if vals:
            out("  %-20s max %d" % ("hd_tcp_active_pcbs", max(vals)))
        for k in ("hd_webfile_503", "hd_rest_503"):
            vals = col(k)
            if vals:
                out("  %-20s %d -> %d (device counter; includes this run's own 503s)"
                    % (k, vals[0], vals[-1]))
    unverified = sum(1 for r in rows if r["verdict"] == "ok" and r["expected"] is None
                     and not r["path"].startswith("/api/"))
    out("listing: %d batches used a stale listing; %d static responses had no expected size"
        % (stale_listings, unverified))
    out("reboots seen: %d" % reboots)
    if failures or reboots:
        out("VERDICT: FAILURES (%d of %d requests, %d reboots)" % (len(failures), len(rows), reboots))
    else:
        out("VERDICT: CLEAN (%d requests)" % len(rows))
    return len(failures)


class _CsvSink:
    """Writes one CSV file, flushed per row so an interrupted run keeps its data."""

    def __init__(self, path, fields):
        self._fh = open(path, "w", newline="", encoding="utf-8")
        self._w = csv.DictWriter(self._fh, fieldnames=fields, extrasaction="ignore")
        self._w.writeheader()
        self._fh.flush()

    def write(self, row):
        self._w.writerow({k: ("" if v is None else v) for k, v in row.items()})
        self._fh.flush()

    def close(self):
        self._fh.close()


def run(args, out=print):
    host, port = args.host, args.port
    timeouts = dict(connect_timeout=args.connect_timeout, idle_timeout=args.idle_timeout,
                    max_time=args.max_time)
    sinks = None
    if args.csv:
        sinks = (_CsvSink(args.csv + "-requests.csv", REQUEST_FIELDS),
                 _CsvSink(args.csv + "-batches.csv", BATCH_FIELDS))
    out("# static_integrity host=%s:%d targets=%s batches=%s minutes=%s idle=%.1fs max=%.1fs gap=%.2fs label=%s"
        % (host, port, ",".join(args.targets), args.batches or "unlimited", args.minutes or "-",
           args.idle_timeout, args.max_time, args.gap, args.label or "-"))

    rows, batch_rows = [], []
    sizes = {}
    have_listing = False
    last_known = {}
    reboots = 0
    stale_listings = 0
    t_start = time.perf_counter()
    batch = 0

    def pause():
        if args.gap > 0:
            time.sleep(args.gap)

    try:
        while True:
            if args.batches and batch >= args.batches:
                break
            if args.minutes and time.perf_counter() - t_start >= args.minutes * 60.0:
                break
            batch += 1
            brow = {"ts": utc_now(), "label": args.label, "batch": batch}

            tel = fetch(host, port, TELEMETRY_PATH, keep_body=True, **timeouts)
            brow["tel_status"], brow["tel_close"] = tel["status"], tel["close"]
            if tel["close"] == "complete" and tel["status"] == 200:
                try:
                    brow.update(parse_telemetry(tel["body"]))
                except ValueError:
                    out("%s batch %d: device/info arrived whole but is not valid JSON"
                        % (brow["ts"], batch))
            pause()

            lst = fetch(host, port, LISTING_PATH, keep_body=True, **timeouts)
            brow["list_status"], brow["list_close"] = lst["status"], lst["close"]
            listing = read_listing(lst)
            if listing is not None:
                sizes, summary = listing
                have_listing = True
                brow["list_truncated"] = summary.get("truncated")
                brow["reboot_log_size"] = sizes.get(REBOOT_LOG)
            elif have_listing:
                stale_listings += 1
            pause()

            current = {k: brow.get(k) for k in ("bootcount", "uptime", "reboot_log_size")
                       if brow.get(k) is not None}
            reason = detect_reboot(last_known, current)
            last_known.update(current)
            brow["reboot"] = reason
            if reason:
                reboots += 1
            batch_rows.append(brow)
            if sinks:
                sinks[1].write(brow)
            out(format_batch(brow))
            if reason:
                out("*** REBOOT before batch %d: %s" % (batch, reason))

            for path in args.targets:
                r = fetch(host, port, path, **timeouts)
                served = served_path(path, r["etag"])
                is_static = not path.startswith("/api/")
                expected = sizes.get(served.lstrip("/")) if is_static else None
                verdict = classify(r, expected)
                if verdict == "size-mismatch" and is_static:
                    # A settings flush can rewrite the file between the batch
                    # listing and this GET. Re-read once before calling it a cut.
                    pause()
                    again = read_listing(fetch(host, port, LISTING_PATH, keep_body=True, **timeouts))
                    if again is not None:
                        sizes = again[0]
                        fresh = sizes.get(served.lstrip("/"))
                        if fresh is not None and fresh != expected and classify(r, fresh) == "ok":
                            verdict = "size-changed"
                row = {k: r.get(k) for k in REQUEST_FIELDS if k in r}
                row.update(ts=utc_now(), label=args.label, batch=batch, served=served,
                           expected=expected, verdict=verdict)
                rows.append(row)
                if sinks:
                    sinks[0].write(row)
                out(format_request(row))
                pause()
    except KeyboardInterrupt:
        out("# interrupted during batch %d" % batch)
    finally:
        if sinks:
            for s in sinks:
                s.close()

    failures = summarize(rows, batch_rows, reboots, stale_listings, out)
    return 1 if (failures or reboots) else 0


def build_parser():
    ap = argparse.ArgumentParser(
        description="Sequential raw-socket download integrity probe (TASK-1162).")
    ap.add_argument("--host", default=None,
                    help="device IP or name (default: device_host from capture-settings.json)")
    ap.add_argument("--port", type=int, default=80)
    ap.add_argument("--batches", type=int, default=100,
                    help="rounds over all targets; 0 = no limit (default 100)")
    ap.add_argument("--minutes", type=float, default=0.0,
                    help="stop after this many minutes; 0 = no limit. The run stops at "
                         "whichever limit it reaches first, so a timed run needs --batches 0")
    ap.add_argument("--targets", default=",".join(DEFAULT_TARGETS),
                    help="comma-separated paths (default: the TASK-1162 set)")
    ap.add_argument("--gap", type=float, default=0.2,
                    help="seconds between requests (default 0.2)")
    ap.add_argument("--idle-timeout", type=float, default=10.0,
                    help="seconds without a byte before a request is a timeout; keep above 5 (default 10)")
    ap.add_argument("--max-time", type=float, default=60.0,
                    help="cap per request in seconds (default 60)")
    ap.add_argument("--connect-timeout", type=float, default=5.0)
    ap.add_argument("--label", default="", help="cell label written to every CSV row")
    ap.add_argument("--csv", default=None,
                    help="write PREFIX-requests.csv and PREFIX-batches.csv")
    return ap


def main(argv=None):
    ap = build_parser()
    args = ap.parse_args(argv)
    if not args.host:
        args.host = _secrets.get("device_host") if _secrets else None
    if not args.host:
        ap.error("no device host: pass --host or set DeviceHost in capture-settings.json")
    args.targets = [t if t.startswith("/") else "/" + t
                    for t in (x.strip() for x in args.targets.split(",")) if t]
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
