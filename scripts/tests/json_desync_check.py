#!/usr/bin/env python3
"""TASK-883 determinism validator for the true-chunked REST endpoints.

The grep-gate proves determinism BY CONSTRUCTION (the emit closure reads only a
frozen snapshot). This is the RUNTIME counter-check: a desync only manifests when
a field's text width changes between two TCP-window passes of one response, so we
must (a) split responses across many windows (memory pressure + concurrency) and
(b) actually PARSE every body. test_load.py discards bodies and checks only the
status code, so it cannot see a corrupt-but-200 desync; this does.

It runs its own N-worker flood and json.loads() EVERY 200 body. A JSONDecodeError
(or a truncated/over-long body) is a determinism failure: it records the raw bytes
of the first failure for diagnosis. 503 (backpressure) and timeouts are counted but
are not failures. Run it standalone, or alongside test_load.py for extra pressure.

Usage:
  python scripts/tests/json_desync_check.py <host> [--workers N] [--seconds S]
"""
import sys, json, time, argparse, threading, urllib.request, urllib.error

ENDPOINTS = ["/api/v2/device/info", "/api/v2/sat/status",
             "/api/v2/sat/status?detail=full", "/api/v2/debug"]

_stop = threading.Event()
_lock = threading.Lock()
_stats = {"ok": 0, "busy": 0, "timeout": 0, "http_err": 0, "desync": 0}
_first_fail = {}   # endpoint -> raw bytes of first parse failure


def _get(host, ep):
    url = "http://%s%s" % (host, ep)
    try:
        with urllib.request.urlopen(url, timeout=8) as r:
            return r.getcode(), r.read()
    except urllib.error.HTTPError as e:
        return e.code, b""
    except Exception:
        return 0, b""


def worker(host, idx):
    i = idx
    n = len(ENDPOINTS)
    while not _stop.is_set():
        ep = ENDPOINTS[i % n]
        i += 1
        code, body = _get(host, ep)
        with _lock:
            if code == 200:
                try:
                    json.loads(body)
                    _stats["ok"] += 1
                except Exception:
                    _stats["desync"] += 1
                    if ep not in _first_fail:
                        _first_fail[ep] = body
            elif code == 503:
                _stats["busy"] += 1
            elif code == 0:
                _stats["timeout"] += 1
            else:
                _stats["http_err"] += 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--workers", type=int, default=16)
    ap.add_argument("--seconds", type=float, default=120.0)
    a = ap.parse_args()

    ts = [threading.Thread(target=worker, args=(a.host, k), daemon=True)
          for k in range(a.workers)]
    print("desync-check: %dw flooding %s for %.0fs, parsing every 200 body..."
          % (a.workers, a.host, a.seconds))
    for t in ts:
        t.start()
    try:
        time.sleep(a.seconds)
    except KeyboardInterrupt:
        pass
    _stop.set()
    for t in ts:
        t.join(timeout=10)

    s = _stats
    total = s["ok"] + s["desync"]
    print("\n=== results ===")
    print("  200+parsed OK : %d" % s["ok"])
    print("  DESYNC (200 but unparseable): %d" % s["desync"])
    print("  503 backpressure: %d   timeout: %d   http_err: %d"
          % (s["busy"], s["timeout"], s["http_err"]))
    if total:
        print("  parse success rate: %.4f%%" % (100.0 * s["ok"] / total))
    for ep, body in _first_fail.items():
        print("\n  FIRST DESYNC on %s (%d bytes):" % (ep, len(body)))
        txt = body.decode("utf-8", "replace")
        print("    head: %s" % txt[:200])
        print("    tail: %s" % txt[-200:])
    if s["desync"]:
        print("\nVERDICT: FAIL — determinism broken under load")
        sys.exit(1)
    print("\nVERDICT: PASS — every 200 body parsed; no desync under load")
    sys.exit(0)


if __name__ == "__main__":
    main()
