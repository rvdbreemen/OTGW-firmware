---
id: TASK-405
title: >-
  Enforce platform-abstraction completeness in helperStuff.ino (raw
  ESP.getFreeHeap stragglers)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 18:34'
updated_date: '2026-04-24 18:49'
labels:
  - cleanup
  - 2.0.0
  - merge-followup
  - platform-abstraction
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two call-sites in helperStuff.ino still call raw ESP.getFreeHeap() instead of platformFreeHeap(): line 522 inside prepareForReboot() and line 930 inside getHeapFragmentation() helper. Both happen to be dual-core safe (ESP.getFreeHeap exists on both platforms), so there is no active bug — but they break the locally-enforced pattern that says "source files use the platform abstraction, only platform_esp8266.h/esp32.h reach into ESP.* directly". Replace both with platformFreeHeap() so the abstraction is maximally consistent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 helperStuff.ino:522 uses platformFreeHeap() instead of ESP.getFreeHeap()
- [x] #2 helperStuff.ino:930 uses platformFreeHeap() instead of ESP.getFreeHeap()
- [x] #3 git grep 'ESP\.getFreeHeap' src/OTGW-firmware/ --include='*.ino' --include='*.h' --include='*.cpp' returns only hits inside platform_esp8266.h or platform_esp32.h or inside explicit #ifdef ESP8266/ESP32 guards
- [x] #4 pio run -e esp8266 and pio run -e esp32 still succeed with identical memory footprint
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Open helperStuff.ino, replace ESP.getFreeHeap() with platformFreeHeap() at line 522 (prepareForReboot DebugTf).
2. Same replacement at line 930 (getHeapFragmentation helper).
3. Grep: `grep -n "ESP\\.getFreeHeap" src/OTGW-firmware/ --include="*.ino" --include="*.h" --include="*.cpp"` — should only hit platform_*.h and guarded blocks.
4. Build check (ESP8266 + ESP32).
5. Commit + mark ACs + Done.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Extended scope beyond the two helperStuff.ino sites to the entire source tree per user intent ("maximaal doorzetten"): all 23 raw ESP.getFreeHeap() call-sites outside the platform layer now go through platformFreeHeap(). Direct ESP.* heap reads are confined to platform_esp8266.h and platform_esp32.h, making the abstraction pattern enforceable at grep level.

**Changes (8 files):**
- `helperStuff.ino`: 1 code site (prepareForReboot log) + getHeapFragmentation() body replaced by a 1-line delegate to platformHeapFragmentation() + 1 comment updated
- `MQTTstuff.ino`: 9 sites (HEAP log lines + discovery heap gates + free_heap stat publish)
- `MQTTHaDiscovery.cpp`: 7 sites (streaming-discovery heap gates), plus added `#include "platform.h"`
- `mqtt_discovery_verify.cpp`: 3 sites
- `OTGW-Core.ino`: 2 sites (OTPROCESS_TRACE probe + OT baseline heap)
- `SATweather.ino`: 1 site (weather-fetch heap sanity)
- `OTGW-firmware.h`: no change (forward-decls unchanged)

**Behaviour unchanged:** ESP.getFreeHeap() is itself dual-core safe; this pass is code organisation, not a functional change.

**Builds:**
- esp8266 (Core 2.7.4): SUCCESS in 50s
- esp32 (pioarduino 55.03.35): SUCCESS in 1m53s

**Grep invariant held:** `grep -rn "ESP\.getFreeHeap()" src/OTGW-firmware/` returns only hits inside platform_esp8266.h (3 internal) and platform_esp32.h (2 internal). AC #3 satisfied.

**Commit:** 5a4f47b1
<!-- SECTION:FINAL_SUMMARY:END -->
