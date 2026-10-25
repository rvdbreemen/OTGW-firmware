#!/usr/bin/env python3
"""Validate the OTGW v2 web UI direct, behind HA ingress and behind a
generic reverse proxy (TASK-1378). Needs the rig from setup_rig.py.

Per scenario it opens the UI in headless Chromium and records:
  - loaded:     the header shows the firmware version (the REST reads worked)
  - escaped:    requests from the UI that left the proxy base path (must be 0)
  - api_bad:    /api/ responses that were not 2xx
  - write:      HTTP status of a settings POST made with the UI's own APIGW
  - upload:     HTTP status of an upload to the page-relative upload endpoint
  - fsx_forms:  FSexplorer form targets that leave the base path (must be 0)
  - websockets: the live-log WebSocket URL and whether frames arrived
plus a screenshot per scenario in out/<label>/. Echo panels record the headers
the proxy sends to the device. Exit code 1 when a required check fails.

    python scripts/ha-ingress-rig/validate_ingress.py --label old
    python scripts/ha-ingress-rig/validate_ingress.py --label fix --password <device admin pw>
"""
import argparse
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

from playwright.sync_api import sync_playwright

RIG = Path(__file__).resolve().parent
RUNTIME = RIG / ".runtime"
HA_HTTP = "http://localhost:18123"
HA_HTTPS = "https://localhost:18443"


def ha_tokens(state, base):
    """Log in through the HA auth flow and return the hassTokens blob the frontend stores."""
    client_id = base + "/"

    def post(url, data, form=False):
        body = urllib.parse.urlencode(data).encode() if form else json.dumps(data).encode()
        ctype = "application/x-www-form-urlencoded" if form else "application/json"
        req = urllib.request.Request(url, data=body, method="POST", headers={"Content-Type": ctype})
        import ssl
        # Rig only: HA on localhost behind a self-signed certificate made by setup_rig.py.
        ctx = ssl._create_unverified_context()
        with urllib.request.urlopen(req, timeout=30, context=ctx) as r:
            return json.loads(r.read().decode())

    flow = post(base + "/auth/login_flow", {"client_id": client_id, "handler": ["homeassistant", None],
                                            "redirect_uri": client_id})
    res = post(base + "/auth/login_flow/" + flow["flow_id"], {"username": state["username"],
                                                               "password": state["password"],
                                                               "client_id": client_id})
    tok = post(base + "/auth/token", {"grant_type": "authorization_code", "code": res["result"],
                                      "client_id": client_id}, form=True)
    tok.update({"hassUrl": base, "clientId": client_id, "expires": int(time.time() * 1000) + tok["expires_in"] * 1000})
    return tok


def find_ui_frame(page, needle, timeout_s=40):
    end = time.time() + timeout_s
    while time.time() < end:
        for f in page.frames:
            if needle in f.url:
                return f
        page.wait_for_timeout(500)
    return None


def probe_ui(frame, base_path, password):
    """Checks that run inside the v2 UI frame (direct page or ingress iframe)."""
    out = {}
    try:
        frame.wait_for_function(
            "() => { const e = document.getElementById('hdrHost'); return e && / v[0-9]/.test(e.textContent); }",
            timeout=30000)
        out["loaded"] = True
        out["header"] = frame.evaluate("document.getElementById('hdrHost').textContent")
    except Exception as e:  # noqa: BLE001
        out["loaded"] = False
        out["load_error"] = str(e)[:200]
    frame.wait_for_timeout(8000)  # WebSocket connect, first frames, REST polls
    # Settings write relative to the page (the proxy path); value is written back unchanged.
    out["write"] = frame.evaluate("""async () => {
        const api = new URL('api/v2/settings', document.baseURI).href;
        const get = await fetch(api);
        if (!get.ok) return {status: get.status, step: 'read', api: api};
        const j = await get.json();
        const cur = (j.settings || {}).darktheme;
        if (!cur) return {status: -1, step: 'no darktheme setting', api: api};
        const post = await fetch(api, {method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({name: 'darktheme', value: String(cur.value)})});
        return {status: post.status, api: api};
    }""")
    out["upload"] = frame.evaluate("""async () => {
        const fd = new FormData();
        fd.append('file', new Blob(['rig probe ' + Date.now()], {type: 'text/plain'}), 'rigprobe.txt');
        const up = new URL('upload', document.baseURI).href;
        const r = await fetch(up, {method: 'POST', body: fd, redirect: 'manual'});
        const del = await fetch(new URL('api/listfiles?delete=' + encodeURIComponent('/rigprobe.txt'), document.baseURI).href);
        return {status: r.status, type: r.type, delete_status: del.status, url: up};
    }""")
    return out


