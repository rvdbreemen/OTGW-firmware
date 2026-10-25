---
id: TASK-1377
title: >-
  Epic: Home Assistant UI integration via standard contracts and ingress (1.x
  and 2.0.0)
status: To Do
assignee: []
created_date: '2026-10-25T09:01:00Z'
updated_date: '2026-10-25T09:09:00Z'
labels:
  - epic
  - home-assistant
  - mqtt
  - webui
dependencies: []
references:
  - docs/research/2026-10-25-home-assistant-ui-integration.md
  - https://github.com/lovelylain/hass_ingress
  - https://next.home-assistant.io/integrations/marketplace
priority: medium
ordinal: 511000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Goal: give Home Assistant users access to the real OTGW web UI from inside HA, with minimal maintenance and without a second HA-specific contract. MQTT discovery and REST stay the only integration contracts. No custom integration, no dashboard card, no Marketplace listing (decision 2026-10-25, see session analysis of the HA 2026.11 Marketplace = HACS in core).

Approach: the HA side uses an existing open-source ingress component (candidate: lovelylain/hass_ingress, to be compared with at least one alternative incl. the built-in Webpage dashboard). The firmware side gets generic improvements that also help any reverse proxy.

Four work items (A-D), each on BOTH lines as separate child tasks:
A. configuration_url (cu) in the MQTT discovery device block -> HA 'Visit device' link.
   2.0.0: src/OTGW-firmware/MQTTHaDiscovery.cpp device block (~l.2346, emits mf/mdl/name/sw, no cu).
   1.x:   src/OTGW-firmware/mqtt_configuratie.cpp device block (kDev ~l.1948, no cu).
B. Bench spike: run the current UI behind hass_ingress (HA over HTTP and over HTTPS), record exactly what breaks (paths, CSRF, WebSocket, uploads/OTA, redirects). Measure first, then build.
C. Make the UI base-path agnostic and the CSRF check proxy-safe, scoped by B's findings.
   2.0.0 v2 UI: v2.js uses location.host + '/api/', + WS_PATH ('/ws'), + '/' (PIC); hardcoded '/update', '/upload?path=', '/ReBoot', '/ResetWireless', '/api/'; v2.html href="/FSexplorer". CSRF: isSameOriginRequest() restAPI.ino:216 and uploadSameOrigin() FSexplorer.ino:635 (ADR-056).
   1.x classic UI: index.js uses location.host/hostname + '/api/', '/pic?action=...', '/update', '/ResetWireless', '/FSexplorer'. WebSocket is a separate WebSocketsServer on port 81 ('ws://' + host + ':81/'), which an ingress proxy on one base URL cannot carry and which an HTTPS page blocks: live log must degrade gracefully. CSRF: isSameOriginRequest() restAPI.ino:107 (ADR-054 on 1.x).
D. Manual (EN/NL) with a tested hass_ingress example config, the admin-only restriction and known limits.

Constraints: HTTP/WS only, never HTTPS/WSS on the device; at most 2 concurrent API calls (ADR-165, 2.0.0); PROGMEM/no String rules; CSRF changes are security-relevant and need an ADR amendment on each line (separate ADR numbering, coherent decision). Cross-worktree rule: one master plan (this epic) approved by the maintainer before any child starts; 1.x work happens in its own otgw-1.x.x worktree.

Ordering: A is independent. C depends on B per line. D depends on C per line.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All eight child tasks are Done, or explicitly dropped with a reason
- [ ] #2 On both lines, a HA instance shows a working 'Visit device' link for the OTGW device, shown by a screenshot
- [ ] #3 On both lines, the OTGW UI opens from the HA sidebar through the chosen ingress component with HA over HTTPS, read and admin actions work, shown by a bench transcript or screenshots
- [ ] #4 MQTT discovery and REST contracts are unchanged apart from the added cu field, shown by a discovery payload diff
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Research 2026-10-25 recorded in docs/research/2026-10-25-home-assistant-ui-integration.md (identical copy on dev and otgw-1.x.x). Key facts: the HA 2026.11 Marketplace is HACS in core (same hacs/default catalog and hacs.json, months of review, nothing audited by HA); options B (card) and D (custom integration) rejected for maintenance and contract duplication; hass_ingress (Apache-2.0) proxies at /api/ingress/{name}/, sends X-Ingress-Path, supports body/header rewrite rules, extra headers (incl. host and authorization) and require_admin. New since the epic was written: rewrite rules may fix absolute UI paths without firmware changes, so the spikes test rewrite-only first and items C shrink to what rewrite cannot fix.

Maintainer decision 2026-10-25 (confirmed): go for the minimal integration and make HA ingress work properly now. No new integration in the HA Marketplace for now (no custom integration, no card). Scope stays items A-D on both lines.
<!-- SECTION:NOTES:END -->
