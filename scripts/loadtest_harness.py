"""
loadtest_harness.py - host-side asyncio/httpx load harness for TASK-1015 (parallelism study)
==============================================================================================

PURPOSE
-------
Drives a configurable, EXACTLY-controlled offered concurrency (via an asyncio.Semaphore)
against a live OTGW-firmware v2 Web UI device, mixing REST API GETs/POSTs and static-asset
GETs, so the load-test study (TASK-1015) can sweep offered-N and observe the firmware's
REST/web-file backpressure gate (ADR-147, TASK-884) responding with 503s under pressure.

This harness deliberately DOES NOT follow the frontend's single-flight rule (CLAUDE.md:
"Fire at most ONE /api request at a time") - that rule protects the browser UX; this tool's
whole job is to synthesize concurrent load server-side and measure where the gate holds or
breaks.

WORKLOAD MIX (one "unit" of work, weighted by --mix)
-----------------------------------------------------
  GET  /api/v2/device/info   - cheap REST read
  GET  /api/v2/sat/status    - cheap REST read
  GET  /api/v2/health        - cheapest REST read (liveness probe shape)
  POST /api/v2/settings      - REST write, mirrors the frontend's postSetting() body shape
                                {"name": "ui_onboarded", "value": true} - idempotent, safe
                                to spam (does not disable/enable any subsystem).
  GET  /v2.html, /v2.css, /ds-tokens.css, /v2.js - static LittleFS asset serve path

OUTPUT
------
NDJSON log, one line per request: {ts, method, path, status, latency_ms}.
Peak concurrent in-flight requests (from THIS client's perspective) is tracked and printed
in the summary - this is the "offered concurrency actually achieved" figure, which should
equal --concurrency once the run is warmed up (it will be lower only during ramp-up/ramp-down).

USAGE
-----
  python scripts/loadtest_harness.py --host 192.168.88.64 --concurrency 4 --duration 30
  python scripts/loadtest_harness.py --host 192.168.88.64 --concurrency 8 --requests 500 --log run_n8.ndjson
"""

import argparse
import asyncio
import json
import sys
import time

try:
    import httpx
except ImportError:
    print("Missing dependency: pip install httpx", file=sys.stderr)
    sys.exit(1)

DEFAULT_MIX = {
    "get_device_info": 3,
    "get_sat_status": 3,
    "get_health": 2,
    "post_settings": 1,
    "get_v2html": 1,
    "get_v2css": 1,
    "get_dstokens": 1,
    "get_v2js": 1,
}


def build_units(base_url):
    return {
        "get_device_info": ("GET", base_url + "/api/v2/device/info", None),
        "get_sat_status": ("GET", base_url + "/api/v2/sat/status", None),
        "get_health": ("GET", base_url + "/api/v2/health", None),
        "post_settings": ("POST", base_url + "/api/v2/settings", {"name": "ui_onboarded", "value": True}),
        "get_v2html": ("GET", base_url + "/v2.html", None),
        "get_v2css": ("GET", base_url + "/v2.css", None),
        "get_dstokens": ("GET", base_url + "/ds-tokens.css", None),
        "get_v2js": ("GET", base_url + "/v2.js", None),
    }


def weighted_cycle(mix):
    """Deterministic round-robin weighted sequence (no randomness - reproducible runs)."""
    names = []
    for name, weight in mix.items():
        names.extend([name] * weight)
    i = 0
    while True:
        yield names[i % len(names)]
        i += 1


class PeakTracker:
    def __init__(self):
        self.active = 0
        self.peak = 0
        self._lock = asyncio.Lock()

    async def enter(self):
        async with self._lock:
            self.active += 1
            self.peak = max(self.peak, self.active)

    async def leave(self):
        async with self._lock:
            self.active -= 1


async def do_request(client, sem, tracker, method, url, json_body, log_fh, stats):
    async with sem:
        await tracker.enter()
        t0 = time.monotonic()
        try:
            if method == "GET":
                r = await client.get(url)
            else:
                r = await client.post(url, json=json_body)
            status = r.status_code
        except httpx.RequestError as exc:
            status = -1
            stats["errors"] += 1
            stats["error_detail"][type(exc).__name__] = stats["error_detail"].get(type(exc).__name__, 0) + 1
        latency_ms = (time.monotonic() - t0) * 1000.0
        await tracker.leave()

        stats["total"] += 1
        stats["by_status"][status] = stats["by_status"].get(status, 0) + 1
        stats["latency_sum_ms"] += latency_ms
        stats["latency_max_ms"] = max(stats["latency_max_ms"], latency_ms)

        if log_fh:
            log_fh.write(json.dumps({
                "ts": time.time(),
                "method": method,
                "path": url,
                "status": status,
                "latency_ms": round(latency_ms, 2),
            }) + "\n")


async def run(args):
    base_url = f"http://{args.host}"
    units = build_units(base_url)
    cycle = weighted_cycle(DEFAULT_MIX)
    sem = asyncio.Semaphore(args.concurrency)
    tracker = PeakTracker()
    stats = {"total": 0, "errors": 0, "error_detail": {}, "by_status": {}, "latency_sum_ms": 0.0, "latency_max_ms": 0.0}

    log_fh = open(args.log, "w", encoding="utf-8") if args.log else None

    async with httpx.AsyncClient(timeout=httpx.Timeout(args.timeout)) as client:
        tasks = []
        deadline = time.monotonic() + args.duration if args.duration else None
        count = 0
        while True:
            if args.requests and count >= args.requests:
                break
            if deadline and time.monotonic() >= deadline:
                break
            name = next(cycle)
            method, url, body = units[name]
            tasks.append(asyncio.create_task(do_request(client, sem, tracker, method, url, body, log_fh, stats)))
            count += 1
            # Don't let the task list race arbitrarily far ahead of the semaphore -
            # keep memory bounded on long/high-N runs.
            if len(tasks) >= args.concurrency * 4:
                await asyncio.gather(*tasks)
                tasks = []
        if tasks:
            await asyncio.gather(*tasks)

    if log_fh:
        log_fh.close()

    print(f"--- loadtest_harness summary (offered concurrency={args.concurrency}) ---")
    print(f"total requests : {stats['total']}")
    print(f"errors (exceptions): {stats['errors']} {stats['error_detail']}")
    print(f"peak concurrent (client-observed): {tracker.peak}")
    if stats["total"]:
        print(f"avg latency    : {stats['latency_sum_ms'] / stats['total']:.2f} ms")
    print(f"max latency    : {stats['latency_max_ms']:.2f} ms")
    print("status distribution:")
    for status, n in sorted(stats["by_status"].items(), key=lambda kv: str(kv[0])):
        print(f"  {status}: {n}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP or hostname (no scheme), e.g. 192.168.88.64")
    p.add_argument("--concurrency", type=int, required=True, help="Exact offered concurrency (semaphore size)")
    p.add_argument("--duration", type=float, default=30.0, help="Run duration in seconds (default 30; ignored if --requests set)")
    p.add_argument("--requests", type=int, default=0, help="Fixed total request count instead of a duration-based run")
    p.add_argument("--timeout", type=float, default=10.0, help="Per-request httpx timeout in seconds")
    p.add_argument("--log", default=None, help="NDJSON per-request log file path")
    args = p.parse_args()
    asyncio.run(run(args))


if __name__ == "__main__":
    main()
