---
id: TASK-1431
title: MQTT feedback when a PIC command is dropped after 5 retries (Tweakers request)
status: To Do
assignee: []
created_date: '2026-10-28T20:28:00Z'
labels:
  - enhancement
  - needs-maintainer-ok
dependencies: []
references:
  - https://gathering.tweakers.net/forum/list_message/86420402#86420402
  - https://gathering.tweakers.net/forum/list_message/86427540#86427540
priority: low
ordinal: 258000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
sjorsjuhmaniac (Tweakers, 2026-10-24/25, 1.7.7, came from ESPEasy + otgw2mqtt): after 5 failed attempts handleOTGWqueue() (OTGW-Core.ino ~3125) drops a command and only logs it (debug + WebSocket '!' event). He wants HA to see the failure, e.g. a TT=22.0 that never lands. Proposed one-line diff: sendMQTTData(F("command_retry_exceeded"), ON). Open design points he raised himself: which command failed, latching or not, how long, cleared by user ack or next successful command, non-persistent. Feature request, not a bug; needs maintainer decision on topic shape before work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Maintainer decided topic shape, payload (which command) and clear semantics
- [ ] #2 A dropped command produces an MQTT publish naming the command, verified on bench by forcing a retry exhaustion
<!-- AC:END -->
