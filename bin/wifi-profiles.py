#!/usr/bin/env python3
"""Named WiFi/location profiles for the OTGW capture + test tooling.

Problem this solves: capture-settings.json holds exactly ONE set of
WifiSsid/WifiPassword/BrokerHost/DeviceHost. Every time you change location you
overwrite the previous location's creds and lose them. This store keeps a NAMED
profile per location and projects the active one into capture-settings.json, so
all existing tooling (_secrets.py, provision-wifi-ap.py, the test harness) keeps
reading capture-settings.json unchanged.

Store (out-of-repo, never synced to git, same dir as capture-settings):
  %LOCALAPPDATA%/OTGW-capture/wifi-profiles.json
  {
    "active": "zutphen",
    "profiles": {
      "home":    {"ssid": "...", "password": "...", "broker": "192.168.1.234",
                  "device": "192.168.1.143"},
      "zutphen": {"ssid": "...", "password": "...", "broker": "192.168.88.32",
                  "device": ""}
    }
  }

Passwords are stored out-of-repo and NEVER printed (list/show mask them).

Usage:
  python bin/wifi-profiles.py list
  python bin/wifi-profiles.py add <name> --ssid <SSID> --password '...' \
                                         --broker <broker-ip> --mqtt-user <user> \
                                         --mqtt-password '...'
  python bin/wifi-profiles.py use zutphen        # -> writes into capture-settings.json
  python bin/wifi-profiles.py show zutphen
  python bin/wifi-profiles.py import-current home # snapshot capture-settings -> profile
"""
import argparse
import json
import os
import sys

STORE_NAME = "wifi-profiles.json"
SETTINGS_NAME = "capture-settings.json"


def _base_dir():
    base = os.environ.get("OTGW_CAPTURE_SETTINGS")
    if base:
        return os.path.dirname(base)
    return os.path.join(os.environ.get("LOCALAPPDATA", ""), "OTGW-capture")


def store_path():
    return os.path.join(_base_dir(), STORE_NAME)


def settings_path():
    base = os.environ.get("OTGW_CAPTURE_SETTINGS")
    return base or os.path.join(_base_dir(), SETTINGS_NAME)


def load_store():
    p = store_path()
    if os.path.isfile(p):
        with open(p, encoding="utf-8") as f:
            return json.load(f)
    return {"active": None, "profiles": {}}


def save_store(store):
    p = store_path()
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        json.dump(store, f, indent=2)


def _mask(pw):
    return "(set)" if pw else "(empty)"


def _set_settings_key(d, aliases, val):
    """Update an existing key case-insensitively, else add the first alias."""
    for k in list(d):
        if k.lower() in [a.lower() for a in aliases]:
            d[k] = val
            return
    d[aliases[0]] = val


def cmd_list(store, _args):
    active = store.get("active")
    profs = store.get("profiles", {})
    if not profs:
        print("(no profiles yet — add one with: wifi-profiles.py add <name> --ssid ...)")
        return
    for name, p in profs.items():
        mark = "*" if name == active else " "
        print(f" {mark} {name:12s} ssid={p.get('ssid','')!r:20s} "
              f"pw={_mask(p.get('password'))} broker={p.get('broker','')} "
              f"device={p.get('device','')} mqtt_user={p.get('mqtt_user','')!r} "
              f"mqtt_pw={_mask(p.get('mqtt_password'))}")
    print(f"\nactive: {active or '(none)'}")


def cmd_show(store, args):
    p = store.get("profiles", {}).get(args.name)
    if not p:
        sys.exit(f"no such profile: {args.name}")
    print(f"{args.name}: ssid={p.get('ssid','')!r} pw={_mask(p.get('password'))} "
          f"broker={p.get('broker','')} device={p.get('device','')} "
          f"mqtt_user={p.get('mqtt_user','')!r} mqtt_pw={_mask(p.get('mqtt_password'))}")


