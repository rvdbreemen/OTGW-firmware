# Home Assistant UI integration: Marketplace, custom integration or ingress

Date: 2026-10-25. Epic: TASK-1377 (children TASK-1377.1 to .8, backlog on `dev`).
This note is identical on `dev` (2.0.0) and `otgw-1.x.x` (1.x).

## Question

How should the OTGW firmware integrate with Home Assistant (HA), now that HA 2026.11
ships a built-in Marketplace, so that HA users can reach the real OTGW web UI from HA,
with minimal maintenance, and with the standard contracts (MQTT discovery, REST) kept
as the main workflow?

## Decision (maintainer, 2026-10-25)

- MQTT discovery and REST stay the only integration contracts. No second HA-specific contract.
- No custom HA integration, no dashboard card, no Marketplace listing.
- HA users reach the real OTGW UI through (1) a `configuration_url` "Visit device" link
  and (2) a sidebar panel provided by an existing open-source ingress component.
- The firmware only gets generic changes that also help any reverse proxy: base-path
  agnostic UI URLs and a CSRF check that works behind a trusted proxy, both only as far
  as the bench spike (item B) shows they are needed.

## 1. The HA Marketplace (HA 2026.11)

Facts from the HA documentation page for the Marketplace integration and the HACS docs:

- The Marketplace is HACS moved into HA core. Same catalog (`hacs/default`), same
  `hacs.json`, same HACS Action for validation. HACS cannot be installed on 2026.11+.
  Submission "works as before"; publisher docs move to contributors.home-assistant.io.
- It installs HA-side content only: integrations (`custom_components`), dashboard cards
  (`www/community`), themes, Jinja templates. AppDaemon apps and python_scripts are no
  longer managed. It cannot distribute device firmware.
- Users: Settings > Marketplace, accept a warning, install. Each installed repository gets
  an update entity (catalog checked every 6 hours) and a per-repository pre-release switch.
- Trust: "Nothing published in the Marketplace is tested, audited, or supported by the
  Home Assistant project." Content runs inside HA with access to the whole home.
- Inclusion requirements (HACS publish docs): public active GitHub repo with description,
  topics and issues; full GitHub releases; HACS Action green without ignores; `hacs.json`
  with at least `name`; OSI-approved licence; PR to `hacs/default` by the owner or a major
  contributor. Integrations also need hassfest, a valid manifest, a brand icon, and a
  domain that does not collide with a core integration (so not `opentherm_gw`); overriding
  core integrations is not accepted. Review of new additions "still take[s] months".
- Status caveat: the page was read on next.home-assistant.io (beta docs) before the
  2026.11 release.

## 2. Options considered

| Option | Verdict | Why |
|---|---|---|
| A. Nothing in the Marketplace, improve MQTT discovery | chosen (base) | already zero-install, stable contract |
| B. Dashboard card via the Marketplace | rejected | a second UI next to the v2 UI the maintainer wants to keep; a card only sees HA entities, it cannot talk to the device |
| C. Jinja templates | not pursued | niche |
| D. Custom integration (`otgw_firmware`) | rejected for now | second contract next to MQTT (conflicts with ADR-101 and KISS), duplicate entities unless discovery is switched off, HA changes internal APIs monthly, months of catalog review |
| E. Upstream work on core `opentherm_gw` | not pursued | out of scope for minimal maintenance |

What D would have added and why it is not needed now:
- One-click firmware update from HA. The device never does HTTPS, so it cannot fetch
  GitHub release assets itself; only an HA-side component could download and push them to
  `/safeboot` + `/update`. Firmware updates stay in the device UI, which becomes reachable
  from HA through the ingress panel. Revisit only on user demand (smallest step then: a
  thin companion integration that provides only an update entity).
- Config flow, zeroconf discovery, diagnostics, repairs: nice to have, not required for
  the goal.

## 3. Ways to show the OTGW UI in HA

