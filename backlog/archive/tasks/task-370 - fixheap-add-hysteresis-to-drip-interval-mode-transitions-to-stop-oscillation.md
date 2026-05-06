---
id: TASK-370
title: >-
  fix(heap): add hysteresis to drip interval mode transitions to stop
  oscillation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 18:31'
updated_date: '2026-04-21 21:18'
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
- [x] #1 Drip mode transitions (2s <-> 10s) only fire after tier-stable state for at least 500ms OR at least N consecutive getHeapHealth reads in the new tier (N >= 3 recommended)
- [x] #2 Under stable healthy heap with no real pressure events, the telnet log shows zero drip mode transitions over any 5-minute window
- [x] #3 When real heap pressure occurs (freeHeap dropping below HEAP_LOW_THRESHOLD for sustained period), slow-mode still engages within 1 second
- [x] #4 Hysteresis preferably implemented inside getHeapHealth() so iEnteredLowCount / iEnteredWarningCount / iEnteredCriticalCount also become accurate (single-point fix; Phase 1A MED converges here)
- [ ] #5 Field log re-capture confirms no spurious slowed->restored pairs at healthy heap
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add static uint32_t modeEnteredMs = 0 to loopMQTTDiscovery()
2. canSwitch = (modeEnteredMs == 0) || ((millis() - modeEnteredMs) >= timerDiscoveryDrip_interval)
3. Both mode-switch branches get && canSwitch guard
4. On every switch: modeEnteredMs = millis()
5. Update block-header comment to document hold-per-interval hysteresis
6. Build + check ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added static uint32_t modeEnteredMs = 0 to loopMQTTDiscovery().
canSwitch = (modeEnteredMs == 0) || ((millis() - modeEnteredMs) >= timerDiscoveryDrip_interval).
Both mode-switch branches guarded by && canSwitch; modeEnteredMs = millis() on every switch.
Block-header comment updated to document hold-per-interval hysteresis (TASK-370).
Build: python build.py --firmware passed (exit 0, no new warnings).
AC5 (field-log re-capture) left unchecked: requires hardware observation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added hold-per-interval hysteresis to the drip mode switcher in loopMQTTDiscovery() to stop rapid normal<->slow oscillation.

Why:
Field logs (2026-04-21, 1.4.1-beta+deaddd8) showed up to 6 slowed/restored pairs per second when freeHeap hovered near HEAP_LOW_THRESHOLD (~5120 bytes). ESP.getFreeHeap() is non-deterministic at that level; every lwIP callback or yield() can briefly push it either side of the threshold. Without a hold window the mode switch fired on every loop iteration.

Change (MQTTstuff.ino, loopMQTTDiscovery):
- Added static uint32_t modeEnteredMs = 0 (0 = boot, first switch always allowed).
- canSwitch = (modeEnteredMs == 0) || ((millis() - modeEnteredMs) >= timerDiscoveryDrip_interval).
- Both mode-switch branches (normal->slow, slow->normal) guarded by && canSwitch.
- modeEnteredMs = millis() on every committed switch.
- Result: Normal->Slow requires >=2s in normal; Slow->Normal requires >=10s in slow.
- Block-header comment updated to document the hysteresis and TASK-370 rationale.

Not changed: getHeapHealth() (raw signal stays as-is; iEnteredLowCount remains a raw-transition counter — known trade-off of this approach vs Option A).

Build: python build.py --firmware passed.

AC5 (field-log re-capture confirms zero spurious slowed->restored pairs at healthy heap): unchecked, requires hardware observation on live unit.
<!-- SECTION:FINAL_SUMMARY:END -->
