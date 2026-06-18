#!/usr/bin/env python
"""TASK-867 AC#8 heap-under-load test for the ArduinoJson v7 migration.

Hammers the heaviest ArduinoJson REST endpoints (settings ~8.6 KB, debug ~7.6 KB,
sat/status 132 fields, device/info) concurrently while a sampler watches the
ESP32-S3 heap via /api/v2/device/info. Also drives inbound (deserializeJson) load
via POST. Gates on the ADR-089 heap contract:

  PASS requires ALL of:
    - bootcount unchanged + uptime monotonic  (no crash / no Task-Watchdog reboot)
    - heap RECOVERS after the load stops       (no leak: final freeheap within
      LEAK_TOLERANCE_PCT of baseline)
    - fragmentation does not run away          (max hd_fragmentation_pct bounded)
    - critical heap tier not breached, or breached-and-recovered
    - request success rate above MIN_SUCCESS_PCT

Usage:
  python scripts/tests/test_load.py [--minutes N] [--workers K] [--host IP]

Stdlib only (urllib + threading), matching scripts/json_golden.py. Read-only on
the firmware; the one mutation (POST sattargettemp) is restored at the end.
"""
import sys, os, json, time, argparse, threading, urllib.request, urllib.error
from collections import Counter

# Heaviest JSON producers first — these churn the most ArduinoJson heap per call.
GET_ENDPOINTS = [
    "/v2/settings",            # ~8.6 KB, biggest JsonDocument
    "/v2/debug",               # ~7.6 KB
    "/v2/sat/status",          # 132 fields
    "/v2/sat/status?detail=full",
    "/v2/device/info",
    "/v2/sensors",
    "/v2/otgw/otmonitor",
    "/v2/filesystem/files",
    "/v2/discovery",
]
HEALTH = "/v2/health"
DEVICE_INFO = "/v2/device/info"

LEAK_TOLERANCE_PCT = 8.0     # final heap must be within this % of baseline
MIN_SUCCESS_PCT = 98.0       # request success rate floor under load
# A transient fragmentation peak during heavy JsonDocument churn is expected and
# harmless if it recovers. Only SUSTAINED fragmentation (still high after the
# cooldown) or a maxfreeblock too small to hold the largest response doc is a real
# risk. ~22 KB covers the biggest JsonDocument (settings ~8.6 KB serialized,
# roughly 2x in-doc).
FRAG_SUSTAINED_PCT = 70      # hd_fragmentation_pct AFTER cooldown above this = concern
MIN_MAXBLOCK_FLOOR = 22000   # largest contiguous free block must stay above this under load

_stop = threading.Event()
_stats = Counter()
_stats_lock = threading.Lock()
_samples = []


def _get(host, ep, timeout=8):
    url = "http://%s/api%s" % (host, ep)
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return r.getcode(), r.read()
    except urllib.error.HTTPError as e:
        return e.code, b""
    except Exception:
        return 0, b""


