---
id: TASK-807
title: >-
  docs-security: document intentional unauthenticated network-identity
  disclosure on /api/v2/device/info (F1)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-01 23:04'
updated_date: '2026-06-01 23:09'
labels:
  - docs
  - security
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding F1 (since-v1.5.0 multi-dimension review): the new wifi_current_subnet/gateway/dns1/dns2 keys on the unauthenticated GET /api/v2/device/info add LAN-topology detail to an endpoint that already exposes ipaddress/macaddress/ssid/chipid. Decision (user, 2026-06-02): accept under the trusted-LAN model and document; no gating. This posture is already covered by ADR-032 (no authentication on local network). Make it explicit so it is not re-flagged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Present-tense code comment at sendDeviceInfoV2 (restAPI.ino) states the endpoint is intentionally unauthenticated per ADR-032 and that network identity incl. topology is exposed by design
- [x] #2 docs/api/README.md device/info section documents the wifi_current_* fields and references the trusted-LAN/no-auth posture
- [x] #3 No code logic change; no behavior change
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Documented review finding F1 (no code logic change). Decision: accept the unauthenticated network-identity disclosure on GET /api/v2/device/info under the trusted-LAN model (ADR-032), rather than gate it.

Changes:
- restAPI.ino: present-tense comment at the Network identity block in sendDeviceInfoV2 stating the endpoint is intentionally unauthenticated per ADR-032 and that LAN topology (subnet/gateway/DNS) is exposed by design; warns against adding an auth gate without revisiting ADR-032.
- docs/api/README.md: added the four wifi_current_* fields to the device/info JSON example and a Security note explaining the intentional unauthenticated network-identity disclosure under the trusted-LAN model.

Rationale: gating only the 4 new keys is inconsistent theater (MAC/SSID/IP/chipid already public); gating the whole endpoint breaks the REST no-auth contract for marginal recon value. ADR-032 already covers this posture; the change just makes it explicit so it is not re-flagged.

Verification: python build.py --firmware -> Build completed successfully (exit 0); python evaluate.py --quick -> 34/34 pass, 0 fail, 100%.
<!-- SECTION:FINAL_SUMMARY:END -->