def run_scenario(browser, label, name, url, base_path, kind, state, outdir, password, tokens=None):
    ctx_args = {"ignore_https_errors": True, "viewport": {"width": 1400, "height": 900}}
    if password:
        ctx_args["http_credentials"] = {"username": "admin", "password": password}
    ctx = browser.new_context(**ctx_args)
    if tokens:
        ctx.add_init_script("localStorage.setItem('hassTokens', %s);" % json.dumps(json.dumps(tokens)))
    page = ctx.new_page()
    reqs, resps, console = [], [], []
    page.on("request", lambda r: reqs.append(r.url))
    page.on("response", lambda r: resps.append((r.url, r.status)))
    page.on("console", lambda m: console.append(m.text[:200]))
    sockets = []

    def on_ws(ws):
        rec = {"url": ws.url, "frames": 0, "closed": False, "error": None}
        ws.on("framereceived", lambda _p: rec.__setitem__("frames", rec["frames"] + 1))
        ws.on("close", lambda _w: rec.__setitem__("closed", True))
        ws.on("socketerror", lambda e: rec.__setitem__("error", str(e)[:120]))
        sockets.append(rec)

    page.on("websocket", on_ws)
    result = {"scenario": name, "url": url}
    try:
        page.goto(url, wait_until="domcontentloaded", timeout=60000)
        if kind == "echo":
            frame = find_ui_frame(page, "/api/ingress/")
            frame.wait_for_function("() => document.getElementById('out') && document.getElementById('out').textContent.startsWith('{')",
                                    timeout=30000)
            result["seen_by_backend"] = json.loads(frame.evaluate("document.getElementById('out').textContent"))
        else:
            frame = page if kind == "direct" else find_ui_frame(page, base_path)
            if frame is None:
                result["error"] = "UI frame not found"
            else:
                try:
                    result.update(probe_ui(frame, base_path, password))
                except Exception as e:  # noqa: BLE001  the HA panel may reload its iframe once
                    if "detached" not in str(e):
                        raise
                    frame = find_ui_frame(page, base_path)
                    result.update(probe_ui(frame, base_path, password))
                page.screenshot(path=str(outdir / f"{name}.png"), full_page=False)
                # FSexplorer form targets resolved against its own URL. The UI page is
                # closed first, as when the user navigates to FSexplorer: the ESP8266
                # web server serves one client at a time and the UI keeps polling.
                fsx_url = urllib.parse.urljoin(frame.url, "FSexplorer.html")
                fsx = page.context.new_page()
                fsx.on("request", lambda r: reqs.append(r.url))
                page.close()
                fsx.goto(fsx_url, wait_until="domcontentloaded", timeout=30000)
                targets = fsx.evaluate("Array.from(document.forms).map(f => f.action)")
                result["fsx_forms_escaped"] = [t for t in targets
                                               if urllib.parse.urlparse(t).path and not urllib.parse.urlparse(t).path.startswith(base_path)]
                fsx.close()
        if not page.is_closed():
            page.screenshot(path=str(outdir / f"{name}.png"), full_page=False)
    except Exception as e:  # noqa: BLE001
        result["error"] = str(e)[:300]
        try:
            if not page.is_closed():
                page.screenshot(path=str(outdir / f"{name}.png"))
        except Exception:  # noqa: BLE001
            pass
    host = urllib.parse.urlparse(url).netloc
    result["escaped"] = sorted({u for u in reqs
                                if urllib.parse.urlparse(u).netloc == host
                                and base_path != "/"
                                and not urllib.parse.urlparse(u).path.startswith(base_path)
                                and kind != "echo"
                                and "/api/ingress/" not in u
                                and not _is_ha_frontend(u)})
    # 503 is the device's busy gate (ADR-147/165); the UI retries it, so it is counted, not failed.
    result["api_bad"] = sorted({f"{s} {u}" for u, s in resps if "/api/v2/" in u and not (200 <= s < 300) and s != 503})
    result["api_503"] = sum(1 for u, s in resps if "/api/v2/" in u and s == 503)
    result["console_errors"] = [c for c in console if "rror" in c][:8]
    result["websockets"] = sockets
    # A live-log socket on another host, port or path than the page is one that left the proxy.
    result["ws_escaped"] = [w["url"] for w in sockets if kind != "direct"
                            and not urllib.parse.urlparse(w["url"]).path.startswith("/api/websocket")  # HA's own
                            and (urllib.parse.urlparse(w["url"]).netloc != host
                                 or not urllib.parse.urlparse(w["url"]).path.startswith(base_path))]
    ctx.close()
    return result


