---
id: TASK-370
title: >-
  fix(heap): add hysteresis to drip interval mode transitions to stop
  oscillation
status: To Do
assignee: []
created_date: '2026-04-21 18:31'
labels:
  - code-review
  - heap
  - mqtt
  - quality-of-life
  - telemetry
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field logs from 2026-04-21 (1.4.1-beta+deaddd8, healthy heap ~12KB free, maxBlock 5672) show the discovery drip rapidly toggling between normal (2s) and slow (10s) mode up to 6x per second with 4-60ms gaps between transitions. Example: 20:24:20.437 slowed -> 449 restored -> 453 slowed -> 469 restored -> 473 slowed -> 530 restored. Same class as Phase 1A MED and Phase 2B MED-2 tier-transition counter inflation, but here the effect surfaces in loopMQTTDiscovery's mode log lines, flooding the telnet output. Not a crash-class issue; quality-of-life and telemetry fidelity. iEnteredLowCount / iEnteredWarningCount in the hourly stats/heap JSON will also be inflated without this fix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Drip mode transitions (2s <-> 10s) only fire after tier-stable state for at least 500ms OR at least N consecutive getHeapHealth reads in the new tier (N >= 3 recommended)
- [ ] #2 Under stable healthy heap with no real pressure events, the telnet log shows zero drip mode transitions over any 5-minute window
- [ ] #3 When real heap pressure occurs (freeHeap dropping below HEAP_LOW_THRESHOLD for sustained period), slow-mode still engages within 1 second
- [ ] #4 Hysteresis preferably implemented inside getHeapHealth() so iEnteredLowCount / iEnteredWarningCount / iEnteredCriticalCount also become accurate (single-point fix; Phase 1A MED converges here)
- [ ] #5 Field log re-capture confirms no spurious slowed->restored pairs at healthy heap
<!-- AC:END -->