| Level | Mechanism | Works with HA over HTTPS / Nabu Casa | Cost |
|---|---|---|---|
| 1 | `configuration_url` (MQTT abbreviation `cu`) in the discovery device block: HA shows "Visit device" | yes, as a new tab on the LAN | small, firmware only |
| 2 | Iframe panel or Webpage dashboard | no: browsers block an `http://` iframe inside an `https://` page (mixed content) | none, but limited to HA over HTTP on the LAN |
| 3 | Reverse proxy through HA (ingress) | yes, also remote, behind HA login | needs an ingress component; maybe firmware changes |

## 4. The ingress component: lovelylain/hass_ingress

Primary source: the project README (read 2026-10-25). Apache-2.0 licence. A custom
integration that brings Supervisor-style ingress to any web service.

- Work modes: `ingress` (default, proxied), `iframe` (like a Webpage dashboard), `auth`
  (nginx `auth_request` for backends ingress cannot proxy), `hassio`; `subapp`/`custom`
  are in the schema but not described.
- Proxied path: `/api/ingress/{name}/`. The variable `$http_x_ingress_path` expands to
  `/api/ingress/{name}`, and the backend can read an `X-Ingress-Path` header.
- The README warns: "Not all backend services can be proxied by ingress." The backend must
  use relative paths or `X-Ingress-Path`; otherwise use `rewrite` rules (body or header
  match/replace), nginx `sub_filter`, or `auth` mode on another domain.
- WebSocket: the TLS section says its settings "apply to this panel's HTTP and WebSocket
  proxy connections", so WebSocket proxying exists; not otherwise documented.
- `headers`: extra headers to the backend, "such as `authorization` for basic auth",
  with variables `$user_id`, `$user_name`, `$username`. A `host` header can be set.
  How Host, Origin and X-Forwarded-* are set by default is not documented.
- `require_admin` (default false) restricts the panel to HA admins. `ui_mode`:
  `normal` (hides header), `toolbar`, `replace`.
- Security notes: a `static_token` creates a public link that bypasses HA login (never use
  it for the OTGW); "Ignore certificate errors" is unsafe (not relevant: the device is HTTP).
- Graphical configuration needs HA 2025.8+; YAML works on older versions.
- Community reports say proxying works well for some apps and not for others, so the
  bench spike is required before any firmware change.

Alternatives to compare in the spike (reuse-open-source rule): the built-in Webpage
dashboard (no proxy, level 2 only), and any other maintained HA proxy component found at
the time of the spike.

Consequence for the firmware work: `rewrite` rules and `X-Ingress-Path` may make some or
all firmware path changes unnecessary. The spike (TASK-1377.3 / .4) first tests the
current firmware with rewrite rules only. Firmware changes (TASK-1377.5 / .6) cover only
what rewrite rules cannot fix cleanly.

## 5. Code findings per line (read 2026-10-25)

### 2.0.0 (`dev`, v2 UI)

- Discovery device block: `src/OTGW-firmware/MQTTHaDiscovery.cpp` (~l.2346) emits
  `mf`, `mdl`, `name`, `sw`; no `cu`. Origin block separately has name, sw, url.
- `v2.js` absolute URLs: `location.protocol + '//' + location.host + '/api/'` (APIGW, l.84),
  the WebSocket URL `ws(s)://` + `location.host` + `WS_PATH` (`'/ws'`, l.444/983), PIC base
  `location.host + '/'` (l.3701), and hardcoded `'/update'` (2x), `'/upload?path='`,
  `'/ReBoot'`, `'/ResetWireless'`, `'/api/'` (2x). `v2.html`: `href="/FSexplorer"`.
- The WebSocket is on port 80 at `/ws` (AsyncWebSocket on the shared server), so a
  single-URL proxy can carry it.