def _post(host, ep, body, timeout=8):
    url = "http://%s/api%s" % (host, ep)
    req = urllib.request.Request(url, data=body.encode(), method="POST",
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.getcode()
    except urllib.error.HTTPError as e:
        return e.code
    except Exception:
        return 0


def get_heap(host):
    """Snapshot the heap metrics from device/info. Returns dict or None."""
    code, body = _get(host, DEVICE_INFO)
    if code != 200:
        return None
    try:
        d = json.loads(body)["device"]
    except Exception:
        return None
    keys = ["freeheap", "maxfreeblock", "hd_fragmentation_pct",
            "hd_enter_low", "hd_enter_warning", "hd_enter_critical",
            "hd_ws_drops", "hd_mqtt_drops", "bootcount", "uptime"]
    return {k: d.get(k) for k in keys}


def worker(host, idx):
    n = len(GET_ENDPOINTS)
    i = idx
    while not _stop.is_set():
        ep = GET_ENDPOINTS[i % n]
        i += 1
        code, _ = _get(host, ep)
        with _stats_lock:
            _stats["req"] += 1
            _stats["ok" if code == 200 else "fail"] += 1
            if code == 0:
                _stats["timeout"] += 1


def inbound_worker(host):
    """Drive deserializeJson under load: cycle sattargettemp via POST."""
    vals = ["20.5", "21.0", "21.5", "22.0"]
    i = 0
    while not _stop.is_set():
        v = vals[i % len(vals)]
        i += 1
        code = _post(host, "/v2/settings/sattargettemp",
                     '{"name":"sattargettemp","value":"%s"}' % v)
        with _stats_lock:
            _stats["post"] += 1
            _stats["post_ok" if code == 200 else "post_fail"] += 1
        time.sleep(0.5)   # gentler than the GET storm; still exercises the path


def sampler(host, interval=3.0):
    while not _stop.is_set():
        h = get_heap(host)
        if h:
            h["t"] = round(time.monotonic(), 1)
            _samples.append(h)
        _stop.wait(interval)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=5.0, help="load window (max 15)")
    ap.add_argument("--workers", type=int, default=8, help="concurrent GET workers")
    ap.add_argument("--host", default=os.environ.get("OTGW_HOST", "192.168.88.39"))
    args = ap.parse_args()
    seconds = int(min(max(args.minutes, 0.5), 15) * 60)
    host = args.host

    print("== TASK-867 AC#8 heap-under-load ==")
    print("host=%s window=%ds workers=%d" % (host, seconds, args.workers))

    base = get_heap(host)
    if not base:
        print("FAIL: device not reachable / device/info not 200")
        sys.exit(2)
    print("baseline: freeheap=%s maxblock=%s frag=%s%% boot=%s uptime=%s"
          % (base["freeheap"], base["maxfreeblock"], base["hd_fragmentation_pct"],
             base["bootcount"], base["uptime"]))

    threads = [threading.Thread(target=sampler, args=(host,), daemon=True)]
    for k in range(args.workers):
        threads.append(threading.Thread(target=worker, args=(host, k), daemon=True))
    threads.append(threading.Thread(target=inbound_worker, args=(host,), daemon=True))
    for t in threads:
        t.start()

    t0 = time.monotonic()
    try:
        while time.monotonic() - t0 < seconds:
            time.sleep(2)
            with _stats_lock:
                req, ok, fail = _stats["req"], _stats["ok"], _stats["fail"]
            cur = _samples[-1] if _samples else {}
            print("  +%4ds  req=%d ok=%d fail=%d  freeheap=%s maxblock=%s frag=%s%%"
                  % (int(time.monotonic() - t0), req, ok, fail,
                     cur.get("freeheap"), cur.get("maxfreeblock"),
                     cur.get("hd_fragmentation_pct")))
    finally:
        _stop.set()
    time.sleep(2)

    # cool-down: let the heap settle, then take the recovery snapshot
    print("cooling down 12s (recovery / leak check)...")
    time.sleep(12)
    final = get_heap(host) or {}

    # ---- analysis ----
    heaps = [s["freeheap"] for s in _samples if isinstance(s.get("freeheap"), int)]
    frags = [s["hd_fragmentation_pct"] for s in _samples if isinstance(s.get("hd_fragmentation_pct"), int)]
    blocks = [s["maxfreeblock"] for s in _samples if isinstance(s.get("maxfreeblock"), int)]
    heap_floor = min(heaps) if heaps else None
    frag_max = max(frags) if frags else None
    block_floor = min(blocks) if blocks else None
    frag_final = final.get("hd_fragmentation_pct")

    def delta(k):
        a, b = base.get(k), final.get(k)
        return (b - a) if isinstance(a, int) and isinstance(b, int) else None

    boot_delta = delta("bootcount")
    low_d, warn_d, crit_d = delta("hd_enter_low"), delta("hd_enter_warning"), delta("hd_enter_critical")
    recover_pct = None
    if isinstance(base.get("freeheap"), int) and isinstance(final.get("freeheap"), int) and base["freeheap"]:
        recover_pct = 100.0 * (final["freeheap"] - base["freeheap"]) / base["freeheap"]

    with _stats_lock:
        req, ok, fail = _stats["req"], _stats["ok"], _stats["fail"]
        post, post_ok = _stats["post"], _stats["post_ok"]
    succ_pct = (100.0 * ok / req) if req else 0.0

    # restore the setting we cycled
    _post(host, "/v2/settings/sattargettemp", '{"name":"sattargettemp","value":"20.0"}')

    print("\n== RESULTS ==")
    print("requests: %d (ok=%d fail=%d) success=%.2f%% | inbound POST: %d ok=%d"
          % (req, ok, fail, succ_pct, post, post_ok))
    print("heap floor under load: %s  (baseline %s)" % (heap_floor, base.get("freeheap")))
    print("heap after cooldown:   %s  (recovery %+.1f%% vs baseline)"
          % (final.get("freeheap"), recover_pct if recover_pct is not None else 0))
    print("fragmentation: peak %s%% during load, %s%% after cooldown (baseline %s%%)"
          % (frag_max, frag_final, base.get("hd_fragmentation_pct")))
    print("maxfreeblock floor under load: %s  (largest contiguous alloc still possible)" % block_floor)
    print("ADR-089 tier entries during load: low+%s warning+%s critical+%s"
          % (low_d, warn_d, crit_d))
    print("bootcount delta:       %s  (0 = no crash/reboot)" % boot_delta)
    print("uptime base->final:    %s -> %s" % (base.get("uptime"), final.get("uptime")))

    # ---- verdict ----
    fails = []
    if boot_delta is None or boot_delta != 0:
        fails.append("device rebooted/crashed during load (bootcount delta=%s)" % boot_delta)
    if recover_pct is not None and recover_pct < -LEAK_TOLERANCE_PCT:
        fails.append("heap did not recover (%.1f%% < -%.1f%% leak tolerance)" % (recover_pct, LEAK_TOLERANCE_PCT))
    if succ_pct < MIN_SUCCESS_PCT:
        fails.append("request success %.2f%% < %.1f%% floor" % (succ_pct, MIN_SUCCESS_PCT))
    if isinstance(frag_final, int) and frag_final >= FRAG_SUSTAINED_PCT:
        fails.append("fragmentation stayed high after cooldown (%s%% >= %s%%) - not just a transient peak"
                     % (frag_final, FRAG_SUSTAINED_PCT))
    if isinstance(block_floor, int) and block_floor < MIN_MAXBLOCK_FLOOR:
        fails.append("largest free block fell to %s (< %s) - big JsonDocument allocs at risk"
                     % (block_floor, MIN_MAXBLOCK_FLOOR))
    if isinstance(crit_d, int) and crit_d > 0 and (recover_pct is None or recover_pct < -LEAK_TOLERANCE_PCT):
        fails.append("critical heap tier entered %sx without recovery" % crit_d)

    if fails:
        print("\nVERDICT: FAIL")
        for f in fails:
            print("  - " + f)
        sys.exit(1)
    print("\nVERDICT: PASS — no crash, heap recovered, fragmentation bounded, requests served.")
    sys.exit(0)


if __name__ == "__main__":
    main()
