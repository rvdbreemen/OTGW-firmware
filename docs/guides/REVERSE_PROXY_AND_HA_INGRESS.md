# Opening the OTGW web UI from Home Assistant or through a reverse proxy

Applies to OTGW-firmware 1.7.8-beta.6 and later (1.x line, ESP8266, classic web UI).

There are three ways to reach the gateway's own web UI from somewhere other than
`http://<gateway-ip>/` in a browser on your LAN:

| Way | What you get | HA over HTTPS / Nabu Casa | Setup |
|---|---|---|---|
| 1. "Visit device" link in HA | a link on the HA device page that opens the UI in a new tab | yes, as long as the browser is on your LAN | none, comes with MQTT discovery |
| 2. HA sidebar panel (ingress) | the UI inside HA, behind the HA login, also remote | yes | install one custom integration, a few lines of YAML |
| 3. Your own reverse proxy (nginx, ...) | the UI on a path of your own web server, e.g. `https://example.lan/otgw/` | yes | one proxy location block |

What does NOT work through a proxy: the **live OpenTherm log**. It streams over a
WebSocket on port 81, and a proxy that publishes the UI on one URL does not carry
that port. The UI shows this instead of failing silently:

> Opened through a reverse proxy or HTTPS: the live OpenTherm log needs direct
> access to the gateway (http://&lt;gateway address&gt;/). Commands still work.

Everything else (status, settings, commands, FSexplorer, file upload, firmware
update) works through the proxy. Open `http://<gateway-ip>/` directly when you
need the live log.

The gateway itself stays plain HTTP on your LAN. It does not do HTTPS and it does
not need to: the proxy handles HTTPS towards the browser.

## 1. The "Visit device" link

With MQTT discovery on, the gateway sends `configuration_url` (`cu`) in the
device block, set to `http://<gateway-ip>/`. Home Assistant shows it as **Visit**
on the device page (Settings > Devices & services > MQTT > OpenTherm Gateway).

- The link uses the IP address, not `<hostname>.local`: mDNS names do not resolve
  on every phone or PC.
- Give the gateway a fixed IP (a DHCP reservation in your router). The link is
  stored in the retained discovery configs and changes only when discovery is
  published again (the daily discovery refresh when enabled, or
  `POST /api/v2/otgw/discovery`); an HA restart reuses the retained configs. After
  an IP change the link can therefore point to the old address until then.
- The link opens a new tab directly on the gateway, so it only works from a
  device on the same LAN (or over your VPN).

## 2. A sidebar panel in Home Assistant (ingress)

Home Assistant's built-in ingress is only for add-ons. For a device on your LAN
the open-source custom integration **hass_ingress** by lovelylain
(https://github.com/lovelylain/hass_ingress, Apache-2.0) adds the same
mechanism: HA proxies the gateway's UI under `/api/ingress/<name>/`, only for
logged-in HA users.

Tested combination: hass_ingress 1.4.0 on Home Assistant 2026.10.4, HA over plain
HTTP and over HTTPS (behind a TLS reverse proxy).

### Install

Install "Ingress" (`lovelylain/hass_ingress`, listed in the HACS default
catalog; from HA 2026.11 the Marketplace), or copy `custom_components/ingress`
from the release zip into your HA config folder. Restart Home Assistant.

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
README); only the YAML form above was tested.

The panel appears in the sidebar as **OTGW**. That is all; the gateway needs no
setting for it.

### Rules for this panel

- **Keep `require_admin: true`.** The gateway UI can change boiler settings,
  flash firmware and reboot the device. Only HA administrators should see it.
- **Do not set a `host` header** (`headers: host: ...`). hass_ingress passes the
  browser's Host header to the gateway, and the gateway's CSRF check (when an
  admin password is set) compares that Host with the browser's Origin. Override
  Host and every save, upload and command returns
  `403 CSRF protection: invalid origin`.
- **Do not add `rewrite` rules.** Firmware 1.7.8-beta.6 and later builds every
  URL relative to the page it was loaded from, so it works under
  `/api/ingress/otgw/` as is. Rewrite rules written for older firmware now break
  the UI (the path gets the prefix twice).
- **Never use `static_token`.** It creates a link that opens the panel without an
  HA login, i.e. anyone with the link controls your heating.
- **Do not inject the gateway password** with `headers: authorization: Basic ...`.
  That would make every HA user who can open the panel a gateway admin without
  knowing the password. If you set a gateway admin password, let the browser ask
  for it, or leave it unset on a trusted LAN. (The bench test supplied the
  credentials from the browser on the 401 challenge; the prompt itself was not
  checked by hand.)

### What was measured

Through hass_ingress 1.4.0 the gateway receives (HA at `https://ha.example:8443`):

| Header | Value |
|---|---|
| Host | `ha.example:8443` (the browser's) |
| Origin | `https://ha.example:8443` |
| X-Forwarded-Host / -Proto | `ha.example:8443` / `https` |
| X-Ingress-Path | `/api/ingress/otgw` |
| HA cookies / tokens | not forwarded (hass_ingress strips its own token) |

So Origin and Host match and the CSRF check passes without any change on the
gateway. With `headers: host: <gateway-ip>` Host no longer matches and admin
requests are refused, which is why that setting must not be used.

## 3. Your own reverse proxy

Any proxy works if it serves the gateway on its own path and **passes the
browser's Host header**. nginx example, gateway at `192.168.1.50`, published as
`https://example.lan/otgw/`:

```nginx
location /otgw/ {
    proxy_pass http://192.168.1.50/;          # trailing slash: strips /otgw/
    proxy_set_header Host $http_host;         # required when a gateway password is set
    proxy_set_header X-Forwarded-Host $http_host;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_request_buffering off;               # stream firmware uploads
    client_max_body_size 4m;                   # firmware ~0.8 MB, LittleFS image ~2 MB
}
```

- Without `proxy_set_header Host $http_host` nginx sends the gateway's own address
  as Host. That is fine without a gateway password, but with one set every save,
  upload and command is refused with 403 (CSRF check), as measured.
- The trailing `/` on both the location and `proxy_pass` matters: the gateway
  expects its pages at `/`, the proxy maps `/otgw/` onto that.
- Only nginx was tested. Caddy and Traefik pass the Host header by default and
  should behave the same, but that was not verified on a gateway.

## Firmware updates and PIC flashing through a proxy

- **Firmware update** through the HA ingress panel (HTTPS) works: upload, flash
  and reboot in about 45 s; the page returns to the UI inside the panel.
- **LittleFS (filesystem) update** works through the HA ingress panel (about
  48 s) and through nginx (about 38 s). Your settings are kept, as with direct
  access. The image is about 2 MB, so the proxy must accept a body of that size
  (nginx: `client_max_body_size 4m`).
- **PIC firmware flashing**: the PIC page, the file list and the flash progress
  work through the proxy, and the flash request goes to the gateway on the proxy
  path. The flash itself runs on the gateway (ESP to PIC over serial), not
  through the proxy. A real PIC flash through a proxy has not been done on the
  bench, only a dry run in which the browser held back the final flash request.
  Until it has, flash the PIC over direct access: an interrupted PIC flash can
  brick the PIC.
- If an update through a proxy stops halfway, open the gateway directly and run it
  again from `http://<gateway-ip>/update`.

## Troubleshooting

| Symptom through the proxy | Cause | Fix |
|---|---|---|
| Header shows `[version]`, red "Error: HTTP 404" | firmware older than 1.7.8-beta.6 asks `/api/...` at the proxy root | update the gateway firmware **and** LittleFS (the UI files live there) |
| Saves, uploads, commands give 403 "CSRF protection: invalid origin" | proxy replaces the Host header, gateway admin password is set | pass the browser's Host (`proxy_set_header Host $http_host`; no `host` header in hass_ingress) |
| File upload gives 502 Bad Gateway | firmware older than 1.7.8-beta.6 ended the upload response without a complete header | update the firmware |
| OpenTherm Monitor shows only the command line and the notice above | expected: the live log needs direct access | open `http://<gateway-ip>/` |
| Sidebar panel loads HA's own page or a login loop | panel URL or ingress name wrong | check `url:` and that the panel name matches the sidebar entry |

## How this was verified

The bench rig in `scripts/ha-ingress-rig/` (Docker: Home Assistant 2026.10.4 with
hass_ingress 1.4.0, a Caddy TLS front, nginx, a header echo backend) runs the
classic UI against a real gateway in headless Chromium:

```bash
python scripts/ha-ingress-rig/setup_rig.py --device <gateway-ip> --broker <broker-ip>:1883
python scripts/ha-ingress-rig/validate_ingress.py --label check [--password <gateway admin pw>]
python scripts/ha-ingress-rig/ota_via_ingress.py --firmware build/<name>.ino.bin
python scripts/ha-ingress-rig/ota_via_ingress.py --filesystem build/<name>.littlefs.bin
python scripts/ha-ingress-rig/pic_flash_dryrun.py --label check   # never writes the PIC
python scripts/ha-ingress-rig/ha_device_page.py --label check
python scripts/ha-ingress-rig/setup_rig.py --down
```

`validate_ingress.py` fails when any UI request leaves the proxy path, an API call
is not 2xx, a settings write or upload fails, an FSexplorer form points outside
the proxy path, or the live-log notice is missing behind a proxy (direct access
must show the live log connected). Results and screenshots go to
`scripts/ha-ingress-rig/out/<label>/`. Bench results for this release: TASK-1378
in the backlog.

Related: [ADR-003](../adr/ADR-003-http-only-no-https.md) (HTTP only),
[ADR-054](../adr/ADR-054-optional-http-basic-auth.md) (admin password and CSRF
check), [WEBSOCKET_LOGGING.md](WEBSOCKET_LOGGING.md).
