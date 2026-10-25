# HA ingress and reverse proxy: 1.x bench results (TASK-1378)

Date: 2026-10-25. Device: bench ESP8266 OTGW 192.168.88.68 (PIC gateway 6.8, no
boiler; the OT frames in the screenshots are the PIC's own requests).
Old = 1.7.8-beta.6+12975b4 (firmware and LittleFS as found). Fix = 1.7.8-beta.6+54cbe02
(this change, firmware and LittleFS flashed over web OTA).
Rig: `scripts/ha-ingress-rig/` (Home Assistant 2026.10.4 + hass_ingress 1.4.0, Caddy
TLS front for HA, nginx 1.27 sub-path proxy, header echo backend; headless Chromium 151).
Background and options: [the research note](../2026-10-25-home-assistant-ui-integration.md).

## Behaviour matrix (no gateway password)

`validate_ingress.py`; "esc" = UI requests that left the proxy path, "api" = /api/v2
responses that were not 2xx. Raw: `results_old.json`, `results_fix.json`.

| Scenario | Old | Fix |
|---|---|---|
| direct `http://<ip>/` | works, live log Connected | works, live log Connected |
| HA ingress, HA over HTTP | header `[version]`, 7 esc, 404s | works, 0 esc, write 200, upload 303, notice shown |
| HA ingress, HA over HTTPS | header `[version]`, 7 esc, 404s, upload 502, 5 FSexplorer forms leave the path | works, same as above |
| HA ingress, `headers: host: <ip>` | as HTTPS row | works (no password set) |
| HA ingress, rewrite rules only | loads, write 200, upload 502, FSexplorer forms leave the path | breaks: prefix applied twice (rewrite rules must be dropped) |
| nginx sub-path, Host kept, HTTPS / HTTP | `[version]`, 6 esc, 404s, upload 502 | works |
| nginx sub-path, nginx default Host | as above | works (no password set) |

Old, HA over HTTPS: `old_ha_https_ingress.png`. Fix: `fix_ha_https_ingress.png`.
Old, nginx: `old_nginx_https_subpath.png`. Fix: `fix_nginx_https_subpath.png`.
Direct access with the live log on the fix: `fix_direct_livelog.png`.

Root causes found on the old build:
1. `index.js` built every device URL on the host root (`location.host + '/api/'`,
   `'/update'`, `"/FSexplorer"`), `FSexplorer.html` and the update page used `/upload`,
   `/update`, `/ReBoot`, `/`, `/api/v2/...`. Behind a proxy those hit the proxy, not the device.
2. The upload/delete response (`FSexplorer.ino` `Header[]`) ended without the blank
   line that closes the header block, and without Content-Length. Chromium tolerates
   that; curl, nginx and aiohttp (hass_ingress) do not. curl direct to the device:
   old `000` (no response), fix `303`. Through every proxy: old `502`, fix `303`.
3. The live log only showed a notice for PS=1 mode; behind a proxy it stayed
   "Disconnected" without explanation, and over plain-HTTP ingress it tried
   `ws://<ha-host>:81`.

## Headers the device receives through hass_ingress 1.4.0

From the echo backend (`results_fix.json`, scenarios `ha_https_echo*`):

| | default | `headers: host: 192.168.88.68` |
|---|---|---|
| Host | `localhost:18443` (browser's) | `192.168.88.68` |
| Origin | `https://localhost:18443` | `https://localhost:18443` |
| X-Forwarded-Host / Proto | `localhost:18443` / `https` | same |
| X-Ingress-Path | `/api/ingress/echo` | `/api/ingress/echo_hostdev` |

hass_ingress copies the browser's request headers except its own token cookie
(source: `custom_components/ingress/ingress.py` `_init_header`, release 1.4.0).

## Gateway admin password set (CSRF check active, ADR-054)

`results_fix_password.json`; the browser supplies the credentials on the 401.

| Scenario | settings write | upload |
|---|---|---|
| direct | 200 | 303 |
| HA ingress HTTP / HTTPS (default) | 200 | 303 |
| HA ingress, `headers: host: <ip>` | 403 | 403 |
| nginx, Host kept | 200 | 303 |
| nginx default (Host = upstream) | 403 | 403 |

curl straight to the device, password set: no Origin 200, same Origin 200,
`Origin: http://evil.example` 403, no credentials 401; upload with the foreign
Origin 403 and the file was not written, upload with the same Origin 303.
`isSameOriginRequest()` is unchanged by this change (no diff in `restAPI.ino`), so
the CSRF behaviour is the same as before; the old build was not re-run with a
password. Outcome: no firmware change to the CSRF check. The recommended proxy
setups keep the browser's Host, and the guide says so.

## Firmware OTA through HA ingress (HTTPS)

`ota_via_ingress.json`: the device's own update page inside the panel, same build
flashed: ok in 43.8 s, uptime 4 to 0 min, reboot log `[OTA] Rebooting...`, the page
returned to the UI inside the panel. LittleFS OTA and PIC flashing through a proxy
were not tested.

## Free heap during the proxied session

After each scenario (`device_heap_after` in `results_fix.json`): free heap
15512..16720 bytes, largest block 13712..16048 bytes. No reboot during the runs
(the only reboots were the two OTA flashes).

## configuration_url (cu)

Device block before (`discovery_device_block_old.txt`, 511-byte payload) and after
(`discovery_device_block_fix.txt`, 540 bytes): the only addition is
`"cu":"http://192.168.88.68/"`. `discovery_diff.txt`: 265 topics in both captures,
0 differ beyond `dev.sw_version`, `dev.cu` and `origin.sw`; 0 invalid JSON; longest
payload 1019 bytes. The 46 extra topics in the fix capture are table rows from the
full discovery run (`POST /api/v2/otgw/discovery`); the old capture was the retained
state on the broker. HA registry: `configuration_url` `http://192.168.88.68/`
(`ha_device_page.json`); the device page shows **Visit** (`fix_ha_device_visit.png`)
and the link opens the classic UI (`fix_ha_visit_opened.png`).

## Not verified

- LittleFS OTA and PIC flashing through a proxy.
- Caddy or Traefik in front of the gateway (only nginx).
- Nabu Casa remote access (same ingress path, not run).
- A real browser's basic-auth prompt inside the HA panel (the test browser answered
  the 401 itself).
- `cu` after an IP change (the link updates on the next discovery run; not exercised).
