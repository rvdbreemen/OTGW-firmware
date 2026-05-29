---
id: TASK-741
title: >-
  ESP abstraction Tier 1: collapse SATweather and SATble per-line ifdefs to
  feature-flag gates; switch raw ESP.* calls to existing shims
status: Done
assignee:
  - '@claude'
created_date: '2026-05-28 08:28'
updated_date: '2026-05-29 22:03'
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
- [x] #1 SATweather.ino contains zero raw ESP-platform conditionals; all gates use HAS_WEATHER_FORECAST
- [x] #2 SATble.ino file-level guard reads #if HAS_SAT_BLE not #if defined(ESP32)
- [x] #3 restAPI.ino runtime.heap_* JSON fields publish unconditionally via platformFreeHeap/platformMinFreeHeap/platformMaxFreeBlock/platformHeapFragmentation - no #if defined(ESP32) around lines 1640
- [x] #4 handleDebug.ino heap diagnostics block uses platform*() shims; the #if defined(ESP32) around lines 26-33 is removed
- [x] #5 OLED.ino:279 calls platformFreeHeap()
- [x] #6 MQTTstuff.ino:1922 calls platformFreeHeap()
- [x] #7 python build.py --firmware exits 0
- [x] #8 python evaluate.py --quick exits 0 and the abstraction guardrail violation count has dropped by at least 15 versus the baseline in the audit doc
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Tier 1 complete (commit 4456e65d, alpha.103). SATweather.ino 12 raw ESP conditionals -> HAS_WEATHER_FORECAST; SATble.ino -> HAS_SAT_BLE; restAPI.ino + handleDebug.ino heap fields -> platform shims unconditional; MQTTstuff.ino + OLED.ino getFreeHeap -> platformFreeHeap. ESP_ABSTRACTION_BASELINE 73->58 (-15). Also fixed evaluate.py check_sat_publishes_use_helpers to skip the gitignored *.ino.cpp concat artifact (it stripped ADR-111 exception markers and false-flagged sat/target). Build clean both targets; evaluate 66 passed 0 failed.
<!-- SECTION:FINAL_SUMMARY:END -->
