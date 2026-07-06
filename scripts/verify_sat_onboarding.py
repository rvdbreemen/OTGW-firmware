"""
verify_sat_onboarding.py - on-device functional check for TASK-1014 AC#5
============================================================================

Drives the real SAT onboarding wizard (v2.js's startSatOnboarding/commit) via a
headless-Edge CDP session, clicking through the actual DOM the same way a user
would (button.click() on each data-sa element, not a curl bypass of the wizard
logic). Confirms: (1) no HTTP 503 anywhere in the commit() POST chain, (2) the
wizard reaches its 'done' screen, (3) /api/v2/settings afterwards shows
satenabled=true and sat_onboarded=true.

Requires the target device to be in a fresh (not yet onboarded) state:
sat_onboarded=false. Does not reset that state itself - the caller resets or
picks a fresh device.

USAGE
  python scripts/verify_sat_onboarding.py --host 192.168.88.64
"""

import argparse
import json
import sys
import time
import urllib.request

sys.path.insert(0, "scripts")
from loadtest_pageload import find_edge, free_port, wait_for_cdp  # noqa: E402

try:
    import websocket
except ImportError:
    print("Missing dependency: pip install websocket-client", file=sys.stderr)
    sys.exit(1)

import base64
import os
import subprocess
import tempfile


def fetch_json(url, timeout=5.0):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


class Cdp:
    def __init__(self, ws_url):
        self.ws = websocket.create_connection(ws_url, timeout=30)
        self.msg_id = 0
        self.responses = {}  # requestId -> {url, status}
        self._pending = {}

    def send(self, method, params=None):
        self.msg_id += 1
        mid = self.msg_id
        self._pending[mid] = None
        self.ws.send(json.dumps({"id": mid, "method": method, "params": params or {}}))
        return mid

    def call(self, method, params=None, timeout=10.0):
        mid = self.send(method, params)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.ws.settimeout(max(0.1, deadline - time.monotonic()))
            raw = self.ws.recv()
            msg = json.loads(raw)
            if msg.get("id") == mid:
                return msg.get("result")
            self._handle_event(msg)
        raise TimeoutError(f"CDP call {method} timed out")

    def _handle_event(self, msg):
        method = msg.get("method")
        params = msg.get("params", {})
        if method == "Network.responseReceived":
            rid = params["requestId"]
            self.responses[rid] = {
                "url": params["response"]["url"],
                "status": params["response"]["status"],
            }

    def drain_events(self, duration=1.0):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            self.ws.settimeout(max(0.05, deadline - time.monotonic()))
            try:
                raw = self.ws.recv()
            except Exception:
                break
            try:
                msg = json.loads(raw)
            except ValueError:
                continue
            if "id" not in msg:
                self._handle_event(msg)

    def evaluate(self, expr):
        result = self.call("Runtime.evaluate", {"expression": expr, "returnByValue": True, "awaitPromise": False})
        return result.get("result", {}).get("value")


def screenshot(cdp, path):
    result = cdp.call("Page.captureScreenshot", {"format": "png"})
    with open(path, "wb") as f:
        f.write(base64.b64decode(result["data"]))
    print(f"screenshot: {path}")


