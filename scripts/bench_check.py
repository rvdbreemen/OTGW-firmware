#!/usr/bin/env python3
"""Scripted bench check for an OTGW-firmware unit (stdlib only).

  python scripts/bench_check.py --host 192.168.88.61 --checks checks.json [--out DIR]

Runs the checks in a JSON file against the device and writes a timestamped
transcript (every request, response excerpt, matched telnet lines, verdict per
check) plus a one-line summary. Exit 0 when all checks pass, 1 when any fails,
2 on a usage error. See docs/guides/bench-check.md.
Device rules honoured: one HTTP request in flight, a pause between requests,
one retry on 503 / connection error. Never reboots or flashes anything.
"""
import argparse
import datetime
import glob
import http.client
import json
import os
import re
import socket
import sys
import time

HTTP_GAP_S = 0.4
TELNET_SILENT_S = 8.0
EXCERPT = 600


class Log:
    def __init__(self, path):
        self.f = open(path, "w", encoding="utf-8")

    def __call__(self, msg=""):
        line = "%s %s" % (datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], msg)
        print(line, flush=True)
        self.f.write(line + "\n")
        self.f.flush()


def excerpt(s, n=EXCERPT):
    s = s.replace("\r", "").replace("\n", "\\n")
    return s if len(s) <= n else s[:n] + "...(+%d)" % (len(s) - n)


# ---------------------------------------------------------------- HTTP
_last_http = [0.0]


def http_request(ctx, method, path, body=None):
    """One request at a time; retry once on 503 / connection error. Returns (code, text)."""
    log = ctx["log"]
    code, text = None, ""
    for attempt in (1, 2):
        wait = HTTP_GAP_S - (time.time() - _last_http[0])
        if wait > 0:
            time.sleep(wait)
        code, text, err = None, "", None
        try:
            c = http.client.HTTPConnection(ctx["host"], 80, timeout=ctx["http_timeout"])
            hdr = {"Connection": "close"}
            data = None
            if body is not None:
                data = body if isinstance(body, str) else json.dumps(body)
                hdr["Content-Type"] = "application/json"
            log("HTTP >> %s %s%s" % (method, path, (" body=" + excerpt(data, 200)) if data else ""))
            c.request(method, path, data, hdr)
            r = c.getresponse()
            code, text = r.status, r.read().decode("utf-8", "replace")
            c.close()
        except Exception as e:  # connection error / timeout
            err = "%s: %s" % (type(e).__name__, e)
        _last_http[0] = time.time()
        if err:
            log("HTTP !! %s" % err)
        else:
            log("HTTP << %d %s" % (code, excerpt(text)))
        if (err or code == 503) and attempt == 1:
            log("HTTP retry once after %s" % (err or "503"))
            time.sleep(1.5)
            continue
        break
    return code, text


def jpath(obj, parts):
    """Dotted-path lookup; tolerates keys that contain dots (e.g. 'state.debug.sat')."""
    if not parts:
        return True, obj
    if isinstance(obj, dict):
        for i in range(len(parts), 0, -1):
            k = ".".join(parts[:i])
            if k in obj:
                ok, v = jpath(obj[k], parts[i:])
                if ok:
                    return True, v
    elif isinstance(obj, list) and parts[0].isdigit() and int(parts[0]) < len(obj):
        return jpath(obj[int(parts[0])], parts[1:])
    return False, None


def _cmp(fn, v, ref):
    try:
        return bool(fn(v, ref))
    except TypeError:
        return False


def assert_json(doc, rules):
    """Returns a list of (ok, description)."""
    out = []
    for r in rules:
        p = r["path"]
        found, v = jpath(doc, p.split("."))
        if "exists" in r:
            want = bool(r["exists"])
            out.append((found == want, "%s exists=%s (found=%s)" % (p, want, found)))
            continue
        if not found:
            out.append((False, "%s missing" % p))
            continue
        if "equals" in r:
            out.append((v == r["equals"], "%s == %r (got %r)" % (p, r["equals"], v)))
        if "gt" in r:
            out.append((_cmp(lambda a, b: a > b, v, r["gt"]), "%s > %r (got %r)" % (p, r["gt"], v)))
        if "lt" in r:
            out.append((_cmp(lambda a, b: a < b, v, r["lt"]), "%s < %r (got %r)" % (p, r["lt"], v)))
        if "contains" in r:
            out.append((_cmp(lambda a, b: b in a, v, r["contains"]),
                        "%s contains %r (got %s)" % (p, r["contains"], excerpt(json.dumps(v), 120))))
        if "regex" in r:
            out.append((re.search(r["regex"], str(v)) is not None, "%s ~ /%s/ (got %r)" % (p, r["regex"], v)))
    return out


def eval_rest(ctx, c):
    code, text = http_request(ctx, c.get("method", "GET").upper(), c["path"], c.get("body"))
    want = c.get("expect_code", 200)
    res = [(code == want, "HTTP code %s (want %s)" % (code, want))]
    if c.get("json"):
        try:
            res += assert_json(json.loads(text), c["json"])
        except ValueError as e:
            res.append((False, "response is not JSON: %s" % e))
    if c.get("regex"):
        res.append((re.search(c["regex"], text or "") is not None, "text ~ /%s/" % c["regex"]))
    return res


