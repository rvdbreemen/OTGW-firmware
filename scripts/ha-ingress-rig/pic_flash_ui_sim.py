#!/usr/bin/env python3
"""Replay a PIC flash against the classic UI behind HA ingress, without a PIC
write (TASK-1378). The UI code is real; the gateway's answers to the flash
request and to /api/v2/flash/status are scripted in the browser:

  poll 1: not started yet (flash queued in the gateway's loop)
  poll 2: running, 40 %
  poll 3+: done, 100 %, "PIC upgrade was successful"

This is the order a real flash behind a proxy produces, where the WebSocket is
not available and the poll is the page's only source. Pass --index to serve a
local index.js instead of the one on the gateway (old vs fix on one setup).

    python scripts/ha-ingress-rig/pic_flash_ui_sim.py --label old
    python scripts/ha-ingress-rig/pic_flash_ui_sim.py --label fix --index src/OTGW-firmware/data/index.js
"""
import argparse
import json
from pathlib import Path

from playwright.sync_api import sync_playwright

from validate_ingress import HA_HTTPS, RUNTIME, find_ui_frame, ha_tokens

RIG = Path(__file__).resolve().parent
STATES = [
    {"flashing": False, "pic_flashing": False, "pic_progress": 100, "pic_filename": "gateway.hex",
     "pic_error": "PIC upgrade was successful"},  # result of an earlier flash, new one not started
    {"flashing": True, "pic_flashing": True, "pic_progress": 40, "pic_filename": "gateway.hex", "pic_error": ""},
    {"flashing": False, "pic_flashing": False, "pic_progress": 100, "pic_filename": "gateway.hex",
     "pic_error": "PIC upgrade was successful"},
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", required=True)
    ap.add_argument("--index", help="local index.js to serve instead of the gateway's")
    args = ap.parse_args()
    state = json.loads((RUNTIME / "rig.json").read_text())
    out = RIG / "out" / args.label
    out.mkdir(parents=True, exist_ok=True)
    polls = []

    def flash_status(route):
        armed = bool(upgrades)  # before the flash request: report idle
        st = STATES[min(len(polls), len(STATES) - 1)] if armed else dict(STATES[0], pic_progress=0, pic_error="")
        if armed:
            polls.append(st)
        route.fulfill(status=200, content_type="application/json", body=json.dumps({"flashstatus": st}))

    upgrades = []

    def upgrade(route):
        upgrades.append(route.request.url)  # never forwarded: no PIC write
        route.fulfill(status=200, content_type="application/json", body='{"status":"started"}')

    tok = ha_tokens(state, HA_HTTPS)
    with sync_playwright() as p:
        browser = p.chromium.launch()
        ctx = browser.new_context(ignore_https_errors=True, viewport={"width": 1400, "height": 900})
        ctx.route(lambda u: "action=upgrade" in u, upgrade)
        ctx.route(lambda u: "/api/v2/flash/status" in u, flash_status)
        if args.index:
            body = Path(args.index).read_text(encoding="utf-8")
            ctx.route(lambda u: u.split("?")[0].endswith("/index.js"),
                      lambda r: r.fulfill(status=200, content_type="application/javascript", body=body))
        ctx.add_init_script("localStorage.setItem('hassTokens', %s);" % json.dumps(json.dumps(tok)))
        page = ctx.new_page()
        page.goto(f"{HA_HTTPS}/otgw", wait_until="domcontentloaded", timeout=60000)
        frame = find_ui_frame(page, "/api/ingress/otgw/")
        frame.wait_for_function(
            "() => { const e = document.getElementById('devVersion'); return e && e.textContent.indexOf('[') < 0; }",
            timeout=30000)
        frame.evaluate("document.querySelector('.tabPICflash').click()")
        frame.wait_for_selector("[id='firmware_version_gateway.hex']", state="attached", timeout=30000)
        frame.evaluate("""() => { const row = document.getElementById('firmware_version_gateway.hex').parentElement;
                                   row.querySelector("img[title='Install firmware onto pic']").click(); }""")
        texts = []
        for _ in range(30):  # 5 s WebSocket wait + polls every 5 s
            page.wait_for_timeout(1000)
            t = frame.evaluate("(document.getElementById('flashPercentageText') || {}).textContent || null")
            if not texts or texts[-1] != t:
                texts.append(t)
        rep = {
            "label": args.label,
            "index_js": args.index or "from gateway",
            "upgrade_requests": upgrades,
            "polls_answered_after_start": len(polls),
            "progress_texts_seen": texts,
            "progress_text": frame.evaluate("(document.getElementById('flashPercentageText') || {}).textContent || null"),
            "page_disabled": frame.evaluate("document.body.classList.contains('disable-interaction')"),
        }
        page.screenshot(path=str(out / "pic_flash_ui_sim.png"))
        browser.close()
    (out / "pic_flash_ui_sim.json").write_text(json.dumps(rep, indent=2))
    print(json.dumps(rep, indent=1))


if __name__ == "__main__":
    main()
