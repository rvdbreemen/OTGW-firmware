"""
loadtest_pageload.py - CDP headless-Edge cold/warm page-load harness for TASK-1015
====================================================================================

PURPOSE
-------
Drives a REAL browser page-load (not a synthetic HTTP client) against the OTGW-firmware
v2 Web UI over the Chrome DevTools Protocol (CDP), so the load-test study (TASK-1015)
can see what an actual user's browser experiences: per-asset parallel connection count,
DOMContentLoaded/load timing, and any 503s the ADR-147 backpressure gate hands back
during a real multi-asset page load (v2.html + v2.css + ds-tokens.css + v2.js + API calls
the page fires on load).

Two runs per invocation:
  COLD  - fresh --user-data-dir, empty cache -> every asset is a full fetch
  WARM  - second navigation in the SAME profile -> conditional GETs, 304s where the
          firmware's static-asset caching allows it

Uses the raw CDP websocket protocol (no selenium/playwright dependency) - same approach
already proven in scripts/capture-mqtt-debug.bat's browser-capture runspace. Navigates
by IP (never a .local mDNS name - resolution flakiness is a known source of false
"page won't load" reports, see reference_capture_browser_devtools memory).

USAGE
-----
  python scripts/loadtest_pageload.py --host 192.168.88.64
  python scripts/loadtest_pageload.py --host 192.168.88.64 --edge-path "C:\\...\\msedge.exe"
"""

import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request

try:
    import websocket  # websocket-client
except ImportError:
    print("Missing dependency: pip install websocket-client", file=sys.stderr)
    sys.exit(1)

EDGE_CANDIDATES = [
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
]


