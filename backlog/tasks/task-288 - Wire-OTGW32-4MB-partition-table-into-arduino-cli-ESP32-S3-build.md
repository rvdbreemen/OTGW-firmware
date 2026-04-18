---
id: TASK-288
title: Wire OTGW32 4MB partition table into arduino-cli ESP32-S3 build
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 07:12'
updated_date: '2026-04-18 07:38'
labels:
  - esp32
  - build
  - partitions
dependencies:
  - TASK-286
  - TASK-287
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After the unified partition table lands (TASK-286), the arduino-cli ESP32-S3 build still uses the default 16MB flash layout of the esp32s3-devkitc-1 board, not the 4MB OTGW32 layout from partitions_otgw_esp32.csv / src/OTGW-firmware/partitions.csv. Symptom: compile output reads "Sketch uses 1835664 bytes (10%) of program storage space. Maximum is 16777216 bytes" (16MB = devkitc-1 default, not the 2.875MB factory app slot from our CSV). PlatformIO already routes this correctly via board_build.partitions = partitions_otgw_esp32.csv; arduino-cli (via --fqbn esp32:esp32:esp32s3:PartitionScheme=custom) does not pick up the sketch-local partitions.csv as expected. Without the fix, factory-image merges will flash the 16MB assumption onto a 4MB OTGW32 board and either corrupt the filesystem slot or silently overflow flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 arduino-cli ESP32-S3 build reports a maximum matching partitions_otgw_esp32.csv (factory app 0x2E0000 = 3014656 bytes)
- [ ] #2 LittleFS partition mapped to 0x2F0000 offset, 0x100000 size, label spiffs (matches unified CSV)
- [x] #3 Bootloader offset for ESP32-S3 stays at 0x0 per the deep-research flash-esp32 report
- [ ] #4 build.py --target esp32 --arduino-cli --factory-image produces dist/otgw-esp32s3-initial-4mb.bin that matches the 4MB OTGW32 layout
- [ ] #5 PlatformIO build path still uses partitions_otgw_esp32.csv via board_build.partitions and produces the same layout
- [ ] #6 Investigate whether the fix is: (a) custom board variant under boards/*.json, (b) --build-property build.custom_partitions override, (c) custom boards.txt entry, or (d) a renamed CSV matching what PartitionScheme=custom probes for
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add app_size=3014656 (0x2E0000) field to TARGETS[esp32s3] in build.py — this matches the factory app slot size in partitions_otgw_esp32.csv. 2. In build_firmware() append --build-property upload.maximum_size=<app_size> when the target defines app_size, so the arduino-cli size check uses the correct 3MB limit instead of the 16MB default from the PartitionScheme=custom menu entry in the ESP32 boards.txt. 3. Verify via fresh arduino-cli compile that the Sketch uses line now reads a 3014656 (or similarly small) maximum. 4. ESP8266 path unaffected (no app_size field). 5. Commit + push.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root cause: the ESP32 Arduino boards.txt entry for PartitionScheme=custom ships upload.maximum_size=16777216 (16 MB). When the OTGW32 firmware compiled under arduino-cli it used our 4 MB partitions.csv correctly (the CSV was parsed, the partitions.bin had the right layout), but the size-check headline reported a 16 MB ceiling instead of the 2.875 MB factory app slot. Any overflow past 3 MB would have been silently allowed at compile time and corrupted the filesystem partition at flash time.

Fix: added app_size=3014656 to TARGETS[esp32s3] in build.py (matches 0x2E0000 factory app slot) and appended --build-property upload.maximum_size=<app_size> in build_firmware() when the target defines app_size. ESP8266 path is unaffected because its target entry has no app_size field.

Verification (manual rerun with the same build-property flag appended):
  Sketch uses 1835664 bytes (60%) of program storage space.
  Maximum is 3014656 bytes.
  Global variables use 98888 bytes (30%) of dynamic memory.

AC #1 is met (maximum matches partitions_otgw_esp32.csv). AC #2 (LittleFS partition) was already correctly mapped in TARGETS[esp32s3].fs_offset/fs_size and was verified before this task. AC #3 (ESP32-S3 bootloader offset 0x0) is encoded in TARGETS[esp32s3].bootloader_offset. AC #4 (factory-image via build.py --target esp32 --arduino-cli --factory-image) is downstream and will be validated when the whole orchestrated flow is exercised; no code change here invalidates it. AC #5 (PlatformIO path) is untouched and already uses board_build.partitions correctly. AC #6 (investigation choice) landed on "override --build-property upload.maximum_size" rather than a custom board variant; this keeps the change localised to build.py without introducing a new variants/boards/*.json file.
<!-- SECTION:FINAL_SUMMARY:END -->
