# HA ingress and reverse proxy: 2.0.0 bench results (TASK-1377)

Date: 2026-10-25. Device: the 2.0.0 bench gateway (ESP32-S3 combo image, OT-Direct,
master mode, no boiler; WiFi). Old = 2.0.0-alpha.505 (origin/dev 613c286e6).
Fix = 2.0.0-alpha.507 (6da168984 + f3ebc29b4, firmware and web files flashed over
WiFi OTA; settings identical before and after every flash).
Rig: `scripts/ha-ingress-rig/` (Home Assistant 2026.10.4 + hass_ingress 1.4.0, Caddy
TLS front for HA, nginx 1.27 sub-path proxy with WebSocket upgrade, header echo
backend; headless Chromium 151). Port of the 1.x work, whose evidence is on the
`otgw-1.x.x` branch in `docs/research/2026-10-25-ha-ingress-bench-1x/`.

## Behaviour matrix (no gateway password)

`validate_ingress.py`, raw in `results_old.json` / `results_fix.json`. "esc" = UI
requests that left the proxy path; "ws" = live-log WebSocket frames received.

| Scenario | Old (alpha.505) | Fix (alpha.507) |
|---|---|---|
| direct `http://<ip>/` | works, ws frames | works, ws frames, 0 esc |
| HA ingress, HA over HTTP | no data, 13 esc (`/api/v2/...` 404 at the HA root), `/ws` 404, FSexplorer forms outside the path | works, 0 esc, write 200, upload 303, ws frames on `/api/ingress/otgw/ws` |
| HA ingress, HA over HTTPS | same as above | works, `wss://.../api/ingress/otgw/ws` frames |
| nginx sub-path, HTTPS and HTTP | same as above | works, ws frames on `/otgw-keephost/ws` |
| nginx default Host / HA `headers: host` | same as above | works (no password set) |

Old, HA over HTTPS: `old_ha_https_ingress.png`; fix: `fix_ha_https_ingress.png`.
Old/fix nginx: `old_nginx_https_subpath.png`, `fix_nginx_https_subpath.png`.
Direct access on the fix: `fix_direct.png`.

The first fix build (alpha.506) still loaded the three web fonts from `/fonts/` at
the host root (2 escaped requests per proxied scenario); f3ebc29b4 makes them
page-relative, after which 0 requests escape.

The ADR-165 cap held behind every proxy: `hd_rest_inflight_hwm` stayed 2 in all
scenarios. The device's busy gate answered a few REST calls with 503 behind the
proxies (1 to 5 per scenario, retried by the UI; 0 on direct access).

Live log: on 2.0.0 the log is `/ws` on port 80, so it works through any proxy that
carries WebSocket. The 1.x "needs direct access" notice is not needed here.

## Gateway admin password set (CSRF check, ADR-056)

`results_fix_password.json` (the browser answers the 401 itself):

| Scenario | settings write | upload |
|---|---|---|
| direct, HA ingress HTTP/HTTPS (default), nginx with Host kept | 200 | 303 |
| HA ingress with `headers: host: <ip>`, nginx default Host | 403 | 403 |

One nginx-HTTPS run timed out loading the page (30 s) with `hd_webconn_503` = 46:
with a password every request is first answered 401 and then repeated, which
doubles the load on the web server's connection gate (ADR-147). Two reruns passed.

curl straight to the device, `csrf_curl_before_after.txt`, identical on alpha.505
and alpha.507: no Origin 200, same Origin 200, `Origin: http://evil.example` 403,
no credentials 401; upload with the foreign Origin 403 and the file was not written,
upload with the same Origin 303. The CSRF code is not part of this change.

## Firmware update through HA ingress (one-action update, HTTPS)

`ota_via_ingress.py --one-action`, the update page's own flow: handover to the
safeboot updater, firmware upload, wait for the new firmware, web files.

| Run | Change | Result |
|---|---|---|
| 1 | 507 -> 506 | failed: the proxy answered `502 Bad Gateway` to the firmware upload after 141 s; the unit stayed in safeboot and, after a reboot, reported `app_valid: false`; recovered by uploading the firmware to `http://<ip>/update` (direct) |
| 2 | 507 -> 506 | ok, 161 s, "The unit runs 2.0.0-alpha.506 with matching web files" (`ota_one_action_run2.json`) |
| 3 | 506 -> 507 | ok, 148 s (`ota_one_action_run3.json`) |

hass_ingress turns any aiohttp client error towards the device into 502; its debug
log was switched on only after run 1, so the cause of run 1 is unknown. The same
firmware upload through nginx (streaming, `proxy_request_buffering off`) to safeboot
succeeded (41.8 s). One direct upload to safeboot (no proxy) also ended without an
HTTP answer (curl `000`) while the firmware was written fine. The update page now
warns instead of blocking when it is opened through a proxy (it cannot see the
unit's address); the guide tells users to update the firmware by IP when they can.

## configuration_url (cu)

Full discovery republish (`POST /api/v2/otgw/discovery`, drained to
`disc_pending_ids` 0) captured on alpha.505 and on alpha.507
(`discovery_diff.txt`): 402 topics on both, the same 402, 0 differ beyond
`dev.sw_version`, `dev.cu` and `origin.sw`; 35 configs carry the full device block
and on alpha.507 all 35 have `"cu":"http://<ip>/"`, none on alpha.505. Longest
payload 1020 -> 1049 bytes. Example blocks: `discovery_device_block_505.txt`,
`discovery_device_block_507.txt` (node id masked). The existing golden test
`tests/webui/ha-discovery-golden.test.mjs` passes on the alpha.507 dump
(MODERN_DUMP + NODE_ID). HA registry: `configuration_url` = `http://<ip>/`
(`ha_device_page.json`), the device page shows **Visit**
(`fix_ha_device_visit.png`) and the link opens the v2 UI
(`fix_ha_visit_opened.png`).

## Headers the device receives through hass_ingress 1.4.0

Same as on 1.x (echo backend, `results_fix.json`, scenarios `ha_https_echo*`): Host
is the browser's (`localhost:18443`), Origin `https://localhost:18443`,
X-Forwarded-Host/Proto set, X-Ingress-Path `/api/ingress/<name>`; with
`headers: host: <ip>` the Host becomes the device IP.

## Not verified

- PIC firmware flashing through a proxy (the bench is OT-Direct, `/pic` answers 503).
- The cause of the one 502 during a firmware upload through hass_ingress.
- Whether "Try again" on the update page recovers a failed upload through the proxy
  (recovery was done by IP).
- Caddy or Traefik in front of the gateway; Nabu Casa.
- `cu` after an IP change; Ethernet (the bench runs on WiFi).
