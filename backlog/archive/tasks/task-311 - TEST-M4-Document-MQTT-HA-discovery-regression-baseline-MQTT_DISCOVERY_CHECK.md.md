---
id: TASK-311
title: >-
  [TEST-M4] Document MQTT HA discovery regression baseline
  (MQTT_DISCOVERY_CHECK.md)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:22'
updated_date: '2026-04-19 07:05'
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
- [x] #1 docs/api/MQTT_DISCOVERY_CHECK.md contains a mosquitto_sub baseline for msgids 0, 1, 5, 16, 17, 25, 26, 28, 56, 126
- [x] #2 Baseline includes expected topic count post-boot on a healthy HA broker
- [x] #3 Procedure referenced from ADR-077
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/api/MQTT_DISCOVERY_CHECK.md: ~130-line baseline with mosquitto_sub procedure, 10-row expected-msgid table (0,1,5,16,17,25,26,28,56,126), required SAT switch/select discovery topics, device-info invariant check via jq, pre-commit diff procedure. References ADR-077 and the closed TASK-275/TASK-283 context so future pipeline reworkers have a starting point rather than discovering the invariants by trial and error.
<!-- SECTION:FINAL_SUMMARY:END -->