def find_edge(explicit):
    if explicit:
        return explicit
    for c in EDGE_CANDIDATES:
        if os.path.exists(c):
            return c
    found = shutil.which("msedge") or shutil.which("chrome")
    if found:
        return found
    raise SystemExit("msedge.exe/chrome.exe not found; pass --edge-path")


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def wait_for_cdp(port, timeout=15.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/json", timeout=1) as r:
                targets = json.loads(r.read().decode())
            for t in targets:
                if t.get("type") == "page" and t.get("webSocketDebuggerUrl"):
                    return t["webSocketDebuggerUrl"]
        except Exception:
            pass
        time.sleep(0.2)
    raise SystemExit(f"CDP page target not found on port {port} within {timeout}s")


class CdpSession:
    def __init__(self, ws_url):
        self.ws = websocket.create_connection(ws_url, timeout=30)
        self.msg_id = 0
        self.requests = {}   # requestId -> {url, method, start_wall_ts}
        self.finished = []   # list of dicts: url, method, status, from_disk_cache, mime, encoded_len
        self.load_event_ts = None
        self.dcl_event_ts = None

    def send(self, method, params=None):
        self.msg_id += 1
        self.ws.send(json.dumps({"id": self.msg_id, "method": method, "params": params or {}}))
        return self.msg_id

    def enable_domains(self):
        self.send("Network.enable")
        self.send("Page.enable")

    def navigate(self, url):
        self.send("Page.navigate", {"url": url})

    def pump_until_load(self, timeout=20.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.ws.settimeout(max(0.1, deadline - time.monotonic()))
            try:
                raw = self.ws.recv()
            except Exception:
                break
            try:
                msg = json.loads(raw)
            except ValueError:
                continue
            method = msg.get("method")
            params = msg.get("params", {})
            if method == "Network.requestWillBeSent":
                self.requests[params["requestId"]] = {
                    "url": params["request"]["url"],
                    "method": params["request"]["method"],
                    "wall_ts": params.get("wallTime"),
                    # monotonic CDP timeline (seconds) - used for the parallel-connection sweep
                    "start_ts": params.get("timestamp"),
                }
            elif method == "Network.responseReceived":
                rid = params["requestId"]
                if rid in self.requests:
                    self.requests[rid]["status"] = params["response"]["status"]
                    self.requests[rid]["from_disk_cache"] = params["response"].get("fromDiskCache", False)
                    self.requests[rid]["mime"] = params["response"].get("mimeType")
                    self.requests[rid]["encoded_len"] = params["response"].get("encodedDataLength", 0)
            elif method == "Network.loadingFinished":
                rid = params["requestId"]
                if rid in self.requests:
                    self.requests[rid]["end_ts"] = params.get("timestamp")
                    self.finished.append(self.requests[rid])
            elif method == "Page.domContentEventFired":
                self.dcl_event_ts = params.get("timestamp")
            elif method == "Page.loadEventFired":
                self.load_event_ts = params.get("timestamp")
                # give in-flight requests a moment to finish, then stop
                deadline = min(deadline, time.monotonic() + 1.0)

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass


def peak_parallel_connections(requests):
    """
    Peak concurrently-in-flight requests via a sweep line over each request's
    [start_ts, end_ts] interval (CDP monotonic timeline, seconds). This is a real
    overlap count, not a request-count proxy - two requests only count as parallel
    if their network lifetimes actually overlapped.
    """
    events = []
    for r in requests:
        if "start_ts" not in r or "end_ts" not in r:
            continue
        events.append((r["start_ts"], 1))
        events.append((r["end_ts"], -1))
    events.sort(key=lambda e: (e[0], e[1]))  # ends (-1) before starts (1) at equal ts
    active = 0
    peak = 0
    for _, delta in events:
        active += delta
        peak = max(peak, active)
    return peak


def run_one(edge_path, url, profile_dir, label):
    port = free_port()
    args = [
        edge_path,
        "--headless=new",
        "--disable-gpu",
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-features=msSmartScreenProtection",
        "--remote-allow-origins=*",
        f"--remote-debugging-port={port}",
        f"--user-data-dir={profile_dir}",
        "about:blank",
    ]
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        ws_url = wait_for_cdp(port)
        cdp = CdpSession(ws_url)
        cdp.enable_domains()
        t0 = time.monotonic()
        cdp.navigate(url)
        cdp.pump_until_load(timeout=25.0)
        wall_elapsed_ms = (time.monotonic() - t0) * 1000.0
        cdp.close()

        statuses = {}
        cache_hits = 0
        for r in cdp.finished:
            st = r.get("status")
            statuses[st] = statuses.get(st, 0) + 1
            if r.get("from_disk_cache"):
                cache_hits += 1

        return {
            "label": label,
            "wall_elapsed_ms": round(wall_elapsed_ms, 1),
            "requests_finished": len(cdp.finished),
            "peak_parallel_connections": peak_parallel_connections(cdp.finished),
            "status_distribution": statuses,
            "count_503": statuses.get(503, 0),
            "from_disk_cache_count": cache_hits,
            "dom_content_loaded": cdp.dcl_event_ts is not None,
            "load_event": cdp.load_event_ts is not None,
        }
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP (no scheme), e.g. 192.168.88.64 - never a .local hostname")
    p.add_argument("--path", default="/v2.html", help="Page path to load (default /v2.html)")
    p.add_argument("--edge-path", default=None, help="Explicit msedge.exe/chrome.exe path (auto-detect if omitted)")
    args = p.parse_args()

    edge_path = find_edge(args.edge_path)
    url = f"http://{args.host}{args.path}"

    tmp_root = tempfile.mkdtemp(prefix="otgw_loadtest_pageload_")
    cold_profile = os.path.join(tmp_root, "cold")
    warm_profile = os.path.join(tmp_root, "warm")
    os.makedirs(cold_profile, exist_ok=True)
    os.makedirs(warm_profile, exist_ok=True)

    try:
        cold = run_one(edge_path, url, cold_profile, "cold")
        # warm: reuse the SAME profile dir for a second navigation so the browser
        # cache from the first load in that profile is available.
        warm_first = run_one(edge_path, url, warm_profile, "warm-priming")
        warm = run_one(edge_path, url, warm_profile, "warm")
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)

    print(json.dumps({"cold": cold, "warm": warm}, indent=2))


if __name__ == "__main__":
    main()
