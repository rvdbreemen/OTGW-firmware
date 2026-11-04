---
id: TASK-890
title: >-
  fix(tooling): flash_esp.py writes LittleFS at stale 0x1F0000 offset -> boot
  loop on 2.375MB-app partition
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 15:40'
updated_date: '2026-06-27 13:48'
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
- [x] #3 a manual -f/-s flash on a 2.375MB-app device boots cleanly (LittleFS mounts, no reboot loop)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1/#2 implemented in flash_esp.py (offset read from partitions_otgw_esp32.csv; erase opt-in preserving NVS). py_compile clean, parser returns 0x270000. AC#3 (flash+boot on real 2.375MB-app device) needs hardware validation.

AC#3 validated on-device 2026-06-27. Flashed bare OTGW32 ESP32-S3 (COM4, MAC 10:20:BA:21:B4:F8) via manual -f/-s: python flash_esp.py --board esp32 -p COM4 -f <ino.bin> -s <littlefs.bin>. esptool wrote LittleFS at 0x00270000 (read from partition CSV, NOT stale 0x1F0000), 'Hash of data verified'. Device hard-reset and booted CLEANLY: /api/v2/health shows status UP, uptime 0:00 climbing (no reboot loop), littlefsMounted:true, WiFi reconnected with NVS creds preserved (no -e). Web UI firmware/filesystem-mismatch banner cleared (fw+fs both 5b158a1). NOTE: completing AC#3 surfaced a separate flash_esp.py portability bug: it emitted hyphenated esptool subcommands (write-flash/erase-flash) which esptool v4.8.1 rejects ('invalid choice: write-flash'); only v5+ accepts hyphens. Fixed by detecting esptool major version and emitting underscore (v4) / hyphen (v5). Without this fix AC#3 cannot run on a v4 host.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
flash_esp.py now reads the LittleFS flash offset from the active partition CSV (0x270000 for the esp32 2.375MB-app layout) instead of the stale hardcoded 0x1F0000, and erase is opt-in (-e) so a default flash preserves WiFi NVS. Validated end-to-end: manual -f/-s flash of a bare OTGW32 ESP32-S3 wrote LittleFS @0x270000 (hash verified) and the device booted cleanly (health UP, littlefsMounted:true, no reboot loop, WiFi creds preserved). Also fixed an esptool v4/v5 subcommand portability bug (hyphen vs underscore) uncovered while validating AC#3.
<!-- SECTION:FINAL_SUMMARY:END -->
