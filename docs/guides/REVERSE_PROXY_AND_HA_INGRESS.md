# Opening the OTGW web UI from Home Assistant or through a reverse proxy

Applies to OTGW-firmware 2.0.0-alpha.507 and later (2.0.0 line, v2 web UI).
The 1.x line has its own version of this guide on the `otgw-1.x.x` branch.

There are three ways to reach the gateway's own web UI from somewhere other than
`http://<gateway-ip>/` in a browser on your LAN:

| Way | What you get | HA over HTTPS / Nabu Casa | Setup |
|---|---|---|---|
| 1. "Visit" link in HA | a link on the HA device page that opens the UI in a new tab | yes, as long as the browser is on your LAN | none, comes with MQTT discovery |
| 2. HA sidebar panel (ingress) | the UI inside HA, behind the HA login, also remote | yes | install one custom integration, a few lines of YAML |
| 3. Your own reverse proxy (nginx, ...) | the UI on a path of your own web server, e.g. `https://example.lan/otgw/` | yes | one proxy location block |

Through a proxy everything works, the live OpenTherm log included: on 2.0.0 the
log is a WebSocket on the same port as the pages (`/ws`), and the UI opens it on
the proxy's path (`wss://` when the page is HTTPS). The proxy has to carry
WebSocket; Home Assistant ingress does, nginx needs two extra lines (below).

The gateway itself stays plain HTTP on your LAN. It does not do HTTPS and it does
not need to: the proxy handles HTTPS towards the browser.

## 1. The "Visit" link

With MQTT discovery on, the gateway sends `configuration_url` (`cu`) in the
device block, set to `http://<gateway-ip>/` (the Ethernet address when the board
is wired, else WiFi). Home Assistant shows it as **Visit** on the device page
(Settings > Devices & services > MQTT > OpenTherm Gateway).

- The link uses the IP address, not `<hostname>.local`: mDNS names do not resolve
  on every phone or PC.
- Give the gateway a fixed IP (a DHCP reservation in your router). The link sits
  in the retained discovery configs and changes only when discovery is published
  again, so after an IP change it can point to the old address for a while.
- The link opens a new tab directly on the gateway, so it only works from a
  device on the same LAN (or over your VPN).

## 2. A sidebar panel in Home Assistant (ingress)

Home Assistant's built-in ingress is only for add-ons. For a device on your LAN
the open-source custom integration **hass_ingress** by lovelylain
(https://github.com/lovelylain/hass_ingress, Apache-2.0, in the HACS default
catalog) adds the same mechanism: HA proxies the gateway's UI under
`/api/ingress/<name>/`, only for logged-in HA users. Alternatives that were
considered: HA's own Webpage dashboard is only an iframe (an `http://` gateway
inside an `https://` HA page is blocked by the browser), and proxy add-ons need
HA OS.

Tested combination: hass_ingress 1.4.0 on Home Assistant 2026.10.4, HA over plain
HTTP and over HTTPS (behind a TLS reverse proxy).

### Install

Install "Ingress" (`lovelylain/hass_ingress`) from HACS (from HA 2026.11 the
Marketplace), or copy `custom_components/ingress` from the release zip into your
HA config folder. Restart Home Assistant.

### Configure

Add this to `configuration.yaml` and restart (replace the IP address):

```yaml
ingress:
  otgw:
    title: OTGW
    icon: mdi:water-boiler
    url: http://192.168.1.50
    require_admin: true
```

On HA 2025.8 and later hass_ingress can also be configured in the UI (see its
README); only the YAML form above was tested. The panel appears in the sidebar as
**OTGW**; the gateway needs no setting for it.

### Rules for this panel

- **Keep `require_admin: true`.** The gateway UI can change boiler settings,
  flash firmware and reboot the device. With HA reachable from outside (Nabu Casa,
  your own domain), the panel makes the gateway reachable there too, for every
  user who can open it.
- **Use the gateway's IP address in `url:`**, not `<hostname>.local`. The firmware
  updater (safeboot) has no network name, so a firmware update through the panel
  only works when HA reaches the gateway by IP.
- **Do not set a `host` header** (`headers: host: ...`). hass_ingress passes the
  browser's Host header to the gateway, and the gateway's CSRF check (when an
  admin password is set) compares that Host with the browser's Origin. Override
  Host and every save, upload and command returns
  `403 CSRF protection: invalid origin`.
- **Do not add `rewrite` rules.** The UI builds every URL relative to the page it
  was loaded from, so it works under `/api/ingress/otgw/` as is.
- **Never use `static_token`.** It creates a link that opens the panel without an
  HA login, i.e. anyone with the link controls your heating.
- **Do not inject the gateway password** with `headers: authorization: Basic ...`.
  That would make every HA user who can open the panel a gateway admin without
  knowing the password. If you set a gateway admin password, let the browser ask
  for it, or leave it unset on a trusted LAN.

### What the gateway receives

