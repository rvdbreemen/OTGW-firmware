---
id: TASK-1664
title: 'MQTT: 13 ''TCP disconnected'' error lines within 40 ms right after a reconnect'
status: To Do
assignee: []
created_date: '2026-11-05T20:24:00Z'
labels:
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 787000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .61 beta.101 (TASK-1451 AC5a, 2026-11-05 20:38:25.061-.098): right after the reconnect via the held broker address, 13 'PrintMQTTErr: Error: MQTT TCP disconnected' lines appeared in 40 ms with heap dipping to 34 KB; MQTT stayed connected. Likely queued publishes failing in the reconnect burst. Unknown whether it predates TASK-1451. Evidence: lead scratchpad bench-101/telnet.log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced on old code (before TASK-1451) or shown to be new
- [ ] #2 Explained from the code; fixed or documented as harmless
<!-- AC:END -->
