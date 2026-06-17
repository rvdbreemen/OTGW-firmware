#!/usr/bin/env python3
"""ACTIVE webserver test for the OTGW32 (separate from the passive capture scripts).

Drives the device over the REST API and records status + latency per endpoint,
repeatedly over the test window. This is the assertion side of the TASK-866/879
"webserver does not come up / responds slowly" investigation: it turns the symptom
into numbers (HTTP code, wall-clock latency, timeouts) the orchestrator can analyse.

It NEVER captures/monitors passively and never opens telnet — that is the capture
scripts' job. It only pokes the HTTP API. Host comes from the out-of-repo
capture-settings.json via _secrets.py.

Usage (normally launched by otgw-test.py):
  python scripts/tests/test_webserver.py --minutes 5 [--host OTGW.local]
"""
import argparse
import os
import sys
import time
import urllib.request
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # scripts/ for _secrets
try:
    import _secrets
except Exception:
    _secrets = None

ENDPOINTS = [
    "/",
    "/index.html",
    "/api/v2/health",
    "/api/v2/device/info",
    "/api/v2/device/time",
    "/api/v2/sat/status",
]


def probe(base, path, timeout):
    url = base + path
    t0 = time.monotonic()
    try:
        req = urllib.request.Request(url, headers={"Connection": "close"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            body = r.read()
            dt = time.monotonic() - t0
            return (r.status, len(body), dt, "")
    except urllib.error.HTTPError as e:
        return (e.code, 0, time.monotonic() - t0, "http")
    except Exception as e:
        return (0, 0, time.monotonic() - t0, type(e).__name__)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=5.0)
    ap.add_argument("--host", default=None)
    ap.add_argument("--timeout", type=float, default=10.0)
    ap.add_argument("--interval", type=float, default=10.0, help="seconds between rounds")
    args = ap.parse_args()

    host = args.host or (_secrets.get("device_host", "OTGW.local") if _secrets else "OTGW.local")
    base = f"http://{host}"
    deadline = time.monotonic() + args.minutes * 60
    print(f"# webserver test  base={base}  window={args.minutes}min  timeout={args.timeout}s")
    print("# time      endpoint                  code  bytes   latency_s  err")

    rounds = 0
    worst = 0.0
    fails = 0
    timeouts = 0
    while time.monotonic() < deadline:
        rounds += 1
        for path in ENDPOINTS:
            code, nbytes, dt, err = probe(base, path, args.timeout)
            ts = datetime.now().strftime("%H:%M:%S")
            print(f"{ts}  {path:24}  {code:4}  {nbytes:6}  {dt:8.3f}  {err}", flush=True)
            worst = max(worst, dt)
            if code == 0:
                fails += 1
                if err == "timeout" or dt >= args.timeout - 0.2:
                    timeouts += 1
            elif code >= 500:
                fails += 1
        time.sleep(args.interval)

    probes = rounds * len(ENDPOINTS)
    print(f"\n# SUMMARY rounds={rounds} probes={probes} fails={fails} "
          f"timeouts={timeouts} worst_latency_s={worst:.3f}")
    # Non-zero exit when the webserver is unhealthy, so the orchestrator/loop can react.
    if probes == 0:
        print("# VERDICT: no probes ran")
        return 2
    fail_ratio = fails / probes
    if fail_ratio > 0.25 or worst > 3.0:
        print(f"# VERDICT: UNHEALTHY (fail_ratio={fail_ratio:.2f}, worst={worst:.3f}s)")
        return 1
    print("# VERDICT: healthy")
    return 0


if __name__ == "__main__":
    sys.exit(main())