Through hass_ingress 1.4.0 (HA at `https://ha.example:8443`): Host
`ha.example:8443` (the browser's), Origin `https://ha.example:8443`,
X-Forwarded-Host/Proto `ha.example:8443`/`https`, X-Ingress-Path
`/api/ingress/otgw`. HA's own login token is not forwarded. Origin and Host match,
so the CSRF check passes without any change on the gateway.

## 3. Your own reverse proxy

Any proxy works if it serves the gateway on its own path, **passes the browser's
Host header** and carries WebSocket. nginx example, gateway at `192.168.1.50`,
published as `https://example.lan/otgw/`:

```nginx
# once, in the http block
map $http_upgrade $connection_upgrade { default upgrade; '' close; }

location /otgw/ {
    proxy_pass http://192.168.1.50/;          # trailing slash: strips /otgw/
    proxy_set_header Host $http_host;         # required when a gateway password is set
    proxy_set_header X-Forwarded-Host $http_host;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_http_version 1.1;                   # live log (WebSocket at /otgw/ws)
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection $connection_upgrade;
    proxy_request_buffering off;              # stream firmware uploads
    client_max_body_size 4m;                  # firmware ~2.1 MB, web files ~0.8 MB
}
```

- Without `proxy_set_header Host $http_host` nginx sends the gateway's own address
  as Host. That is fine without a gateway password, but with one set every save,
  upload and command is refused with 403 (CSRF check), as measured.
- Without the three WebSocket lines the page works but the live log stays empty.
- Only nginx was tested. Caddy and Traefik pass Host and WebSocket by default and
  should behave the same, but that was not verified on a gateway.

## Firmware updates through a proxy

The update page (`<base>/update`, also linked from the UI) works through the proxy.
Its one-action update first restarts the gateway into the firmware updater
(safeboot), sends the firmware there, waits for the new firmware and then sends
the web files.

- **Web files (LittleFS) only:** works through the proxy, settings are kept.
- **Firmware through HA ingress:** tested three times on a bench gateway. Two runs
  completed (148 s and 161 s). In one run the proxy answered `502 Bad Gateway`
  to the firmware upload; the gateway then stayed in the updater with no valid
  firmware, and was recovered by sending the firmware again from
  `http://<gateway-ip>/` (the updater page answers there). The cause was not found.
  Through nginx (which streams the upload) the firmware upload succeeded.
- So: **update the firmware by IP on your LAN when you can.** If you update
  through the panel and it stops at "upload", the gateway is in the updater and
  keeps waiting: open `http://<gateway-ip>/` on the LAN and send the firmware
  again. ADR-191 describes the updater and its rollback.
- PIC firmware flashing through a proxy was not tested on 2.0.0 (the test bench
  has no PIC). The page uses the same proxy-aware URLs.

## Troubleshooting

| Symptom through the proxy | Cause | Fix |
|---|---|---|
| Page loads without data, requests to `/api/v2/...` give 404 | firmware older than 2.0.0-alpha.507 asks the API at the proxy root | update the gateway firmware **and** web files |
| Saves, uploads, commands give 403 "CSRF protection: invalid origin" | proxy replaces the Host header, gateway admin password is set | pass the browser's Host (`proxy_set_header Host $http_host`; no `host` header in hass_ingress) |
| Live log stays empty, the rest works | proxy does not carry WebSocket | nginx: add the `Upgrade`/`Connection` lines above |
| Update page says the updater can only be reached by IP | page opened by name directly, not through a proxy | open `http://<gateway-ip>/update` |
| Firmware update stopped at "upload" (502) | see "Firmware updates through a proxy" | send the firmware again by IP on the LAN |

## How this was verified

The bench rig in `scripts/ha-ingress-rig/` (Docker: Home Assistant 2026.10.4 with
hass_ingress 1.4.0, a Caddy TLS front, nginx, a header echo backend) runs the v2
UI against a real gateway in headless Chromium:

```bash
python scripts/ha-ingress-rig/setup_rig.py --device <gateway-ip> --broker <broker-ip>:1883
python scripts/ha-ingress-rig/validate_ingress.py --label check [--password <gateway admin pw>]
python scripts/ha-ingress-rig/ota_via_ingress.py --one-action <app.ino.bin> <fs.littlefs.bin>
python scripts/ha-ingress-rig/ha_device_page.py --label check
python scripts/ha-ingress-rig/setup_rig.py --down
```

`validate_ingress.py` fails when any UI request leaves the proxy path, an API call
is not 2xx (503 from the device's busy gate is counted, not failed), a settings
write or upload fails, or an FSexplorer form points outside the proxy path; it
records the live-log WebSocket per scenario and the device's REST counters
(`hd_rest_503`, `hd_rest_inflight_hwm`). Bench results: TASK-1377 in the backlog
and `docs/research/2026-10-25-ha-ingress-bench-2.0.0/`.

Related: [ADR-003](../adr/ADR-003-http-only-no-https.md) (HTTP only),
[ADR-056](../adr/ADR-056-protected-admin-endpoint-security-and-secret-handling-contract.md)
(admin password and CSRF check),
[ADR-165](../adr/ADR-165-optimal-request-parallelism-esp32s3-webui-rest.md)
(at most 2 API calls in flight, kept behind a proxy),
[ADR-191](../adr/ADR-191-update-firmware-over-wifi-on-4mb-boards-through-a-safeboot-factory-partition.md)
(safeboot updater), [WEBSOCKET_LOGGING.md](WEBSOCKET_LOGGING.md).
