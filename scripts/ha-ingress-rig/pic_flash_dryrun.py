#!/usr/bin/env python3
"""Dry run of the PIC flash flow of the classic UI through a proxy (TASK-1378).

The PIC page is opened through the proxy and the real UI code is made to start
a flash by clicking the flash icon, but the browser never sends the
`/pic?action=upgrade` request: a context-wide route answers it locally with
{"status":"started"}. Nothing is written to the PIC. Everything else (file list,
icons, progress polling) goes to the device through the proxy and is recorded.

    python scripts/ha-ingress-rig/pic_flash_dryrun.py --label check
"""
import argparse
import json
import urllib.parse
from pathlib import Path

from playwright.sync_api import sync_playwright

from validate_ingress import HA_HTTPS, RUNTIME, find_ui_frame, ha_tokens

RIG = Path(__file__).resolve().parent
SCENARIOS = [
    ("ha_https_otgw", f"{HA_HTTPS}/otgw", "/api/ingress/otgw/", True),
    ("nginx_https_keephost", "https://localhost:19443/otgw-keephost/", "/otgw-keephost/", False),
]


def run(browser, name, url, base, ingress, tok, outdir):
    ctx = browser.new_context(ignore_https_errors=True, viewport={"width": 1400, "height": 900})
    blocked, reqs, resps = [], [], []

    def block_upgrade(route):
        # The safety net: a PIC write never leaves the browser.
        blocked.append(route.request.url)
        route.fulfill(status=200, content_type="application/json", body='{"status":"started"}')

    ctx.route(lambda u: "action=upgrade" in u, block_upgrade)
    if tok:
        ctx.add_init_script("localStorage.setItem('hassTokens', %s);" % json.dumps(json.dumps(tok)))
    page = ctx.new_page()
    page.on("request", lambda r: reqs.append(r.url))
    page.on("response", lambda r: resps.append((r.url, r.status)))
    res = {"scenario": name}
    page.goto(url, wait_until="domcontentloaded", timeout=60000)
    frame = find_ui_frame(page, base) if ingress else page
    frame.wait_for_function("() => { const e = document.getElementById('devVersion'); return e && e.textContent.indexOf('[') < 0; }", timeout=30000)
    # The menu entry sits in a hover dropdown; fire its click handler directly.
    frame.evaluate("document.querySelector('.tabPICflash').click()")
    frame.wait_for_selector("#displayPICpage img[title='Install firmware onto pic']", state="attached", timeout=30000)
    res["files_listed"] = frame.evaluate(
        "Array.from(document.querySelectorAll('#displayPICpage [id^=firmware_version_]')).map(e => e.id.replace('firmware_version_',''))")
    res["icon_srcs"] = frame.evaluate(
        "Array.from(document.querySelectorAll('#displayPICpage img.firmware-icon')).slice(0,2).map(i => i.src)")
    page.screenshot(path=str(outdir / f"pic_{name}_page.png"))
    # Canary: a harmless URL carrying the same marker must come back from the
    # route, not from the device, before the real flash icon is clicked.
    canary = frame.evaluate("fetch(localURL + '/favicon.ico?action=upgrade&canary=1').then(r => r.text())")
    blocked.clear()
    if canary != '{"status":"started"}':
        raise SystemExit("interception not active, refusing to click the flash icon")
    frame.locator("#displayPICpage img[title='Install firmware onto pic']").first.click()
    page.wait_for_timeout(15000)  # WebSocket attempt (up to 5 s), then upgrade call + status polls
    res["progress_text"] = frame.evaluate(
        "(document.getElementById('flashPercentageText') || {}).textContent || null")
    page.screenshot(path=str(outdir / f"pic_{name}_after_click.png"))
    res["upgrade_requests_blocked"] = blocked
    res["upgrade_reached_network"] = [u for u, s in resps if "action=upgrade" in u and s != 200] or []
    res["flash_status_polls"] = sorted({f"{s} {urllib.parse.urlparse(u).path}" for u, s in resps if "flash/status" in u})
    res["firmware_files"] = sorted({f"{s} {urllib.parse.urlparse(u).path}" for u, s in resps if "firmware/files" in u})
    host = urllib.parse.urlparse(url).netloc
    res["escaped"] = sorted({u for u in reqs if urllib.parse.urlparse(u).netloc == host
                             and "/pic" in u and not urllib.parse.urlparse(u).path.startswith(base)})
    ctx.close()
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", required=True)
    args = ap.parse_args()
    state = json.loads((RUNTIME / "rig.json").read_text())
    outdir = RIG / "out" / args.label
    outdir.mkdir(parents=True, exist_ok=True)
    tok = ha_tokens(state, HA_HTTPS)
    results = []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        for name, url, base, ingress in SCENARIOS:
            r = run(browser, name, url, base, ingress, tok if ingress else None, outdir)
            print(json.dumps(r, indent=1), flush=True)
            results.append(r)
        browser.close()
    (outdir / "pic_flash_dryrun.json").write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
