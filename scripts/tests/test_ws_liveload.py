#!/usr/bin/env python3
"""TASK-888 / TASK-779 / TASK-879 persistent-WS-realism load test.

The OTGW webui live-log uses a LONG-LIVED ws://<host>/ws connection (initOTLog-
WebSocket). Under heap pressure the WS broadcast path churns heap (canSendWebSocket
gate, TASK-779) and is implicated in George's field crash (WS heap churn). Every
other test tool only opens a TRANSIENT WS (one recv + close per page-load), so the
persistent-subscriber path is untested -- the documented realism gap.

This tool holds N long-lived ws://host/ws subscribers draining the live-log for the
whole run, concurrent with an HTTP asset/API flood, and watches the device for a
reboot/crash (bootcount delta) plus heap floor/recovery. Pair it with a serial
capture on the USB port to decode any panic:
    python scripts/tests/_serialcap.py COM4 <seconds+30> ws-serial.log   # in parallel

Usage:
  python scripts/tests/test_ws_liveload.py <host> [--subscribers N] [--flood-workers K]
      [--seconds S] [--no-flood]
"""
import sys, os, time, json, argparse, threading, urllib.request, urllib.error

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
try:
    import _secrets
except Exception:
    _secrets = None

ASSETS = ["/", "/index.js", "/components.css", "/ds-tokens.css", "/graph.js",
          "/sat.js", "/sat-slider.js", "/theme-toggle.js", "/echarts-theme.js"]
APIS = ["/api/v2/device/info", "/api/v2/settings", "/api/v2/sat/status",
        "/api/v2/otdirect/status", "/api/v2/otgw/otmonitor"]
LOADSET = ASSETS + APIS

_stop = threading.Event()
_lock = threading.Lock()
_stats = {"ws_frames": 0, "ws_bytes": 0, "ws_reconnects": 0, "ws_open": 0,
          "ws_errors": 0, "http_ok": 0, "http_fail": 0}


def ws_subscriber(host, idx):
    """Hold a persistent ws://host/ws connection, drain frames, reconnect on drop."""
    import websocket
    while not _stop.is_set():
        try:
            ws = websocket.create_connection("ws://%s/ws" % host, timeout=8)
            ws.settimeout(1.0)
            with _lock:
                _stats["ws_open"] += 1
        except Exception:
            with _lock:
                _stats["ws_errors"] += 1
            _stop.wait(2)
            continue
        while not _stop.is_set():
            try:
                msg = ws.recv()
                if msg == "" or msg is None:
                    break
                with _lock:
                    _stats["ws_frames"] += 1
                    _stats["ws_bytes"] += len(msg)
            except websocket.WebSocketTimeoutException:
                continue
            except Exception:
                break
        try:
            ws.close()
        except Exception:
            pass
        if not _stop.is_set():
            with _lock:
                _stats["ws_reconnects"] += 1
            _stop.wait(0.5)


def http_flood(host, loadset):
    i = 0
    while not _stop.is_set():
        path = loadset[i % len(loadset)]
        i += 1
        try:
            with urllib.request.urlopen("http://%s%s" % (host, path), timeout=10) as r:
                r.read()
                with _lock:
                    _stats["http_ok"] += 1
        except Exception:
            with _lock:
                _stats["http_fail"] += 1