# ---------------------------------------------------------------- telnet
TELNET_IAC = re.compile(rb"\xff[\xfb-\xfe].|\xff.")


class Telnet:
    def __init__(self, ctx):
        self.ctx, self.s, self.buf = ctx, None, b""
        self.last_rx = time.time()
        self.connect()

    def connect(self):
        self.close()
        self.s = socket.create_connection((self.ctx["host"], 23), timeout=5)
        self.s.settimeout(0.5)
        self.buf = b""
        self.last_rx = time.time()
        self.ctx["log"]("TELNET connect %s:23" % self.ctx["host"])

    def close(self):
        if self.s:
            try:
                self.s.close()
            except Exception:
                pass
        self.s = None

    def send(self, keys):
        self.ctx["log"]("TELNET >> keys %r" % keys)
        self.s.sendall(keys.encode())

    def lines(self):
        """Poll once; return complete new lines. Reconnects after 8 s of silence."""
        out = []
        try:
            d = self.s.recv(4096)
            if not d:
                raise ConnectionError("closed by peer")
            self.last_rx = time.time()
            self.buf += TELNET_IAC.sub(b"", d)
        except socket.timeout:
            pass
        except Exception as e:
            self.ctx["log"]("TELNET !! %s (device may be at its telnet client cap; backing off 3 s)" % e)
            time.sleep(3.0)  # do not hammer a device whose client slots are taken
            self.last_rx = 0
        if time.time() - self.last_rx > TELNET_SILENT_S:
            self.ctx["log"]("TELNET silent > %.0fs, reconnecting" % TELNET_SILENT_S)
            try:
                self.connect()
            except Exception as e:
                self.ctx["log"]("TELNET !! reconnect failed: %s" % e)
                self.last_rx = time.time()
        while b"\n" in self.buf:
            ln, self.buf = self.buf.split(b"\n", 1)
            out.append(ln.decode("utf-8", "replace").rstrip("\r"))
        return out


class TelnetLog:
    """Read the telnet stream from a running capture instead of connecting (the device takes one telnet client).
    `path` is a capture run folder (newest live/*/telnet.log, follows the hourly rotation) or a telnet.log file.
    Only lines written after the check starts count. Key presses are not possible through a log."""

    def __init__(self, ctx, path):
        self.ctx, self.path, self.cur, self.pos, self.buf = ctx, path, None, 0, ""
        self._follow(initial=True)
        ctx["log"]("TELNET reading capture log %s" % self.cur)

    def _newest(self):
        if os.path.isfile(self.path):
            return self.path
        cands = sorted(glob.glob(os.path.join(self.path, "live", "*", "telnet.log"))) \
            or sorted(glob.glob(os.path.join(self.path, "telnet.log")))
        return cands[-1] if cands else None

    def _follow(self, initial=False):
        n = self._newest()
        if n and n != self.cur:
            self.cur, self.buf = n, ""
            self.pos = os.path.getsize(n) if initial else 0   # a new hour file: read it from the start

    def send(self, keys):
        self.ctx["log"]("TELNET (capture log) cannot send keys %r: ignored" % keys)

    def close(self):
        pass

    def lines(self):
        self._follow()
        if not self.cur:
            time.sleep(0.5)
            return []
        try:
            with open(self.cur, "r", encoding="utf-8", errors="replace") as f:
                f.seek(self.pos)
                self.buf += f.read()
                self.pos = f.tell()
        except OSError:
            pass
        time.sleep(0.5)
        out = []
        while "\n" in self.buf:
            ln, self.buf = self.buf.split("\n", 1)
            out.append(ln.rstrip("\r"))
        return out


