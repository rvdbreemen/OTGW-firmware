#!/usr/bin/env python3
"""Provision an OTGW (ESP32/ESP8266) onto WiFi via the WiFiManager config portal.

Why this exists: after a full USB (erase) flash the device loses its stored WiFi
credentials (they live in the ESP NVS, written by WiFiManager + WiFi.persistent,
NOT in any file on LittleFS), so it boots into its open config-portal AP
(SSID "OTGW-<MAC>"). WiFiManager 2.0.17 exposes a callable HTTP API on that
portal:  GET/POST http://192.168.4.1/wifisave  with args  s=<ssid> p=<password>
(see WiFiManager::handleWifiSave -> server->arg("s")/arg("p")). This tool drives
that endpoint headlessly so you do not have to click through the captive portal.

SECRETS: credentials are read ONLY from the out-of-repo store
%LOCALAPPDATA%/OTGW-capture/capture-settings.json (keys WifiSsid/WifiPassword),
or from --ssid/--password. They are NEVER written into the repo and never printed.

Windows only (uses netsh to hop WiFi). It saves the current WiFi, joins the open
OTGW AP, posts the creds, then restores your WiFi. If anything fails mid-run it
prints the exact netsh command to manually rejoin your network.

Usage:
  python bin/provision-wifi-ap.py                 # creds from _secrets, auto-find AP
  python bin/provision-wifi-ap.py --ap OTGW-10:20:BA:21:B4:F8
  python bin/provision-wifi-ap.py --home Koekie-boven --dry-run
"""
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.parse
import urllib.request

PORTAL_IP = "192.168.4.1"


def log(msg):
    print(f"[provision-wifi] {msg}", flush=True)


def load_secrets():
    """WiFi creds from the out-of-repo store. Returns (ssid, password) or (None, None)."""
    p = os.path.join(os.environ.get("LOCALAPPDATA", ""), "OTGW-capture", "capture-settings.json")
    if not os.path.isfile(p):
        return None, None
    try:
        d = json.load(open(p, encoding="utf-8"))
    except (OSError, ValueError):
        return None, None
    ssid = d.get("WifiSsid") or d.get("wifi_ssid")
    pw = d.get("WifiPassword") or d.get("wifi_password")
    return ssid, pw


def netsh(*args, check=False):
    return subprocess.run(["netsh", *args], capture_output=True, text=True, check=check)


def current_ssid():
    out = netsh("wlan", "show", "interfaces").stdout
    for line in out.splitlines():
        s = line.strip()
        # Match "SSID                   : Foo" but not "BSSID"
        if s.lower().startswith("ssid") and not s.lower().startswith("bssid") and ":" in s:
            return s.split(":", 1)[1].strip()
    return None


def scan_otgw_ap():
    """Return the first visible open OTGW config-portal SSID, or None."""
    out = netsh("wlan", "show", "networks").stdout
    found = []
    for line in out.splitlines():
        s = line.strip()
        if s.lower().startswith("ssid") and not s.lower().startswith("bssid") and ":" in s:
            name = s.split(":", 1)[1].strip()
            if name.upper().startswith("OTGW-"):
                found.append(name)
    return found[0] if found else None


def open_profile_xml(ssid):
    """A minimal WLAN profile for an OPEN (no-auth) network, SSID hex-encoded so
    colons/specials in 'OTGW-AA:BB:..' survive netsh."""
    ssid_hex = ssid.encode("utf-8").hex().upper()
    return f"""<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
  <name>{ssid}</name>
  <SSIDConfig><SSID><hex>{ssid_hex}</hex><name>{ssid}</name></SSID></SSIDConfig>
  <connectionType>ESS</connectionType>
  <connectionMode>manual</connectionMode>
  <MSM><security>
    <authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption>
  </security></MSM>
</WLANProfile>
"""


