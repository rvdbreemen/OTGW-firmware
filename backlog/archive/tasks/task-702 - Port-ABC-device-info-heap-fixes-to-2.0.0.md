---
id: TASK-702
title: Port A+B+C device-info heap fixes to 2.0.0
status: Done
assignee: []
created_date: '2026-05-25 12:44'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port three performance improvements from dev branch (commit c6bb72b2) to the 2.0.0 ESP32/SAT feature branch to reduce heap pressure and TCP yield points on /api/v2/device/info.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Change A: BootFlashCache struct and cacheBootFlashInfo() added to restAPI.ino using platform abstraction functions
- [ ] #2 Change B: Coalescing send buffer already present in jsonStuff.ino; confirmed no changes needed
- [ ] #3 Change C: DEVICE_INFO_MIN_HEAP_BLOCK=8192 heap guard added to sendDeviceInfoV2() using platformMaxFreeBlock()
- [ ] #4 cacheBootFlashInfo() called in setup() after LittleFS.begin() in OTGW-firmware.ino
- [ ] #5 Build passes: python build.py --firmware exits 0 (ESP8266 SUCCESS; ESP32 compile+link pass, esptool packaging issue pre-existing)
- [ ] #6 Evaluator passes: evaluate.py --quick shows no new failures vs pre-change baseline
<!-- AC:END -->
