---
id: TASK-661
title: 'Harden resetgateway MQTT command: payload validation + rate-limit'
status: To Do
assignee: []
created_date: '2026-05-22 05:52'
labels:
  - security
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The new resetgateway set-command (MQTTstuff.ino:704-707, commit 908e1e16) lets any LAN client publish to <toptopic>/set/<nodeid>/resetgateway to trigger resetOTGW() (PIC hardware reset) with no payload validation — payload_press: '1' in HA discovery is convention only, the dispatch ignores the payload entirely. Consistent with ADR-032 (no app-level auth) but expands the unauthenticated blast radius from 'tune behaviour' to 'induce hardware reset'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTTstuff.ino:704-707 resetgateway dispatch requires exact payload match ('1' OR 'press' OR 'PRESS' — pick one and document in ADR/CHANGELOG)
- [ ] #2 resetgateway rate-limited: subsequent calls within N seconds (suggest 5-10s) are silently dropped with a DebugTln log line; N defined as a const at top of the file
- [ ] #3 HA discovery payload_press value matches whatever the dispatch now requires (no drift between discovery and implementation)
- [ ] #4 Field-test: rapid-fire MQTT publish of resetgateway only resets once per window; non-matching payloads are logged + ignored
<!-- AC:END -->