def connect_ssid(ssid, profile_xml=None, timeout=25):
    if profile_xml is not None:
        # Write the temp profile OUTSIDE the repo (TEMP), add, then delete the file.
        tmp = os.path.join(os.environ.get("TEMP", "."), "otgw_ap_profile.xml")
        with open(tmp, "w", encoding="utf-8") as fh:
            fh.write(profile_xml)
        try:
            netsh("wlan", "add", "profile", f"filename={tmp}", "user=all")
        finally:
            try:
                os.remove(tmp)
            except OSError:
                pass
    netsh("wlan", "connect", f"name={ssid}", f"ssid={ssid}")
    deadline = time.time() + timeout
    while time.time() < deadline:
        if current_ssid() == ssid:
            return True
        time.sleep(2)
    return current_ssid() == ssid


def post_wifisave(ssid, password, timeout=10):
    """Call WiFiManager /wifisave with s=<ssid> p=<password> (URL-encoded)."""
    data = urllib.parse.urlencode({"s": ssid, "p": password}).encode()
    url = f"http://{PORTAL_IP}/wifisave"
    req = urllib.request.Request(url, data=data, method="POST")  # 2.0.17 form is POST; arg() also reads GET
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read(200).decode(errors="replace")
    except Exception as e:  # noqa: BLE001 - report any transport error verbatim
        return None, str(e)


def main():
    ap = argparse.ArgumentParser(description="Provision an OTGW onto WiFi via the WiFiManager config portal.")
    ap.add_argument("--ssid", help="Target WiFi SSID (default: from _secrets WifiSsid)")
    ap.add_argument("--password", help="Target WiFi password (default: from _secrets WifiPassword)")
    ap.add_argument("--ap", help="OTGW config-portal AP SSID (default: auto-scan for OTGW-*)")
    ap.add_argument("--home", help="WiFi to rejoin afterwards (default: current SSID, else --ssid)")
    ap.add_argument("--dry-run", action="store_true", help="Show what would happen; do not touch WiFi")
    args = ap.parse_args()

    if os.name != "nt":
        log("ERROR: this tool is Windows-only (uses netsh). On other OSes use the captive portal manually.")
        return 2

    ssid = args.ssid
    password = args.password
    if not ssid or not password:
        s_ssid, s_pw = load_secrets()
        ssid = ssid or s_ssid
        password = password or s_pw
    if not ssid or not password:
        log("ERROR: no WiFi creds. Set WifiSsid/WifiPassword in %LOCALAPPDATA%/OTGW-capture/capture-settings.json "
            "or pass --ssid/--password. (Secrets must stay out of the repo.)")
        return 2

    home = args.home or current_ssid() or ssid
    log(f"target SSID: {ssid}  (password: {'*' * len(password)})")
    log(f"will rejoin afterwards: {home}")

    ap_ssid = args.ap or scan_otgw_ap()
    if not ap_ssid:
        log("ERROR: no 'OTGW-*' config-portal AP found in range. Is the device powered + in AP mode? "
            "Pass --ap <SSID> explicitly if you know it.")
        return 1
    log(f"OTGW config-portal AP: {ap_ssid}")

    if args.dry_run:
        log("DRY RUN: would join the AP, POST s/p to http://%s/wifisave, then rejoin %s. No changes made." % (PORTAL_IP, home))
        return 0

    log(f"joining open AP {ap_ssid} ...")
    if not connect_ssid(ap_ssid, profile_xml=open_profile_xml(ap_ssid)):
        log(f"ERROR: could not join {ap_ssid}. Manually rejoin your WiFi with: netsh wlan connect name=\"{home}\"")
        return 1

    log(f"joined. posting credentials to http://{PORTAL_IP}/wifisave ...")
    status, body = post_wifisave(ssid, password)
    log(f"/wifisave -> {status}" + (f" ({body[:80]})" if body else ""))

    log("waiting ~8s for the device to save + reboot into STA ...")
    time.sleep(8)

    log(f"restoring this PC's WiFi -> {home} ...")
    if not connect_ssid(home, timeout=30):
        log(f"WARNING: could not auto-rejoin {home}. Run manually: netsh wlan connect name=\"{home}\"")
        return 1

    log("done. Give the OTGW ~20s to join, then find it (otgw.local or your DHCP table) and verify /api/v2/health.")
    log("If it did not connect, the creds may be wrong, or re-run with the device freshly in AP mode.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
