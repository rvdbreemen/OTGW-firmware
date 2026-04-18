---
id: TASK-288
title: Wire OTGW32 4MB partition table into arduino-cli ESP32-S3 build
status: To Do
assignee: []
created_date: '2026-04-18 07:12'
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
- [ ] #1 arduino-cli ESP32-S3 build reports a maximum matching partitions_otgw_esp32.csv (factory app 0x2E0000 = 3014656 bytes)
- [ ] #2 LittleFS partition mapped to 0x2F0000 offset, 0x100000 size, label spiffs (matches unified CSV)
- [ ] #3 Bootloader offset for ESP32-S3 stays at 0x0 per the deep-research flash-esp32 report
- [ ] #4 build.py --target esp32 --arduino-cli --factory-image produces dist/otgw-esp32s3-initial-4mb.bin that matches the 4MB OTGW32 layout
- [ ] #5 PlatformIO build path still uses partitions_otgw_esp32.csv via board_build.partitions and produces the same layout
- [ ] #6 Investigate whether the fix is: (a) custom board variant under boards/*.json, (b) --build-property build.custom_partitions override, (c) custom boards.txt entry, or (d) a renamed CSV matching what PartitionScheme=custom probes for
<!-- AC:END -->
