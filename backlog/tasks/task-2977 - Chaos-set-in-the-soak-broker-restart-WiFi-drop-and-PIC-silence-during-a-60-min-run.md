---
id: TASK-2977
title: >-
  Chaos set in the soak: broker restart, WiFi drop and PIC silence during a
  60-min run
status: To Do
assignee:
  - '@otgw-bench-tester'
created_date: '2026-11-20T16:28:00Z'
updated_date: '2026-11-20T16:34:00Z'
labels:
  - tests
  - bench
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1109000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test-pyramid gap analysis 2026-11-20 (Robert). Goal: prove recovery under the faults the field sees, inside the soak instead of by chance. During a 60-min soak on .61, inject: an MQTT broker restart; a WiFi drop (router or AP side, or a WiFi reconnect command); and on .63 (Classic, PIC) PIC silence. Never on .16. Pass criteria: no reboot (bootcount and /reboot_log.txt unchanged), no heap trend over the run, MQTT and Home Assistant entities back within N seconds after each fault (N proposed from the first run and recorded).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Recipe and script: one command runs the 60-min soak with the three injections at fixed times and logs bootcount, heap and MQTT reconnect time per fault
- [ ] #2 One run on .61 (broker restart, WiFi drop) and one on .63 (PIC silence) with evidence: no reboot, no heap trend, reconnect within N s, with N stated
<!-- AC:END -->