- CSRF (ADR-056): `isSameOriginRequest()` in `restAPI.ino:216` compares the host part of
  Origin (or Referer) with Host, and allows requests without Origin/Referer;
  `uploadSameOrigin()` in `FSexplorer.ino:635` mirrors it. Behind a proxy, Origin is the
  HA host and Host may be the device address, so admin POSTs (settings, uploads, OTA) can
  be rejected. Whether that happens depends on the Host header the proxy sends (the
  `headers` option can set `host`): measure in the spike.
- Concurrency: at most 2 API calls in flight (ADR-165); the proxy adds no extra clients
  but must not burst.
- mDNS advertises only `_http._tcp` on port 80 (`networkStuff.ino:649`).

### 1.x (`otgw-1.x.x`, classic UI)

- Discovery device block: `src/OTGW-firmware/mqtt_configuratie.cpp` (`kDev` ~l.1948);
  no `cu`. ESP8266: check discovery buffer headroom for the extra field.
- `index.js` absolute URLs: `location.host` + `'/api/'` (2x), `location.hostname` (3x),
  `'/pic?action=upgrade|refresh&name='`, `'/update'`, `'/update.png'`,
  `'/ResetWireless'`, `"/FSexplorer"`.
- The live-log WebSocket is a separate `WebSocketsServer` on port 81
  (`index.js` builds `'ws://' + wsHost + ':' + WEBSOCKET_PORT + '/'`, port 81 hardcoded in
  `webSocketStuff.ino`). A proxy on one base URL cannot carry it, and an HTTPS page blocks
  `ws://`. Expect the live log to be unavailable through ingress; the UI must degrade
  visibly instead of failing silently.
- CSRF (ADR-054 on 1.x): `isSameOriginRequest()` in `restAPI.ino:107`, used for admin
  routes and in `FSexplorer.ino:593`. Same proxy question as 2.0.0.

## 6. What each task takes from this note

| Task | Line | Uses sections |
|---|---|---|
| TASK-1377.1 / .2 configuration_url | 2.0.0 / 1.x | 3 (level 1), 5 (device block) |
| TASK-1377.3 / .4 bench spike | 2.0.0 / 1.x | 3, 4 (modes, rewrite, headers, require_admin), 5 (URLs, WS, CSRF) |
| TASK-1377.5 / .6 base path and CSRF | 2.0.0 / 1.x | 4 (X-Ingress-Path, rewrite), 5 |
| TASK-1377.7 / .8 manual | 2.0.0 / 1.x | 1 (trust), 3, 4 (require_admin, static_token warning) |

## 7. Open points

- Choose IP or hostname for `cu` (mDNS `.local` does not resolve on every client) and how
  it follows an IP change.
- Default Host/Origin/X-Forwarded-* behaviour of hass_ingress: not documented, measure.
- Whether WebSocket proxying in hass_ingress carries the 2.0.0 `/ws` live log: measure.
- Whether rewrite rules alone are enough on either line (then items C shrink to the
  CSRF part or disappear).
- If the device admin password is injected through `headers: authorization`, every HA
  admin effectively gets device admin. The manual must say so and require
  `require_admin: true`.

## Sources

- HA Marketplace integration (beta docs): https://next.home-assistant.io/integrations/marketplace
- HACS and the Marketplace: https://www.hacs.xyz/docs/marketplace/
- HACS, include default repositories: https://www.hacs.xyz/docs/publish/include/
- HACS issue #5590, HACS is moving to Home Assistant as Marketplace: https://github.com/hacs/integration/issues/5590
- HA contributor glossary, HACS: https://contributors.home-assistant.io/glossary/hacs/
- HA integration quality scale: https://developers.home-assistant.io/docs/core/integration-quality-scale/
- lovelylain/hass_ingress: https://github.com/lovelylain/hass_ingress
- Panel iFrame docs (mixed-content limitation): https://git.jeena.net/jeena/home-assistant.github.io/raw/branch/master/components/panel_iframe/index.html
- HA community, Ingress with custom URL: https://community.home-assistant.io/t/ingress-with-custom-url/327676/27
