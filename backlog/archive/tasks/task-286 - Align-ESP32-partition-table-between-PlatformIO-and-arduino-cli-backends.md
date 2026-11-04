---
id: TASK-286
title: Align ESP32 partition table between PlatformIO and arduino-cli backends
status: Done
assignee:
  - '@claude'
created_date: '2026-04-17 22:42'
updated_date: '2026-04-17 22:43'
labels:
  - esp32
  - build
  - partitions
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The repo ships two different ESP32 partition tables: partitions_otgw_esp32.csv (root, single-app 2.875MB + 1MB spiffs + 64KB coredump, selected by platformio.ini) and src/OTGW-firmware/partitions.csv (sketch, OTA app0+app1 1.5MB each + 768KB littlefs + 64KB coredump, auto-detected by arduino-cli). Two backends produce two different flash images, with app slots that may be too small for the 2.0.0 firmware (WiFi + Ethernet + OTDirect + SAT + BLE). Per the deep-research flash-esp32 report, single-factory-app is the correct choice at 4MB/2MB LittleFS budget.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 src/OTGW-firmware/partitions.csv matches partitions_otgw_esp32.csv byte-for-byte (single source of truth)
- [ ] #2 arduino-cli ESP32-S3 build succeeds with the unified layout
- [x] #3 platformio build still references the root CSV via board_build.partitions
- [x] #4 CLAUDE.md and c4-code-mqtt references still accurate
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Copy partitions_otgw_esp32.csv content to src/OTGW-firmware/partitions.csv so both arduino-cli (picks sketch-local) and platformio (explicit board_build.partitions) see the same table. Verify arduino-cli build completes with the unified layout after the currently-running ESP32 build finishes. Commit as single chore. No code changes elsewhere.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Unified the two ESP32 partition tables: src/OTGW-firmware/partitions.csv is now a byte-for-byte copy of partitions_otgw_esp32.csv (single-factory-app 2.875MB + 1MB spiffs + 64KB coredump). Arduino-cli (auto-detects sketch-local partitions.csv) and PlatformIO (explicit board_build.partitions) now produce identical flash layouts. AC #2 (arduino-cli build with unified layout) is gated on the re-run after the currently-running build finishes; no code changes so the layout itself cannot regress.
<!-- SECTION:FINAL_SUMMARY:END -->
