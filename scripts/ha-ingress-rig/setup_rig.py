#!/usr/bin/env python3
"""Set up the Home Assistant ingress + reverse proxy bench rig (TASK-1378).

Creates ./.runtime (certs, HA config with lovelylain/hass_ingress, nginx config),
starts the containers, onboards Home Assistant with a rig-only admin account and
adds the MQTT integration pointed at --broker (the broker the OTGW publishes to). Idempotent: a second run
keeps the existing HA config and account.

    python scripts/ha-ingress-rig/setup_rig.py --device 192.168.88.68
    python scripts/ha-ingress-rig/setup_rig.py --down        # stop the rig

The account, its password and the device address go to .runtime/rig.json, which
validate_ingress.py reads. Nothing in .runtime is committed.
"""
import argparse
import io
import json
import secrets
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

RIG = Path(__file__).resolve().parent
RUNTIME = RIG / ".runtime"
HA_URL = "http://localhost:18123"
HASS_INGRESS_ZIP = "https://github.com/lovelylain/hass_ingress/releases/download/1.4.0/ingress.zip"

CONFIG_YAML = """\
default_config:

# Panels for the OTGW (and a header echo backend standing in for it).
# "otgw" is the recommended setup: Host header left as the browser sent it.
# "otgw_hostdev" overrides Host with the device address (negative row).
# "otgw_rw" uses only rewrite rules, for firmware that is not base-path aware.
ingress:
  otgw:
    title: OTGW
    icon: mdi:water-boiler
    url: http://{device}
    require_admin: true
  otgw_hostdev:
    title: OTGW host=device
    icon: mdi:water-boiler-alert
    url: http://{device}
    require_admin: true
    headers:
      host: "{device}"
  otgw_rw:
    title: OTGW rewrite
    icon: mdi:water-boiler-off
    url: http://{device}
    require_admin: true
    rewrite:
      - mode: body
        path: /index.js
        match: "window\\\\.location\\\\.host \\\\+ '/api/'"
        replace: "window.location.host + '/api/ingress/otgw_rw/api/'"
      - mode: body
        path: /index.js
        match: "localURL \\\\+ '/"
        replace: "localURL + '/api/ingress/otgw_rw/"
  echo:
    title: Echo
    icon: mdi:radar
    url: http://echo:8080
    require_admin: true
  echo_hostdev:
    title: Echo host=device
    icon: mdi:radar
    url: http://echo:8080
    require_admin: true
    headers:
      host: "{device}"
"""

NGINX_CONF = """\
# Generic reverse proxy on a sub-path, two flavours:
#   /otgw/          nginx defaults (Host = upstream address)
#   /otgw-keephost/ Host and X-Forwarded-* passed through (recommended)
map $http_upgrade $connection_upgrade {{ default upgrade; '' close; }}

server {{
  listen 19080;
  listen 19443 ssl;
  ssl_certificate     /certs/rig.crt;
  ssl_certificate_key /certs/rig.key;
  client_max_body_size 2m;

  location /otgw/ {{
    proxy_pass http://{device}/;
  }}

  location /otgw-keephost/ {{
    proxy_pass http://{device}/;
    proxy_set_header Host $http_host;
    proxy_set_header X-Forwarded-Host $http_host;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_request_buffering off;
  }}
}}
"""


def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def make_certs():
    certs = RUNTIME / "certs"
    certs.mkdir(parents=True, exist_ok=True)
    if (certs / "rig.crt").exists():
        return
    # openssl inside a throwaway container: no host dependency on openssl.
    run(["docker", "run", "--rm", "-v", f"{certs}:/out", "alpine/openssl", "req", "-x509",
         "-newkey", "rsa:2048", "-nodes", "-days", "365", "-subj", "/CN=localhost",
         "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
         "-keyout", "/out/rig.key", "-out", "/out/rig.crt"])


def install_hass_ingress():
    target = RUNTIME / "ha" / "custom_components" / "ingress"
    if (target / "manifest.json").exists():
        return
    target.mkdir(parents=True, exist_ok=True)
    print("download", HASS_INGRESS_ZIP, flush=True)
    with urllib.request.urlopen(HASS_INGRESS_ZIP, timeout=60) as r:
        zipfile.ZipFile(io.BytesIO(r.read())).extractall(target)
    manifest = json.loads((target / "manifest.json").read_text())
    print("hass_ingress", manifest.get("version"), flush=True)


def write_configs(device):
    ha = RUNTIME / "ha"
    ha.mkdir(parents=True, exist_ok=True)
    (ha / "configuration.yaml").write_text(CONFIG_YAML.format(device=device))
    (RUNTIME / "nginx.conf").write_text(NGINX_CONF.format(device=device))


def http(method, url, data=None, token=None, form=False):
    headers = {}
    body = None
    if data is not None:
        if form:
            body = urllib.parse.urlencode(data).encode()
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        else:
            body = json.dumps(data).encode()
            headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(url, data=body, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=30) as r:
        txt = r.read().decode()
        return json.loads(txt) if txt else {}


def wait_ha():
    """Wait until HA answers its auth endpoint; return the onboarding steps."""
    for _ in range(180):
        try:
            with urllib.request.urlopen(HA_URL + "/auth/providers", timeout=5):
                pass
            try:
                with urllib.request.urlopen(HA_URL + "/api/onboarding", timeout=5) as r:
                    return json.loads(r.read().decode())
            except urllib.error.HTTPError:
                return []  # onboarding finished: the endpoint is gone
        except (urllib.error.URLError, ConnectionError, TimeoutError):
            time.sleep(2)
    sys.exit("Home Assistant did not come up on " + HA_URL)


