---
id: TASK-865.16
title: >-
  fix(build): give esp32-combo its own partition table (2 MB app / 1.875 MB
  LittleFS)
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-14 15:48'
updated_date: '2026-06-21 07:51'
labels:
  - async-esp32s3
dependencies: []
parent_task_id: TASK-865
ordinal: 79000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why
The esp32-combo binary links BOTH OT engines (PIC OTGWSerial + OTDirect, ADR-127) so it is inherently larger than the single-engine esp32/esp32-classic builds. It overflows the shared 1.875 MB (0x1E0000) app0 slot: PlatformIO post-link check failed at 1986399 B > 1966080 B (101%) at alpha.191. It compiles+links clean; only the partition-fit fails. esp32 (~98.6%) and esp32-classic (~95.8%) fit and ship fine.

## What
Give the combo its OWN partition table so the shared partitions_otgw_esp32.csv (used by esp32 + esp32-classic) is NOT touched — changing the shared app/fs boundary would corrupt LittleFS on OTA-upgraded esp32/esp32-classic devices (data sits at the old 0x1F0000 offset). The combo is still transitional/field-validation, so a new layout for it is safe.

New partitions_otgw_esp32_combo.csv (mirrors the single-target layout, app and fs swapped):
- app0  ota_0  0x10000  0x200000   (2.0 MB, was 1.875 MB) -> fits 1986399 with ~108 KB margin
- spiffs spiffs 0x210000 0x1E0000   (1.875 MB LittleFS, was 2.0 MB)
- coredump 0x3F0000 0x10000 (unchanged) -> ends at 0x400000 (4 MB)
The web assets are far under 1.875 MB (the 2 MB littlefs.bin is a mostly-empty full-partition image), so shrinking fs is free.

Wiring:
- platformio.ini [env:esp32-combo]: board_build.partitions = partitions_otgw_esp32_combo.csv (override the inherited esp32 value).
- build.py TARGETS['esp32-combo']: override app_size=0x200000, fs_offset=0x210000, fs_size=0x1E0000 so the merge step + size report match the new layout.

## Anchors
- partitions_otgw_esp32.csv (shared, DO NOT edit), new partitions_otgw_esp32_combo.csv
- platformio.ini [env:esp32-combo] (extends env:esp32, inherits board_build.partitions)
- build.py TARGETS['esp32'] tcfg (fs_offset/fs_size/app_size, lines ~78-89) inherited by TARGETS['esp32-combo']
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New partitions_otgw_esp32_combo.csv exists: app0 0x10000/0x200000, spiffs 0x210000/0x1E0000, coredump 0x3F0000/0x10000; offsets sum to 0x400000
- [ ] #2 Shared partitions_otgw_esp32.csv is UNCHANGED (esp32 + esp32-classic OTA partition compatibility preserved)
- [ ] #3 platformio.ini [env:esp32-combo] overrides board_build.partitions to the combo CSV; build.py TARGETS['esp32-combo'] overrides app_size/fs_offset/fs_size
- [x] #4 python build.py: esp32, esp32-classic AND esp32-combo all show per-env SUCCESS (grep-verified); combo no longer overflows
- [x] #5 evaluate.py --quick: no new failures
- [ ] #6 FIELD (epic TASK-865): the combo merged bin flashes and boots on both OTGW32 and Classic-with-PIC hardware, LittleFS web UI intact
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DONE (build): combo now uses partitions_otgw_esp32_combo.csv (2.0 MB app0 / 1.875 MB spiffs). Full build.py at alpha.192: esp32, esp32-classic AND esp32-combo all per-env SUCCESS; combo fits at 94.7% (1986395/2097152 B), was 101% overflow. Shared partitions_otgw_esp32.csv UNTOUCHED so esp32/esp32-classic OTA fs offset is unchanged. evaluate.py --quick 0 failures. Field AC (flash+boot on OTGW32 + Classic hardware, LittleFS UI intact) remains for hardware soak under epic TASK-865.

3-target build verified GREEN at HEAD (alpha.232): esp32, esp32-classic, esp32-combo all SUCCESS (fw+fs); esp32-combo bin now FITS (no overflow). evaluate.py --quick 0-fail. Code ACs were verified by the planning pass reading the committed source. Remaining = field/hardware AC(s) for Robert. esp32-combo SUCCESS confirms AC#4 (combo fits).
<!-- SECTION:NOTES:END -->
