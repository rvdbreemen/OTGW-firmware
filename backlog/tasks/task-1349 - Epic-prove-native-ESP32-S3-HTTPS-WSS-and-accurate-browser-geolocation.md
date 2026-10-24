---
id: TASK-1349
title: 'Epic: prove native ESP32-S3 HTTPS/WSS and accurate browser geolocation'
status: To Do
assignee: []
created_date: '2026-10-24T10:55:00Z'
updated_date: '2026-10-24T19:11:00Z'
labels:
  - epic
  - https
  - esp32-s3
dependencies: []
references:
  - >-
    https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_https_server.html
  - TASK-1257
documentation:
  - docs/research/2026-10-24-esp32s3-native-https-prototype.md
  - docs/adr/ADR-003-http-only-no-https.md
  - docs/guides/pr-checklist.md
priority: high
type: feature
ordinal: 472000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ESP32-S3 migration makes encrypted local browser access feasible, but the current HTTP-only firmware prevents browser geolocation. Establish a measured native-device prototype and a reviewed implementation path, with trustworthy certificate provisioning, bounded resource use and recoverable updates. Success requires a real local browser connected directly to an ESP32-S3 over trusted HTTPS, obtaining a real browser location and verifying it against the installation site. This epic is a prototype and evidence programme; production rollout is a separate explicit decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A source-backed impact report separates verified configuration, estimates, unknowns and measured prototype results.
- [ ] #2 A maintainer-reviewed architecture decision defines the ESP32-S3 experiment and its relationship to ADR-003 without silently changing accepted decisions.
- [ ] #3 A real local browser loads the application directly from the ESP32-S3 using trusted HTTPS without a reverse proxy or certificate-error bypass.
- [ ] #4 The browser application obtains a real unmocked location with permission and demonstrates correctness against independently established installation coordinates under the documented accuracy threshold.
- [ ] #5 Confirmed coordinates reach the ESP32 over HTTPS and read back correctly after reboot; denial, timeout and wrong-location paths do not overwrite saved coordinates.
- [ ] #6 HTTPS REST, WSS, resource limits, provisioning and recovery have reproducible evidence, including flash/RAM deltas and concurrent-load behaviour.
- [ ] #7 An evidence-backed go/no-go documents supported board variants, browser coverage, remaining limitations and whether production integration is justified.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 Robert: parked ('Even parkeren'). Not created in this session; conflicts with the 'never HTTPS/WSS' project rule, so no work until he decides.
<!-- SECTION:NOTES:END -->
