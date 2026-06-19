#!/usr/bin/env python3
"""ACTIVE provisioning: point the OTGW-under-test at the MQTT broker, then reboot.

Before a capture run can observe MQTT traffic, the device itself must be connected
to the broker. This uploads the broker host/port/user/password from the out-of-repo
capture-settings.json to the device via the REST API (POST /api/v2/settings, one
{"name","value"} per field), enables MQTT, then reboots the device so the settings
are flushed and a clean broker connection is made. It finally polls /api/v2/health
until mqttconnected is true.

This is an ACTIVE test/provisioning step — it DRIVES the device. It is kept separate
from the passive capture scripts (which only monitor). otgw-test.py runs it before
starting the monitors. The capture tool's own telnet reconnect handles re-attaching
after the reboot.

Usage:
  python scripts/tests/provision_mqtt.py [--host OTGW.local] [--timeout 40]
"""
import argparse
import json
import os
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
try:
    import _secrets
except Exception:
    _secrets = None


def _api(base, path, method="GET", body=None, timeout=10):
    url = base + path
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={"Content-Type": "application/json",
                                          "Connection": "close"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")
    except Exception as e:
        return 0, type(e).__name__


def set_setting(base, name, value):
    code, resp = _api(base, "/api/v2/settings", "POST", {"name": name, "value": value})
    ok = (code == 200)
    shown = "***" if "passwd" in name.lower() else value
    print(f"  set {name:16} = {shown!r:24} -> {code} {'OK' if ok else resp[:80]}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=None)
    ap.add_argument("--timeout", type=float, default=45.0, help="seconds to wait for mqttconnected after reboot")
    ap.add_argument("--no-reboot", action="store_true")
    args = ap.parse_args()

    g = (lambda k, d=None: _secrets.get(k, d)) if _secrets else (lambda k, d=None: d)
    host = args.host or g("device_host", "OTGW.local")
    base = f"http://{host}"
    # Prefer the real broker; fall back to the laptop test-rig when it is unreachable.
    if _secrets:
        broker, port, used_fb = _secrets.resolve_broker()
        if used_fb:
            print(f"# real broker unreachable -> falling back to test-rig {broker}:{port}")
    else:
        broker, port = "homeassistant.local", 1883
    user = g("mqtt_user", "") or ""
    pw = _secrets.mqtt_password() if _secrets else ""

    print(f"# provision MQTT on {base}: broker={broker}:{port} user={user!r} pw={'set' if pw else 'EMPTY'}")
    if not pw:
        print("# WARNING: no MQTT password in capture-settings.json — run capture-mqtt-debug.bat -SaveSecrets once")

    # reachable?
    code, _ = _api(base, "/api/v2/health", timeout=8)
    if code != 200:
        print(f"# device not reachable (health => {code}). Is it on the LAN / provisioned for WiFi?")
        return 2

    ok = True
    ok &= set_setting(base, "MQTTbroker", broker)
    ok &= set_setting(base, "MQTTbrokerPort", port)
    ok &= set_setting(base, "MQTTuser", user)
    ok &= set_setting(base, "MQTTpasswd", pw)
    ok &= set_setting(base, "MQTTenable", True)
    if not ok:
        print("# one or more settings POSTs failed")
        return 1

    if args.no_reboot:
        print("# --no-reboot: settings staged (2s debounce flush); MQTT may connect without a reboot")
    else:
        print("# rebooting device (/ReBoot) to flush settings + make a clean broker connection...")
        code, _ = _api(base, "/ReBoot", "POST", timeout=8)
        print(f"  /ReBoot -> {code}")
        print("# waiting ~16s for reboot + WiFi rejoin...")
        time.sleep(16)

    # poll health until MQTT is connected
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        code, resp = _api(base, "/api/v2/health", timeout=8)
        if code == 200:
            try:
                h = json.loads(resp)["health"]
                if h.get("mqttconnected"):
                    print(f"# MQTT CONNECTED (uptime {h.get('uptime')}, heap {h.get('heap')})")
                    return 0
                print(f"  health: mqttconnected={h.get('mqttconnected')} uptime={h.get('uptime')} (waiting)")
            except Exception:
                pass
        time.sleep(4)
    print("# TIMEOUT: device did not report mqttconnected=true. Check broker host/creds reachability.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
