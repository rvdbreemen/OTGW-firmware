---
id: TASK-807
title: >-
  docs-security: document intentional unauthenticated network-identity
  disclosure on /api/v2/device/info (F1)
status: To Do
assignee: []
created_date: '2026-06-01 23:04'
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
- [ ] #1 Present-tense code comment at sendDeviceInfoV2 (restAPI.ino) states the endpoint is intentionally unauthenticated per ADR-032 and that network identity incl. topology is exposed by design
- [ ] #2 docs/api/README.md device/info section documents the wifi_current_* fields and references the trusted-LAN/no-auth posture
- [ ] #3 No code logic change; no behavior change
<!-- AC:END -->