def device_heap(dev):
    """Heap, REST gate and in-flight counters straight from the device after a scenario."""
    try:
        with urllib.request.urlopen(f"http://{dev}/api/v2/device/info", timeout=5) as r:
            d = json.loads(r.read().decode())["device"]
        keys = ("freeheap", "maxfreeblock", "hd_rest_503", "hd_webfile_503", "hd_webconn_503",
                "hd_rest_inflight_hwm", "hd_webfile_inflight_hwm", "hd_ws_drops")
        return {k: d.get(k) for k in keys}
    except Exception as e:  # noqa: BLE001
        return {"error": str(e)[:100]}


def _is_ha_frontend(u):
    p = urllib.parse.urlparse(u).path
    return (p.startswith(("/frontend_", "/static/", "/auth/", "/manifest.json", "/sw-modern.js", "/api/websocket",
                          "/local/", "/api/hassio", "/hacsfiles/", "/api/ingress/", "/files/ingress/"))
            or p in ("/", "/otgw", "/otgw_hostdev", "/otgw_rw", "/echo", "/echo_hostdev", "/favicon.ico", "/api/")
            or p.startswith("/api/") and not p.startswith("/api/v2/"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", required=True, help="run label, e.g. old or fix (output dir name)")
    ap.add_argument("--password", default="", help="device admin password, if one is set on the device")
    ap.add_argument("--only", default="", help="comma-separated scenario names to run")
    args = ap.parse_args()
    state = json.loads((RUNTIME / "rig.json").read_text())
    dev = state["device"]
    outdir = RIG / "out" / args.label
    outdir.mkdir(parents=True, exist_ok=True)

    tok_http = ha_tokens(state, HA_HTTP)
    tok_https = ha_tokens(state, HA_HTTPS)
    scenarios = [
        ("direct", f"http://{dev}/", "/", "direct", None),
        ("ha_http_otgw", f"{HA_HTTP}/otgw", "/api/ingress/otgw/", "ingress", tok_http),
        ("ha_https_otgw", f"{HA_HTTPS}/otgw", "/api/ingress/otgw/", "ingress", tok_https),
        ("ha_https_otgw_hostdev", f"{HA_HTTPS}/otgw_hostdev", "/api/ingress/otgw_hostdev/", "ingress", tok_https),
        ("ha_https_otgw_rw", f"{HA_HTTPS}/otgw_rw", "/api/ingress/otgw_rw/", "ingress", tok_https),
        ("nginx_https_keephost", "https://localhost:19443/otgw-keephost/", "/otgw-keephost/", "proxy", None),
        ("nginx_https_default", "https://localhost:19443/otgw/", "/otgw/", "proxy", None),
        ("nginx_http_keephost", "http://localhost:19080/otgw-keephost/", "/otgw-keephost/", "proxy", None),
        ("ha_https_echo", f"{HA_HTTPS}/echo", "/api/ingress/echo/", "echo", tok_https),
        ("ha_https_echo_hostdev", f"{HA_HTTPS}/echo_hostdev", "/api/ingress/echo_hostdev/", "echo", tok_https),
    ]
    only = {s for s in args.only.split(",") if s}
    results = []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        for name, url, base, kind, tok in scenarios:
            if only and name not in only:
                continue
            print("==", name, url, flush=True)
            r = run_scenario(browser, args.label, name, url, base, kind, state, outdir, args.password, tok)
            r["device_heap_after"] = device_heap(dev)
            print(json.dumps(r, indent=1)[:2500], flush=True)
            results.append(r)
        browser.close()
    (outdir / "results.json").write_text(json.dumps(results, indent=2))

    # Required checks: the recommended setups must fully work.
    required = {"direct", "ha_http_otgw", "ha_https_otgw", "nginx_https_keephost", "nginx_http_keephost"}
    failed = []
    for r in results:
        if r["scenario"] not in required:
            continue
        ok = (r.get("loaded") and not r.get("escaped") and not r.get("api_bad")
              and r.get("write", {}).get("status") == 200
              and r.get("upload", {}).get("status") in (200, 303, 0)
              and not r.get("fsx_forms_escaped"))
        ws_ok = any(w["frames"] > 0 and not w["error"] for w in r.get("websockets", []))
        r["live_log_frames"] = ws_ok
        if r["scenario"] == "direct":
            ok = ok and ws_ok
        print(("PASS " if ok else "FAIL ") + r["scenario"])
        if not ok:
            failed.append(r["scenario"])
    print("results:", outdir / "results.json")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