def click_step(cdp, selector):
    # selector is passed through JSON.stringify-safe encoding to survive embedded quotes
    # (e.g. "[data-sa='sys-radiators']") rather than naive single-quote wrapping.
    encoded = json.dumps(selector)
    expr = f"(function(){{var el=document.querySelector({encoded}); if(!el) return 'MISSING'; el.click(); return 'OK';}})()"
    r = cdp.evaluate(expr)
    if r != "OK":
        raise RuntimeError(f"click_step failed for {selector}: {r}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", required=True)
    p.add_argument("--edge-path", default=None)
    p.add_argument("--screenshot-dir", default="build/sat_onboarding_evidence")
    args = p.parse_args()
    os.makedirs(args.screenshot_dir, exist_ok=True)

    pre = fetch_json(f"http://{args.host}/api/v2/settings")["settings"]
    if pre.get("sat_onboarded", {}).get("value"):
        print("ABORT: device is already sat_onboarded=true; this check needs a fresh device.", file=sys.stderr)
        sys.exit(2)

    edge_path = find_edge(args.edge_path)
    port = free_port()
    profile = tempfile.mkdtemp(prefix="otgw_sat_onboard_verify_")
    proc = subprocess.Popen(
        [edge_path, "--headless=new", "--disable-gpu", "--no-first-run", "--no-default-browser-check",
         "--remote-allow-origins=*", f"--remote-debugging-port={port}", f"--user-data-dir={profile}", "about:blank"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    try:
        ws_url = wait_for_cdp(port)
        cdp = Cdp(ws_url)
        cdp.call("Network.enable")
        cdp.call("Page.enable")
        cdp.call("Page.navigate", {"url": f"http://{args.host}/v2.html"})

        # Wait for the app to boot (fetchSettings() etc.), then navigate to the SAT
        # page and flip the enable toggle - that's what actually triggers the wizard
        # for a genuinely fresh (never-onboarded) device (v2.js:3221-3227), NOT an
        # auto-show on load (that path is 'migrate', gated on satenabled already true).
        deadline = time.monotonic() + 20
        booted = False
        while time.monotonic() < deadline:
            cdp.drain_events(0.5)
            if cdp.evaluate("!!document.querySelector('[data-page=sat]')"):
                booted = True
                break
        if not booted:
            print("FAIL: app did not finish booting ([data-page=sat] nav button not found) within 20s", file=sys.stderr)
            sys.exit(1)

        click_step(cdp, "[data-page=sat]")
        deadline = time.monotonic() + 10
        has_toggle = False
        while time.monotonic() < deadline:
            cdp.drain_events(0.5)
            if cdp.evaluate("!!document.getElementById('satEnable')"):
                has_toggle = True
                break
        if not has_toggle:
            print("FAIL: #satEnable not found after showPage('sat')", file=sys.stderr)
            sys.exit(1)
        cdp.drain_events(1.0)  # let fetchSatPage()/satLastData settle before toggling
        click_step(cdp, "#satEnable")

        deadline = time.monotonic() + 10
        shown = False
        while time.monotonic() < deadline:
            cdp.drain_events(0.5)
            if cdp.evaluate("!!document.getElementById('satOnbBack')"):
                shown = True
                break
        if not shown:
            print("FAIL: SAT onboarding wizard did not appear after toggling #satEnable", file=sys.stderr)
            sys.exit(1)

        screenshot(cdp, os.path.join(args.screenshot_dir, "01_welcome.png"))

        steps = [
            ("[data-sa=begin]", "02_system_step.png"),
            ("[data-sa='sys-radiators']", None),
            ("[data-sa=to-mfr]", "03_manufacturer_step.png"),
            ("[data-sa=pick-auto]", None),
            ("[data-sa=to-tuning]", "04_tuning_step.png"),
            ("[data-sa=tune-auto]", None),
            ("[data-sa=to-sources]", "05_sources_step.png"),
        ]
        for sel, shot in steps:
            click_step(cdp, sel)
            cdp.drain_events(0.3)
            if shot:
                screenshot(cdp, os.path.join(args.screenshot_dir, shot))

        # sources screen kicks off loadHealth() (health+sat/status sequential fetch);
        # give it a moment before hitting Finish.
        cdp.drain_events(1.5)
        screenshot(cdp, os.path.join(args.screenshot_dir, "06_sources_health_loaded.png"))
        click_step(cdp, "[data-sa=finish]")

        # Poll for the 'done' screen or a finishErr banner.
        outcome = None
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            cdp.drain_events(0.5)
            if cdp.evaluate("!!document.querySelector(\"h1\") && document.querySelector('h1').textContent.includes('now controlling')"):
                outcome = "done"
                break
            if cdp.evaluate("document.body.innerHTML.includes('SAT was not enabled')"):
                outcome = "error"
                break

        screenshot(cdp, os.path.join(args.screenshot_dir, f"07_final_{outcome or 'timeout'}.png"))

        settings_posts = [r for r in cdp.responses.values() if "v2/settings" in r["url"]]
        status_503s = [r for r in settings_posts if r["status"] == 503]

        print(f"wizard outcome: {outcome}")
        print(f"settings POSTs observed: {len(settings_posts)}, 503s: {len(status_503s)}")
        for r in settings_posts:
            print(f"  {r['status']}  {r['url']}")

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    post = fetch_json(f"http://{args.host}/api/v2/settings")["settings"]
    sat_enabled = post.get("satenabled", {}).get("value")
    sat_onboarded = post.get("sat_onboarded", {}).get("value")
    print(f"post-check: satenabled={sat_enabled} sat_onboarded={sat_onboarded}")

    ok = outcome == "done" and not status_503s and sat_enabled and sat_onboarded
    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
