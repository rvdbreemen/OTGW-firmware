"""Shared secret/identity loader for the capture + test scripts.

Single out-of-repo store: %LOCALAPPDATA%/OTGW-capture/capture-settings.json (the
same file capture-mqtt-debug.bat already writes). It lives OUTSIDE the git repo so
it is never synced to GitHub. Keys are read case-insensitively (the PowerShell tool
writes PascalCase: DeviceHost/BrokerHost/MqttPassword; this helper also accepts
snake_case). Every value can be overridden by an environment variable so CI / cron
runs need nothing on disk.

Usage:
    from _secrets import get, mqtt_password
    pw = mqtt_password()              # "" if not stored
    broker = get("broker_host", "homeassistant.local")
"""
import json
import os

# logical key -> (settings.json aliases, env var)
_KEYS = {
    "device_host":  (["DeviceHost", "device_host"],        "OTGW_DEVICE_HOST"),
    "broker_host":  (["BrokerHost", "broker_host", "mqtt_broker"], "OTGW_MQTT_BROKER"),
    "broker_port":  (["BrokerPort", "broker_port", "mqtt_port"],   "OTGW_MQTT_PORT"),
    "topic":        (["Topic", "topic"],                   "OTGW_MQTT_TOPIC"),
    "mqtt_user":    (["Username", "mqtt_user", "username"], "OTGW_MQTT_USER"),
    "mqtt_password":(["MqttPassword", "mqtt_password", "password"], "OTGW_MQTT_PASSWORD"),
    "wifi_ssid":    (["WifiSsid", "wifi_ssid"],            "OTGW_WIFI_SSID"),
    "wifi_password":(["WifiPassword", "wifi_password"],    "OTGW_WIFI_PASSWORD"),
    "uniqueid":     (["MqttUniqueId", "mqtt_uniqueid", "uniqueid"], "OTGW_UNIQUEID"),
    "hardware":     (["Hardware", "hardware"],             "OTGW_HARDWARE"),
    "com_port":     (["ComPort", "com_port"],              "OTGW_COM_PORT"),
}


def settings_path():
    base = os.environ.get("OTGW_CAPTURE_SETTINGS")
    if base:
        return base
    return os.path.join(os.environ.get("LOCALAPPDATA", ""), "OTGW-capture", "capture-settings.json")


def _load_file():
    try:
        p = settings_path()
        if os.path.isfile(p):
            with open(p, encoding="utf-8") as fh:
                raw = json.load(fh)
            # case-insensitive view
            return {str(k).lower(): v for k, v in raw.items()}
    except Exception:
        pass
    return {}


def get(logical_key, default=None):
    aliases, env = _KEYS.get(logical_key, ([logical_key], None))
    if env and os.environ.get(env):
        return os.environ[env]
    data = _load_file()
    for a in aliases:
        v = data.get(a.lower())
        if v not in (None, ""):
            return v
    return default


def mqtt_password():
    return get("mqtt_password", "") or ""


def all_secrets():
    return {k: get(k) for k in _KEYS}


if __name__ == "__main__":
    # Masked dump for sanity-checking (never prints secret values).
    for k, v in all_secrets().items():
        if v is None:
            shown = "(unset)"
        elif "password" in k:
            shown = "***set***"
        else:
            shown = str(v)
        print(f"{k:14} = {shown}")
    print(f"\nstore: {settings_path()}")
