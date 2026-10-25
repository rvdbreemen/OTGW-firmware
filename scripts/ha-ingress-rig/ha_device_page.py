#!/usr/bin/env python3
"""Screenshot the HA device page of the OTGW and check its "Visit device" link
(configuration_url from MQTT discovery, TASK-1378). Needs the rig from
setup_rig.py with the OTGW publishing to the same broker.

    python scripts/ha-ingress-rig/ha_device_page.py --label fix
"""
import argparse
import asyncio
import json
from pathlib import Path

import websockets
from playwright.sync_api import sync_playwright

from validate_ingress import HA_HTTP, RUNTIME, ha_tokens

RIG = Path(__file__).resolve().parent


async def find_device(token):
    async with websockets.connect(HA_HTTP.replace("http", "ws") + "/api/websocket") as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": token}))
        await ws.recv()
        await ws.send(json.dumps({"id": 1, "type": "config/device_registry/list"}))
        devices = json.loads(await ws.recv())["result"]
    return [d for d in devices if d.get("manufacturer") == "NodoShop"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", required=True)
    args = ap.parse_args()
    state = json.loads((RUNTIME / "rig.json").read_text())
    out = RIG / "out" / args.label
    out.mkdir(parents=True, exist_ok=True)
    tok = ha_tokens(state, HA_HTTP)
    devices = asyncio.run(find_device(tok["access_token"]))
    report = [{"id": d["id"], "name": d.get("name"), "sw_version": d.get("sw_version"),
               "configuration_url": d.get("configuration_url")} for d in devices]
    print(json.dumps(report, indent=1))
    with sync_playwright() as p:
        browser = p.chromium.launch()
        ctx = browser.new_context(viewport={"width": 1400, "height": 900})
        ctx.add_init_script("localStorage.setItem('hassTokens', %s);" % json.dumps(json.dumps(tok)))
        page = ctx.new_page()
        for d in devices:
            page.goto(f"{HA_HTTP}/config/devices/device/{d['id']}", wait_until="domcontentloaded")
            page.wait_for_timeout(6000)
            link = page.get_by_role("link", name="Visit")
            d_rep = next(r for r in report if r["id"] == d["id"])
            d_rep["visit_link"] = link.first.get_attribute("href") if link.count() else None
            page.screenshot(path=str(out / f"ha_device_{d['id'][:8]}.png"))
            if d_rep["visit_link"]:
                # The link opens a new tab on the device itself (not proxied).
                with ctx.expect_page() as new_tab:
                    link.first.click()
                tab = new_tab.value
                tab.wait_for_function(
                    "() => { const e = document.getElementById('devVersion'); return e && e.textContent.indexOf('[') < 0; }",
                    timeout=30000)
                d_rep["visit_opened"] = {"url": tab.url, "devVersion": tab.evaluate("document.getElementById('devVersion').textContent")}
                tab.screenshot(path=str(out / f"ha_visit_{d['id'][:8]}.png"))
                tab.close()
        browser.close()
    (out / "ha_device_page.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
