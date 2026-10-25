#!/usr/bin/env python3
"""REAL PIC flash through the HA ingress panel, using the classic UI's own flash
icon (TASK-1378). This writes the PIC. Run it only on the maintainer's explicit
order for a named file; an interrupted PIC flash can brick the PIC.

Only an upgrade request for --file may leave the browser; any other upgrade
request is answered locally and the run stops.

    python scripts/ha-ingress-rig/pic_flash_via_proxy.py --file gateway.hex --label pic-real
"""
import argparse
import json
import time
import urllib.parse
import urllib.request
from pathlib import Path

from playwright.sync_api import sync_playwright

from validate_ingress import HA_HTTPS, RUNTIME, find_ui_frame, ha_tokens

RIG = Path(__file__).resolve().parent


def device(dev, path):
    try:
        with urllib.request.urlopen(f"http://{dev}{path}", timeout=5) as r:
            return json.loads(r.read().decode())
    except Exception as e:  # noqa: BLE001
        return {"error": str(e)[:120]}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--file", required=True, help="PIC hex file on the gateway, e.g. gateway.hex")
    ap.add_argument("--label", required=True)
    args = ap.parse_args()
    state = json.loads((RUNTIME / "rig.json").read_text())
    dev = state["device"]
    out = RIG / "out" / args.label
    out.mkdir(parents=True, exist_ok=True)
    before = device(dev, "/api/v2/device/info").get("device", {})
    rep = {"file": args.file, "pic_before": {k: before.get(k) for k in ("picfwtype", "picfwversion", "picdeviceid")}}
    sent, refused, polls = [], [], []

    def guard(route):
        q = urllib.parse.parse_qs(urllib.parse.urlparse(route.request.url).query)
        if q.get("name") == [args.file]:
            sent.append(route.request.url)
            route.continue_()
        else:
            refused.append(route.request.url)
            route.fulfill(status=200, content_type="application/json", body='{"status":"refused by test"}')

    tok = ha_tokens(state, HA_HTTPS)
    with sync_playwright() as p:
        browser = p.chromium.launch()
        ctx = browser.new_context(ignore_https_errors=True, viewport={"width": 1400, "height": 900})
        ctx.route(lambda u: "action=upgrade" in u, guard)
        ctx.add_init_script("localStorage.setItem('hassTokens', %s);" % json.dumps(json.dumps(tok)))
        page = ctx.new_page()
        page.on("response", lambda r: polls.append((round(time.time(), 1), r.status))
                if "flash/status" in r.url else None)
        page.goto(f"{HA_HTTPS}/otgw", wait_until="domcontentloaded", timeout=60000)
        frame = find_ui_frame(page, "/api/ingress/otgw/")
        frame.wait_for_function(
            "() => { const e = document.getElementById('devVersion'); return e && e.textContent.indexOf('[') < 0; }",
            timeout=30000)
        frame.evaluate("document.querySelector('.tabPICflash').click()")
        frame.wait_for_selector(f"[id='firmware_version_{args.file}']", state="attached", timeout=30000)
        # The flash icon in the row of --file, and only that one.
        icons = frame.evaluate_handle(
            "(name) => { const row = document.getElementById('firmware_version_' + name).parentElement;"
            " return Array.from(row.querySelectorAll(\"img[title='Install firmware onto pic']\")); }", args.file)
        count = frame.evaluate("(a) => a.length", icons)
        if count != 1:
            raise SystemExit(f"expected 1 flash icon in the {args.file} row, found {count}; not flashing")
        t0 = time.time()
        frame.evaluate("(a) => a[0].click()", icons)
        done = None
        for _ in range(120):  # up to ~6 min
            page.wait_for_timeout(3000)
            fs = device(dev, "/api/v2/flash/status").get("flashstatus", {})
            if refused:
                done = "refused"
                break
            if sent and fs and not fs.get("pic_flashing") and time.time() - t0 > 10:
                done = {"pic_progress": fs.get("pic_progress"), "pic_error": fs.get("pic_error"),
                        "pic_filename": fs.get("pic_filename")}
                break
        rep["seconds"] = round(time.time() - t0, 1)
        rep["progress_text"] = frame.evaluate(
            "(document.getElementById('flashPercentageText') || {}).textContent || null")
        page.screenshot(path=str(out / "pic_flash_via_ingress.png"))
        browser.close()
    rep.update({"upgrade_sent": sent, "upgrade_refused": refused, "final_status": done,
                "ui_flash_status_polls": len(polls),
                "ui_flash_status_codes": sorted({s for _, s in polls})})
    time.sleep(15)  # let the gateway re-read the PIC version
    after = device(dev, "/api/v2/device/info").get("device", {})
    rep["pic_after"] = {k: after.get(k) for k in ("picfwtype", "picfwversion", "picdeviceid")}
    (out / "pic_flash_via_ingress.json").write_text(json.dumps(rep, indent=2))
    print(json.dumps(rep, indent=1))


if __name__ == "__main__":
    main()
