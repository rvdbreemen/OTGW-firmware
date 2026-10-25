#!/usr/bin/env python3
"""Flash firmware through the HA ingress panel, using the device's own update
page, to see whether OTA survives the proxy (TASK-1378). Needs the rig from
setup_rig.py. Writes a screenshot and the page log to out/ota/.

    python scripts/ha-ingress-rig/ota_via_ingress.py --firmware build/<name>.ino.bin

Recovery if the upload drops: see docs/guides/REVERSE_PROXY_AND_HA_INGRESS.md.
"""
import argparse
import json
import time
import urllib.request
from pathlib import Path

from playwright.sync_api import sync_playwright

from validate_ingress import HA_HTTPS, RUNTIME, find_ui_frame, ha_tokens

RIG = Path(__file__).resolve().parent


def device_info(dev):
    """(fwversion, uptime in minutes) from the device directly, or (None, None) while it reboots."""
    try:
        with urllib.request.urlopen(f"http://{dev}/api/v2/device/info", timeout=4) as r:
            d = json.loads(r.read().decode())["device"]
    except Exception:  # noqa: BLE001  device rebooting
        return None, None
    days, _, hm = d.get("uptime", "0(d)-0:0(H:m)").partition("(d)-")
    h, _, m = hm.split("(")[0].partition(":")
    return d.get("fwversion"), int(days) * 1440 + int(h) * 60 + int(m)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--firmware", required=True)
    ap.add_argument("--panel", default="otgw")
    args = ap.parse_args()
    state = json.loads((RUNTIME / "rig.json").read_text())
    dev = state["device"]
    out = RIG / "out" / "ota"
    out.mkdir(parents=True, exist_ok=True)
    log = []
    before, up_before = device_info(dev)
    tok = ha_tokens(state, HA_HTTPS)
    with sync_playwright() as p:
        browser = p.chromium.launch()
        ctx = browser.new_context(ignore_https_errors=True, viewport={"width": 1400, "height": 900})
        ctx.add_init_script("localStorage.setItem('hassTokens', %s);" % json.dumps(json.dumps(tok)))
        page = ctx.new_page()
        page.on("console", lambda m: log.append(m.text[:300]))
        page.goto(f"{HA_HTTPS}/{args.panel}", wait_until="domcontentloaded", timeout=60000)
        frame = find_ui_frame(page, f"/api/ingress/{args.panel}/")
        frame.goto(frame.url.split("#")[0].rsplit("/", 1)[0] + "/update", wait_until="domcontentloaded")
        frame.set_input_files("#fwForm input[name='firmware']", args.firmware)
        t0 = time.time()
        frame.click("#fwSubmit")
        result = "timeout"
        for _ in range(120):
            page.wait_for_timeout(2000)
            try:
                text = frame.evaluate("document.body.innerText")
            except Exception:  # noqa: BLE001  frame navigated (redirect to the UI)
                text = ""
            v, up = device_info(dev)
            # Flashing the same build keeps the version, so a reboot (uptime back
            # to near zero) after the upload is the success signal.
            if "back online" in text.lower() or (v and (v != before or up < up_before) and time.time() - t0 > 20):
                result = "ok"
                break
            on_update_page = "/update" in frame.url if not frame.is_detached() else False
            if on_update_page and "failed" in text.lower():
                result = "failed: " + text[-300:]
                break
        page.screenshot(path=str(out / "ota_via_ingress.png"))
        browser.close()
    after, up_after = None, None
    for _ in range(30):
        after, up_after = device_info(dev)
        if after:
            break
        time.sleep(3)
    rep = {"result": result, "seconds": round(time.time() - t0, 1), "before": before, "after": after,
           "uptime_min_before": up_before, "uptime_min_after": up_after,
           "console_tail": log[-15:]}
    (out / "ota_via_ingress.json").write_text(json.dumps(rep, indent=2))
    print(json.dumps(rep, indent=1))


if __name__ == "__main__":
    main()
