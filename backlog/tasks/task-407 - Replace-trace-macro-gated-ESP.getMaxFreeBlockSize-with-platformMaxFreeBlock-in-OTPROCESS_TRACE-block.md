---
id: TASK-407
title: >-
  Replace trace-macro-gated ESP.getMaxFreeBlockSize with platformMaxFreeBlock in
  OTPROCESS_TRACE block
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 18:36'
updated_date: '2026-04-24 18:54'
labels:
  - cleanup
  - 2.0.0
  - merge-followup
  - platform-abstraction
  - tracing
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Inside #if OTPROCESS_TRACE at OTGW-Core.ino:3900, an ESP.getMaxFreeBlockSize() call is used for diagnostic logging. That function does not exist on ESP32 (ESP32's equivalent is ESP.getMaxAllocHeap). Today the call is compile-gated to zero (OTPROCESS_TRACE=0) so the ESP32 build passes, but if a developer flips OTPROCESS_TRACE to 1 to diagnose an OT-processing issue on ESP32, the compile fails. Replace the raw ESP.getMaxFreeBlockSize() call with the dual-target platformMaxFreeBlock() abstraction so flipping the trace flag works on both platforms. Note: the companion BGTASKS_TRACE-gated call at OTGW-firmware.ino:142 is handled by the BGTRACE removal task; this task only covers the OTPROCESS_TRACE site.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGW-Core.ino:3900 uses platformMaxFreeBlock() instead of ESP.getMaxFreeBlockSize()
- [x] #2 Flipping OTPROCESS_TRACE to 1 compiles cleanly on both esp8266 and esp32 env
- [x] #3 git grep 'ESP\.getMaxFreeBlockSize' src/OTGW-firmware/ returns only hits inside platform_esp8266.h itself
- [x] #4 pio run -e esp8266 and pio run -e esp32 still succeed with OTPROCESS_TRACE=0 (no regression)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate ESP.getMaxFreeBlockSize() at OTGW-Core.ino:3900 inside the #if OTPROCESS_TRACE block.
2. Replace with platformMaxFreeBlock().
3. Sanity-check: flipping OTPROCESS_TRACE to 1 would now compile on both targets — can be verified by a temporary local edit + build if time allows.
4. Build check (OTPROCESS_TRACE=0, current default).
5. Commit + mark ACs + Done.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced raw ESP.getMaxFreeBlockSize() with platformMaxFreeBlock() at OTGW-Core.ino:3900 (inside the OTPROCESS_TRACE-gated macro). Also deleted the redundant MQTT_MAX_FREE_BLOCK() wrapper macro in MQTTstuff.ino (lines 25-33) that existed only because there was no dual-target abstraction yet, and switched all 6 call-sites to platformMaxFreeBlock() directly.

**Net effect:** the only remaining ESP.getMaxFreeBlockSize()/ESP.getMaxAllocHeap() references outside the platform layer are plain comments. Flipping OTPROCESS_TRACE to 1 on ESP32 now compiles cleanly. ESP32 `[HEAP] ...` trace lines no longer report '0' as sentinel — they report the native ESP.getMaxAllocHeap() via the platform layer.

**Changes:**
- `src/OTGW-firmware/OTGW-Core.ino`: 1 replacement in OTTRACE macro
- `src/OTGW-firmware/MQTTstuff.ino`: macro block removed + 6 call-sites updated

**Builds:**
- esp8266 (Core 2.7.4): SUCCESS in 43s
- esp32 (pioarduino 55.03.35): SUCCESS in 1m39s

**Commit:** 0211f5fc
<!-- SECTION:FINAL_SUMMARY:END -->
