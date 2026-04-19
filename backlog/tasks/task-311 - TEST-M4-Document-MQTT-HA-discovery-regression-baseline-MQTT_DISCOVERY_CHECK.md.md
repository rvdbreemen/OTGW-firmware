---
id: TASK-311
title: >-
  [TEST-M4] Document MQTT HA discovery regression baseline
  (MQTT_DISCOVERY_CHECK.md)
status: To Do
assignee: []
created_date: '2026-04-18 19:22'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Streaming discovery rewrite is the biggest MQTT change in the branch. TASK-275 closed as superseded, so heap-stability ACs never cleanly passed. No documented way to diff future pipeline reworks.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 docs/api/MQTT_DISCOVERY_CHECK.md contains a mosquitto_sub baseline for msgids 0, 1, 5, 16, 17, 25, 26, 28, 56, 126
- [ ] #2 Baseline includes expected topic count post-boot on a healthy HA broker
- [ ] #3 Procedure referenced from ADR-077
<!-- AC:END -->