def cmd_add(store, args):
    profs = store.setdefault("profiles", {})
    existing = profs.get(args.name, {})
    prof = {
        "ssid": args.ssid if args.ssid is not None else existing.get("ssid", ""),
        "password": args.password if args.password is not None else existing.get("password", ""),
        "broker": args.broker if args.broker is not None else existing.get("broker", ""),
        "device": args.device if args.device is not None else existing.get("device", ""),
        "mqtt_user": args.mqtt_user if args.mqtt_user is not None else existing.get("mqtt_user", ""),
        "mqtt_password": args.mqtt_password if args.mqtt_password is not None else existing.get("mqtt_password", ""),
    }
    profs[args.name] = prof
    if store.get("active") is None:
        store["active"] = args.name
    save_store(store)
    print(f"saved profile '{args.name}' (ssid={prof['ssid']!r}, pw={_mask(prof['password'])}, "
          f"mqtt_user={prof['mqtt_user']!r}, mqtt_pw={_mask(prof['mqtt_password'])})")


def cmd_import_current(store, args):
    """Snapshot the current capture-settings.json into a named profile."""
    sp = settings_path()
    if not os.path.isfile(sp):
        sys.exit(f"no capture-settings to import: {sp}")
    with open(sp, encoding="utf-8") as f:
        s = {k.lower(): v for k, v in json.load(f).items()}
    prof = {
        "ssid": s.get("wifissid", s.get("wifi_ssid", "")),
        "password": s.get("wifipassword", s.get("wifi_password", "")),
        "broker": s.get("brokerhost", s.get("broker_host", "")),
        "device": s.get("devicehost", s.get("device_host", "")),
        "mqtt_user": s.get("username", s.get("mqtt_user", "")),
        "mqtt_password": s.get("mqttpassword", s.get("mqtt_password", "")),
    }
    store.setdefault("profiles", {})[args.name] = prof
    save_store(store)
    print(f"imported capture-settings into profile '{args.name}' "
          f"(ssid={prof['ssid']!r}, pw={_mask(prof['password'])})")


def cmd_use(store, args):
    prof = store.get("profiles", {}).get(args.name)
    if not prof:
        sys.exit(f"no such profile: {args.name}")
    sp = settings_path()
    d = {}
    if os.path.isfile(sp):
        with open(sp, encoding="utf-8") as f:
            d = json.load(f)
    _set_settings_key(d, ["WifiSsid", "wifi_ssid"], prof.get("ssid", ""))
    _set_settings_key(d, ["WifiPassword", "wifi_password"], prof.get("password", ""))
    if prof.get("broker"):
        _set_settings_key(d, ["BrokerHost", "broker_host"], prof["broker"])
    if prof.get("device"):
        _set_settings_key(d, ["DeviceHost", "device_host"], prof["device"])
    if prof.get("mqtt_user"):
        _set_settings_key(d, ["Username", "mqtt_user"], prof["mqtt_user"])
    if prof.get("mqtt_password"):
        _set_settings_key(d, ["MqttPassword", "mqtt_password"], prof["mqtt_password"])
    os.makedirs(os.path.dirname(sp), exist_ok=True)
    with open(sp, "w", encoding="utf-8") as f:
        json.dump(d, f, indent=2)
    store["active"] = args.name
    save_store(store)
    print(f"active profile -> '{args.name}'; projected ssid/password"
          f"{'/broker' if prof.get('broker') else ''}"
          f"{'/device' if prof.get('device') else ''}"
          f"{'/mqtt-user' if prof.get('mqtt_user') else ''}"
          f"{'/mqtt-pw' if prof.get('mqtt_password') else ''} into {os.path.basename(sp)}")


def main():
    ap = argparse.ArgumentParser(description="Named WiFi/location profiles (out-of-repo).")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list").set_defaults(func=cmd_list)
    s = sub.add_parser("show"); s.add_argument("name"); s.set_defaults(func=cmd_show)
    a = sub.add_parser("add")
    a.add_argument("name")
    a.add_argument("--ssid")
    a.add_argument("--password")
    a.add_argument("--broker")
    a.add_argument("--device")
    a.add_argument("--mqtt-user", dest="mqtt_user")
    a.add_argument("--mqtt-password", dest="mqtt_password")
    a.set_defaults(func=cmd_add)
    u = sub.add_parser("use"); u.add_argument("name"); u.set_defaults(func=cmd_use)
    ic = sub.add_parser("import-current"); ic.add_argument("name"); ic.set_defaults(func=cmd_import_current)
    args = ap.parse_args()
    store = load_store()
    args.func(store, args)


if __name__ == "__main__":
    main()
