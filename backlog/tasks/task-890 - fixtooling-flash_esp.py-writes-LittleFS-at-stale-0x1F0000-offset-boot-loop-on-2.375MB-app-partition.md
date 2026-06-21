---
id: TASK-890
title: >-
  fix(tooling): flash_esp.py writes LittleFS at stale 0x1F0000 offset -> boot
  loop on 2.375MB-app partition
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-20 15:40'
updated_date: '2026-06-21 07:42'
labels: []
dependencies: []
ordinal: 106000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
flash_esp.py --board esp32 (manual -f/-s mode) flashes the CURRENT partition table (esp32-partitions.bin, spiffs @ 0x270000 for the 2.375MB-app layout post-TASK-867) at 0x8000, but writes the littlefs.bin at a HARDCODED stale offset 0x1F0000 (the OLD 1.875MB-app layout). Result: firmware looks for LittleFS at 0x270000, finds nothing -> 'Mounting LittleFS failed! Error: -1' -> reboot loop. Observed 2026-06-20 on OTGW32 (alpha.226): the USB flash bricked the device into a boot loop; recovered by flashing the merged-full.bin at 0x0 (self-consistent offsets). Also: flash_esp.py ran erase-flash even without -e for the esp32 path, wiping NVS WiFi creds. FIX: derive the littlefs (spiffs) flash offset from the partition table / partitions_otgw_esp32.csv (0x270000), not a hardcoded constant; make erase opt-in (-e) for esp32 too; consider preferring merged-full for full installs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 littlefs flash offset is read from the active partition table / CSV (0x270000 for esp32 2.375MB layout), never hardcoded
- [x] #2 erase is opt-in (-e) on the esp32 path too; default flash preserves NVS (WiFi creds)
- [ ] #3 a manual -f/-s flash on a 2.375MB-app device boots cleanly (LittleFS mounts, no reboot loop)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1/#2 implemented in flash_esp.py (offset read from partitions_otgw_esp32.csv; erase opt-in preserving NVS). py_compile clean, parser returns 0x270000. AC#3 (flash+boot on real 2.375MB-app device) needs hardware validation.
<!-- SECTION:NOTES:END -->