def onboard(state):
    steps = wait_ha()
    done = {s["step"]: s["done"] for s in steps}
    client_id = HA_URL + "/"
    if not done.get("user", "username" in state):
        state["username"] = "rigadmin"
        state["password"] = secrets.token_urlsafe(18)
        res = http("POST", HA_URL + "/api/onboarding/users", {
            "client_id": client_id, "name": "Rig Admin", "username": state["username"],
            "password": state["password"], "language": "en"})
        tok = http("POST", HA_URL + "/auth/token", {
            "grant_type": "authorization_code", "code": res["auth_code"], "client_id": client_id}, form=True)
        access = tok["access_token"]
        http("POST", HA_URL + "/api/onboarding/core_config", {}, token=access)
        http("POST", HA_URL + "/api/onboarding/analytics", {}, token=access)
        http("POST", HA_URL + "/api/onboarding/integration", {
            "client_id": client_id, "redirect_uri": client_id}, token=access)
        save(state)
    return login(state, HA_URL)


def login(state, base):
    """Password login through the auth flow; returns an access token for `base`."""
    client_id = base + "/"
    flow = http("POST", base + "/auth/login_flow", {
        "client_id": client_id, "handler": ["homeassistant", None], "redirect_uri": client_id})
    res = http("POST", base + "/auth/login_flow/" + flow["flow_id"], {
        "username": state["username"], "password": state["password"], "client_id": client_id})
    tok = http("POST", base + "/auth/token", {
        "grant_type": "authorization_code", "code": res["result"], "client_id": client_id}, form=True)
    return tok["access_token"]


async def _ws(token, *msgs):
    import websockets  # noqa: PLC0415  (only needed for the HTTP config step)
    async with websockets.connect(HA_URL.replace("http", "ws") + "/api/websocket") as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": token}))
        await ws.recv()
        out = []
        for i, m in enumerate(msgs, 1):
            await ws.send(json.dumps(dict(m, id=i)))
            out.append(json.loads(await ws.recv()))
        return out


def ensure_trusted_proxy(state, token):
    """Trust the Caddy TLS front (X-Forwarded-For). HA 2026.10 keeps the HTTP
    config in storage, applies a change on restart and reverts it after 5 min
    unless it is promoted, so: configure, wait for the restart, promote."""
    import asyncio  # noqa: PLC0415
    cur = asyncio.run(_ws(token, {"type": "http/config"}))[0]
    print("http/config:", json.dumps(cur.get("result"))[:400], flush=True)
    res = cur.get("result") or {}
    stable = res.get("stable") or {}
    if stable.get("use_x_forwarded_for"):
        return token
    pending = res.get("pending") or {}
    if not (pending.get("use_x_forwarded_for") and not pending.get("error")):
        meta = ("created_at", "error", "error_message")
        new = {k: v for k, v in stable.items() if k not in meta}
        new.update(use_x_forwarded_for=True, trusted_proxies=["172.16.0.0/12"])
        for _ in range(60):  # configure is refused until HA reports RUNNING
            r = asyncio.run(_ws(token, {"type": "http/config/configure", "config": new}))[0]
            if r.get("success") or (r.get("error") or {}).get("code") != "not_running":
                break
            time.sleep(3)
        print("configure:", r, flush=True)
        time.sleep(15)
        wait_ha()
        time.sleep(10)
        token = login(state, HA_URL)
    print("promote:", asyncio.run(_ws(token, {"type": "http/config/promote"})), flush=True)
    return token


def ensure_mqtt(token, broker):
    host, _, port = broker.partition(":")
    port = int(port or 1883)
    entries = http("GET", HA_URL + "/api/config/config_entries/entry", token=token)
    for e in entries:
        if e["domain"] != "mqtt":
            continue
        if e.get("title") == host:
            return
        http("DELETE", HA_URL + "/api/config/config_entries/entry/" + e["entry_id"], token=token)
    flow = http("POST", HA_URL + "/api/config/config_entries/flow",
                {"handler": "mqtt", "show_advanced_options": False}, token=token)
    res = http("POST", HA_URL + "/api/config/config_entries/flow/" + flow["flow_id"],
               {"broker": host, "port": port, "protocol": "5",
                "other_settings": {"set_client_cert": False, "set_ca_cert": "off", "transport": "tcp"}},
               token=token)
    print("mqtt flow:", res.get("type"), res.get("errors") or "", flush=True)


def save(state):
    (RUNTIME / "rig.json").write_text(json.dumps(state, indent=2))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", help="OTGW address on the LAN, e.g. 192.168.88.68")
    ap.add_argument("--broker", default="host.docker.internal:1883",
                    help="MQTT broker for HA, host[:port]; the OTGW must publish to the same broker")
    ap.add_argument("--down", action="store_true", help="stop and remove the rig containers")
    args = ap.parse_args()
    compose = ["docker", "compose", "-f", str(RIG / "docker-compose.yml"), "-p", "otgw-ingress-rig"]
    if args.down:
        run(compose + ["down"])
        return
    if not args.device:
        ap.error("--device is required")
    RUNTIME.mkdir(exist_ok=True)
    state = json.loads((RUNTIME / "rig.json").read_text()) if (RUNTIME / "rig.json").exists() else {}
    state["device"] = args.device
    make_certs()
    install_hass_ingress()
    write_configs(args.device)
    run(compose + ["up", "-d"])
    run(compose + ["restart", "homeassistant", "nginx"])  # pick up rewritten configs
    token = onboard(state)
    token = ensure_trusted_proxy(state, token)
    ensure_mqtt(token, args.broker)
    save(state)
    print(json.dumps({k: v for k, v in state.items() if k != "password"}))
    print("Rig up: HA http %s, https https://localhost:18443, proxy https://localhost:19443/otgw-keephost/"
          % HA_URL)


if __name__ == "__main__":
    main()
