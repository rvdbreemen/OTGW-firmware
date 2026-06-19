#!/usr/bin/env python3
"""TASK-879 webui sub-resource-burst stress / hang repro + realistic-load discriminator.

A real browser loading the OTGW32 webui issues ~20 requests (root HTML + ~8 static
JS/CSS assets + ~11 initial API XHRs) plus a WebSocket, but over a bounded pool of
~6 connections per host (HTTP/1.1), NOT 20-at-once. This tool can do either:
  --max-conns 0   : every page-load fires all ~20 requests fully in parallel
                    (synthetic worst case; N browsers => ~20*N simultaneous sockets)
  --max-conns K   : a global semaphore caps in-flight requests to K, modelling a
                    browser's bounded connection pool (K~6 = one browser, ~12-18 =
                    a few tabs + HA). Use this to tell a REAL-client-reachable hang
                    from a synthetic-overload artifact.

A health monitor probes /api/v2/device/info every 2s; `--hang-after` consecutive
failures = declared hang. On a hang it does NOT reset the device: it stops all load
and polls for self-recovery for `--recovery` seconds (lwIP TIME_WAIT releases PCBs
in ~1-2 min). Self-recovery => transient backpressure, not a deadlock.

Usage:
  python scripts/tests/webui_burst.py <host> [--browsers N] [--max-conns K]
      [--seconds S] [--ws/--no-ws] [--hang-after J] [--recovery R]
"""
import sys, time, argparse, threading, urllib.request, urllib.error

ASSETS = ["/", "/index.js", "/components.css", "/ds-tokens.css", "/graph.js",
          "/sat.js", "/sat-slider.js", "/theme-toggle.js", "/echarts-theme.js"]
APIS = ["/api/v2/device/info", "/api/v2/settings", "/api/v2/sat/status",
        "/api/v2/otdirect/overrides", "/api/v2/otdirect/status",
        "/api/v2/otgw/boiler-support", "/api/v2/otgw/ot-support",
        "/api/v2/otgw/otmonitor", "/api/v2/otgw/overrides",
        "/api/v2/pic/settings", "/api/v2/sat/ble/discovery"]
PAGE = ASSETS + APIS

_stop = threading.Event()
_hang = threading.Event()
_lock = threading.Lock()
_stats = {"loads": 0, "req_ok": 0, "req_fail": 0, "req_503": 0}
_sem = None   # set in main when --max-conns > 0


def _get(host, path):
    if _sem is not None:
        _sem.acquire()
    try:
        with urllib.request.urlopen("http://%s%s" % (host, path), timeout=10) as r:
            r.read()
            return r.getcode()
    except urllib.error.HTTPError as e:
        return e.code
    except Exception:
        return 0
    finally:
        if _sem is not None:
            _sem.release()


def page_load(host, use_ws):
    codes = [None] * len(PAGE)
    def one(i):
        codes[i] = _get(host, PAGE[i])
    threads = [threading.Thread(target=one, args=(i,)) for i in range(len(PAGE))]
    ws = None
    if use_ws:
        try:
            import websocket
            ws = websocket.create_connection("ws://%s/ws" % host, timeout=8)
            ws.settimeout(0.2)
        except Exception:
            ws = None
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=12)
    if ws is not None:
        try:
            ws.recv()
        except Exception:
            pass
        try:
            ws.close()
        except Exception:
            pass
    ok = sum(1 for c in codes if c == 200)
    busy = sum(1 for c in codes if c == 503)
    fail = sum(1 for c in codes if c in (0, None) or (c not in (200, 503)))
    with _lock:
        _stats["loads"] += 1
        _stats["req_ok"] += ok
        _stats["req_503"] += busy
        _stats["req_fail"] += fail


def browser(host, use_ws, max_loads):
    while not _stop.is_set():
        if max_loads > 0:
            with _lock:
                if _stats["loads"] >= max_loads:
                    _stop.set()
                    return
        page_load(host, use_ws)


def health(host, hang_after):
    consecutive = 0
    while not _stop.is_set():
        try:
            with urllib.request.urlopen("http://%s/api/v2/device/info" % host, timeout=4) as r:
                ok = (r.getcode() == 200)
                r.read()
        except Exception:
            ok = False
        if ok:
            consecutive = 0
        else:
            consecutive += 1
            if consecutive >= hang_after:
                _hang.set()
                _stop.set()
                return
        _stop.wait(2)


def recovery_watch(host, seconds):
    """All load stopped. Poll for self-recovery; return seconds-to-recover or None."""
    t0 = time.time()
    while time.time() - t0 < seconds:
        try:
            with urllib.request.urlopen("http://%s/api/v2/device/info" % host, timeout=4) as r:
                if r.getcode() == 200:
                    r.read()
                    return round(time.time() - t0, 1)
        except Exception:
            pass
        time.sleep(5)
    return None


def main():
    global _sem
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--browsers", type=int, default=3)
    ap.add_argument("--max-conns", type=int, default=0,
                    help="cap total in-flight requests (0=unlimited; 6=one browser)")
    ap.add_argument("--seconds", type=float, default=90.0)
    ap.add_argument("--ws", dest="ws", action="store_true", default=True)
    ap.add_argument("--no-ws", dest="ws", action="store_false")
    ap.add_argument("--hang-after", type=int, default=6)
    ap.add_argument("--recovery", type=int, default=240,
                    help="on hang, poll this many seconds for self-recovery (no reset)")
    a = ap.parse_args()
    if a.max_conns > 0:
        _sem = threading.Semaphore(a.max_conns)

    print("webui-burst: %d browsers, max-conns=%s%s, %.0fs on %s"
          % (a.browsers, a.max_conns or "unlimited", "+WS" if a.ws else "", a.seconds, a.host))
    hm = threading.Thread(target=health, args=(a.host, a.hang_after), daemon=True)
    hm.start()
    bs = [threading.Thread(target=browser, args=(a.host, a.ws), daemon=True)
          for _ in range(a.browsers)]
    for t in bs:
        t.start()
    _stop.wait(a.seconds)
    _stop.set()
    for t in bs:
        t.join(timeout=15)

    s = _stats
    print("\n=== results ===")
    print("  page loads completed: %d" % s["loads"])
    print("  sub-requests: ok=%d  503=%d  fail/timeout=%d" %
          (s["req_ok"], s["req_503"], s["req_fail"]))
    if _hang.is_set():
        print("\n  *** DEVICE HANG DETECTED *** after %d loads at %d browsers / max-conns=%s."
              % (s["loads"], a.browsers, a.max_conns or "unlimited"))
        print("  load stopped; watching for self-recovery (no reset) up to %ds..." % a.recovery)
        rec = recovery_watch(a.host, a.recovery)
        if rec is not None:
            print("  SELF-RECOVERED after %.1fs with no reset -> transient backpressure, not a deadlock." % rec)
            print("VERDICT: HANG (transient) — recovered on its own")
            sys.exit(3)
        print("  NO self-recovery in %ds -> persistent hang (needs esptool reset)." % a.recovery)
        print("VERDICT: HANG (persistent) — hard deadlock")
        sys.exit(2)
    print("\nVERDICT: survived — no hang at %d browsers / max-conns=%s"
          % (a.browsers, a.max_conns or "unlimited"))
    sys.exit(0)


if __name__ == "__main__":
    main()
