---
id: TASK-741
title: >-
  ESP abstraction Tier 1: collapse SATweather and SATble per-line ifdefs to
  feature-flag gates; switch raw ESP.* calls to existing shims
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-28 08:28'
updated_date: '2026-05-29 21:47'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 68000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two distinct cleanup wins that together remove ~20 leaks without inventing any new shims. (a) Replace the 11 inner #ifndef ESP8266 blocks scattered across SATweather.ino with one outer #if HAS_WEATHER_FORECAST. Replace SATble.ino's file-level #if defined(ESP32) with #if HAS_SAT_BLE. (b) Substitute raw ESP.getXxx() calls with the existing platform*() shims at: restAPI.ino:1639/1641/1642/1644/1646 (heap stats - drop the #ifdef entirely), handleDebug.ino:25/27/28/30/32 (heap stats - drop the #ifdef), OLED.ino:279 (single getFreeHeap), MQTTstuff.ino:1922 (getFreeHeap in heap predicate). Depends on Tier 0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SATweather.ino contains zero raw ESP-platform conditionals; all gates use HAS_WEATHER_FORECAST
- [ ] #2 SATble.ino file-level guard reads #if HAS_SAT_BLE not #if defined(ESP32)
- [ ] #3 restAPI.ino runtime.heap_* JSON fields publish unconditionally via platformFreeHeap/platformMinFreeHeap/platformMaxFreeBlock/platformHeapFragmentation - no #if defined(ESP32) around lines 1640
- [ ] #4 handleDebug.ino heap diagnostics block uses platform*() shims; the #if defined(ESP32) around lines 26-33 is removed
- [ ] #5 OLED.ino:279 calls platformFreeHeap()
- [ ] #6 MQTTstuff.ino:1922 calls platformFreeHeap()
- [ ] #7 python build.py --firmware exits 0
- [ ] #8 python evaluate.py --quick exits 0 and the abstraction guardrail violation count has dropped by at least 15 versus the baseline in the audit doc
<!-- AC:END -->
