---
id: TASK-897
title: >-
  feat-779-followup: tune per-consumer heap ladders (WS-strict/MQTT-relaxed)
  from ESP32-S3 telemetry
status: Done
assignee: []
created_date: '2026-06-21 12:44'
updated_date: '2026-06-30 04:30'
labels:
  - bug
  - websocket
  - heap
milestone: 2.0.0
dependencies: []
ordinal: 121000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-779. ADR-121 Option B independent per-consumer heap ladders shipped structurally (behaviour-equivalent step-1 = shared HEAP_* defaults). This task tunes the actual values once ESP32-S3 on-device telemetry exists. Inherits TASK-779 deferred ACs #1/#3/#4/#8.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Characterise WS live-log per-frame heap/maxBlock cost on ESP32-S3 via logHeapStats (live-log open vs closed)
- [ ] #2 WS live-log applies drop-to-latest/rate-cap backpressure under heap pressure beyond the current throttle
- [ ] #3 WS connection survives a heap-pressure event on ESP32-S3 bench: clean reconnect, no crash, no MQTT desync cascade
- [ ] #4 Relax MQTT publish-gate thresholds from telemetry (CRITICAL OOM floor unchanged), decoupled from WS gate; WS ladder stays strict
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
CLOSE 2026-06-30 per maintainer: follow-up to TASK-779, which was closed as FIXED BY THE 1.7.0 RELEASE. The per-consumer heap-ladder tuning here is superseded by that fix (the WS live-log reliability issue it addressed is resolved in the shipped firmware). Open ACs (WS per-frame heap characterisation, drop-to-latest backpressure, MQTT gate relax) no longer needed as separate 2.0.0 deliverables. Same disposition as the parent TASK-779.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as superseded: parent TASK-779 fixed by the 1.7.0 release; per-consumer heap-ladder tuning no longer a separate deliverable.
<!-- SECTION:FINAL_SUMMARY:END -->