def eval_telnet(ctx, c):
    log = ctx["log"]
    timeout = float(c.get("timeout", 30))
    all_of = [re.compile(p) for p in c.get("all_of", [])]
    any_of = [re.compile(p) for p in c.get("any_of", [])]
    cnt_re = re.compile(c["regex"]) if c.get("regex") else None
    count_min = int(c.get("count_min", 1))
    if not (all_of or any_of or cnt_re):
        return [(False, "telnet check needs all_of, any_of or regex")]
    keys = c.get("keys", "")
    try:
        t = TelnetLog(ctx, ctx["telnet_log"]) if ctx.get("telnet_log") else Telnet(ctx)
    except Exception as e:
        return [(False, "telnet connect failed: %s" % e)]
    if ctx.get("telnet_log") and keys:
        return [(False, "keys %r cannot be sent while reading a capture log (--telnet-log)" % keys)]
    all_hit = [0] * len(all_of)
    any_hit, cnt = 0, 0
    try:
        if keys:
            t.send(keys)
        end = time.time() + timeout
        while time.time() < end:
            for ln in t.lines():
                m = False
                for i, rx in enumerate(all_of):
                    if rx.search(ln):
                        all_hit[i] += 1
                        m = True
                for rx in any_of:
                    if rx.search(ln):
                        any_hit += 1
                        m = True
                if cnt_re and cnt_re.search(ln):
                    cnt += 1
                    m = True
                if m:
                    log("TELNET << MATCH %s" % excerpt(ln, 300))
            ok = (all(all_hit) if all_of else True) \
                and (any_hit >= 1 if any_of else True) \
                and (cnt >= count_min if cnt_re else True)
            if ok:
                break
        if c.get("restore_toggles") and keys:
            t.send(keys)  # debug keys are toggles: same keys again restores them
            time.sleep(0.5)
            t.lines()
    finally:
        t.close()
    res = []
    for i, p in enumerate(c.get("all_of", [])):
        res.append((all_hit[i] > 0, "all_of /%s/ matched %d" % (p, all_hit[i])))
    if any_of:
        res.append((any_hit >= 1, "any_of %s matched %d" % (c["any_of"], any_hit)))
    if cnt_re:
        res.append((cnt >= count_min,
                    "regex /%s/ count %d (want >= %d) within %.0fs" % (c["regex"], cnt, count_min, timeout)))
    if c.get("restore_toggles"):
        res.append((True, "toggles restored (keys %r pressed again)" % keys))
    return res


# ---------------------------------------------------------------- wait / version
def eval_wait(ctx, c):
    if "path" not in c:
        s = float(c.get("seconds", 1))
        ctx["log"]("WAIT sleep %.1fs" % s)
        time.sleep(s)
        return [(True, "slept %.1fs" % s)]
    timeout, interval = float(c.get("timeout", 60)), float(c.get("interval", 3))
    end = time.time() + timeout
    while True:
        code, text = http_request(ctx, "GET", c["path"])
        try:
            last = [(code == 200, "HTTP code %s" % code)] + assert_json(json.loads(text), c["json"])
        except Exception as e:
            last = [(False, "poll error: %s" % e)]
        if all(o for o, _ in last):
            return last
        if time.time() + interval > end:
            return [(False, "timeout %.0fs; last: %s" % (timeout, "; ".join(d for o, d in last if not o)))]
        time.sleep(interval)


def eval_version(ctx, c):
    code, text = http_request(ctx, "GET", "/api/v2/device/info")
    try:
        ver = json.loads(text)["device"]["fwversion"]
    except Exception as e:
        return [(False, "HTTP %s, no device.fwversion: %s" % (code, e))]
    ctx["log"]("VERSION running fwversion = %s" % ver)
    return [(re.search(c["regex"], ver) is not None, "fwversion %r ~ /%s/" % (ver, c["regex"]))]


HANDLERS = {"rest": eval_rest, "telnet": eval_telnet, "wait": eval_wait, "version": eval_version}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", required=True)
    ap.add_argument("--checks", required=True)
    ap.add_argument("--out", default=".", help="directory for the transcript (default: .)")
    ap.add_argument("--http-timeout", type=float, default=10.0)
    ap.add_argument("--telnet-log", help="read telnet from a running capture (run folder or telnet.log) "
                                         "instead of connecting; the device takes one telnet client")
    a = ap.parse_args()
    try:
        with open(a.checks, encoding="utf-8") as f:
            spec = json.load(f)
        checks = spec["checks"] if isinstance(spec, dict) else spec
        for c in checks:
            if c.get("type") not in HANDLERS:
                raise ValueError("unknown check type %r" % c.get("type"))
    except Exception as e:
        print("bad checks file: %s" % e, file=sys.stderr)
        return 2
    os.makedirs(a.out, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    tpath = os.path.join(a.out, "bench-check-%s-%s.txt" % (a.host.replace(":", "_"), stamp))
    log = Log(tpath)
    ctx = {"host": a.host, "log": log, "http_timeout": a.http_timeout, "telnet_log": a.telnet_log}
    log("bench_check host=%s checks=%s transcript=%s" % (a.host, a.checks, tpath))
    npass = 0
    verdicts = []
    for i, c in enumerate(checks, 1):
        name = c.get("name", "%s-%d" % (c["type"], i))
        log("=== CHECK %d/%d %s [%s]" % (i, len(checks), name, c["type"]))
        try:
            res = HANDLERS[c["type"]](ctx, c)
        except Exception as e:
            res = [(False, "exception %s: %s" % (type(e).__name__, e))]
        ok = all(o for o, _ in res)
        for o, d in res:
            log("  %s %s" % ("ok  " if o else "FAIL", d))
        log("=== VERDICT %s: %s" % (name, "PASS" if ok else "FAIL"))
        npass += ok
        verdicts.append((name, ok))
    failed = "" if npass == len(checks) else " FAILED: " + ", ".join(n for n, o in verdicts if not o)
    log("SUMMARY host=%s %d/%d passed%s [%s] transcript=%s"
        % (a.host, npass, len(checks), failed, "PASS" if not failed else "FAIL", tpath))
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