def snap(host):
    try:
        with urllib.request.urlopen("http://%s/api/v2/device/info" % host, timeout=5) as r:
            d = json.loads(r.read())["device"]
            return {"boot": d.get("bootcount"), "heap": d.get("freeheap"),
                    "maxblock": d.get("maxfreeblock"), "frag": d.get("hd_fragmentation_pct"),
                    "uptime": d.get("uptime"), "mqtt": d.get("mqttconnected")}
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host", nargs="?", default=None,
                    help="device host/IP (else OTGW_HOST env or _secrets device_host)")
    ap.add_argument("--subscribers", type=int, default=3,
                    help="persistent ws://host/ws subscribers (a real dashboard + tabs)")
    ap.add_argument("--flood-workers", type=int, default=8,
                    help="concurrent HTTP GET flood workers (0 with --no-flood)")
    ap.add_argument("--seconds", type=float, default=120.0)
    ap.add_argument("--minutes", type=float, default=None,
                    help="load window in minutes (overrides --seconds; for otgw-test.py)")
    ap.add_argument("--no-flood", dest="flood", action="store_false", default=True)
    ap.add_argument("--api-only", dest="api_only", action="store_true", default=False,
                    help="flood only in-memory API endpoints (no LittleFS static files) "
                         "-- isolates the WS path from the static-file/LittleFS hang")
    a = ap.parse_args()
    a.host = a.host or os.environ.get("OTGW_HOST") or (_secrets.get("device_host") if _secrets else None)
    if not a.host:
        ap.error("no host (positional, OTGW_HOST env, or _secrets device_host)")
    if a.minutes is not None:
        a.seconds = a.minutes * 60.0
    loadset = APIS if a.api_only else LOADSET

    base = snap(a.host)
    floodlabel = ("%dw %s flood" % (a.flood_workers, "API-only" if a.api_only else "static+API")) if a.flood else "no flood"
    print("ws-liveload: %d persistent WS subscribers + %s on %s, %.0fs"
          % (a.subscribers, floodlabel, a.host, a.seconds))
    print("baseline: %s" % base)

    threads = []
    for i in range(a.subscribers):
        threads.append(threading.Thread(target=ws_subscriber, args=(a.host, i), daemon=True))
    if a.flood:
        for _ in range(a.flood_workers):
            threads.append(threading.Thread(target=http_flood, args=(a.host, loadset), daemon=True))
    for t in threads:
        t.start()

    t0 = time.time()
    floor = base
    while time.time() - t0 < a.seconds:
        time.sleep(5)
        s = snap(a.host)
        if s:
            if s["heap"] and (floor is None or not floor.get("heap") or s["heap"] < floor["heap"]):
                floor = s
            with _lock:
                print("  +%4ds boot=%s heap=%s maxblk=%s frag=%s%% ws_frames=%d reconn=%d ws_err=%d http_ok=%d"
                      % (int(time.time() - t0), s["boot"], s["heap"], s["maxblock"], s["frag"],
                         _stats["ws_frames"], _stats["ws_reconnects"], _stats["ws_errors"], _stats["http_ok"]))
        else:
            print("  +%4ds device/info UNREACHABLE (possible hang/reboot)" % int(time.time() - t0))
    _stop.set()
    for t in threads:
        t.join(timeout=10)

    time.sleep(3)
    final = snap(a.host)
    print("\n== RESULTS ==")
    print("WS: frames=%d bytes=%d opens=%d reconnects=%d errors=%d"
          % (_stats["ws_frames"], _stats["ws_bytes"], _stats["ws_open"],
             _stats["ws_reconnects"], _stats["ws_errors"]))
    print("HTTP flood: ok=%d fail=%d" % (_stats["http_ok"], _stats["http_fail"]))
    print("baseline : %s" % base)
    print("heap floor: %s" % floor)
    print("final    : %s" % final)
    bd = None
    if base and final and base["boot"] is not None and final["boot"] is not None:
        bd = final["boot"] - base["boot"]
    print("bootcount delta: %s" % bd)
    if bd == 0:
        print("VERDICT: PASS - no reboot under persistent WS + flood")
        sys.exit(0)
    elif bd is None:
        print("VERDICT: INCONCLUSIVE - could not read bootcount (device may be hung; check serial)")
        sys.exit(2)
    else:
        print("VERDICT: REBOOT - bootcount moved %s (decode the serial panic!)" % bd)
        sys.exit(1)


if __name__ == "__main__":
    main()
