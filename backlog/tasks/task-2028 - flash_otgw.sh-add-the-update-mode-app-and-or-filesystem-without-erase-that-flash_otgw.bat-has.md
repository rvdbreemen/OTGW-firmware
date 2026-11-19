---
id: TASK-2028
title: >-
  flash_otgw.sh: add the --update mode (app and/or filesystem without erase)
  that flash_otgw.bat has
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T14:02:00Z'
updated_date: '2026-11-19T16:06:00Z'
labels:
  - tools
milestone: 2.0.0
dependencies: []
ordinal: 1040000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-2027: the recovery in manual 7.12a (reset settings.ini over USB, keep WiFi) works on Windows only; flash_otgw.sh parses --port/--bin/--board/--baud/--migrate/--host/--password/--no-backup (:145-152) and has no --update. Linux/macOS users must factory-flash and lose WiFi.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 flash_otgw.sh --update [--app] [--fs] writes at the bat offsets (app0 0x100000, spiffs 0x330000) without erase, refuses on a non-safeboot layout like the bat
- [x] #2 Host test or a real run on a unit shows WiFi kept
- [x] #3 Manual 7.12a step 2 updated to drop the Windows-only note
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 picked up by otgw-dev, parked the same night (coordinator: a 2.0.0 high SAT bug comes first). Nothing written yet; lane-2028 in wt-1995 holds only this note.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
flash_otgw.sh gets the --update mode of flash_otgw.bat: --update [--app <fw.bin>] [--fs <littlefs.bin>] (plus --factory), update_flow() after migrate_flow. Same rules as the bat: explicit --app/--fs mean exactly that image (no auto-pick of a nearby filesystem image, TASK-1160), bare --update auto-picks the release images next to the script, then build/, then .pio; the unit's partition table (read_flash 0x8000 0x1000) must name the safeboot partition, else it refuses with the --migrate hint before any write; then write_flash -z at app0 0x100000 / spiffs 0x330000, no -e, no write at NVS 0x9000. Help text and the start banner name the mode. AC1: tests/test_flash_otgw_sh_update.py runs the real flash_otgw.sh against a fake esptool at tools/esptool/esptool (no download): red on the old script (6 cases, 'Unknown argument: --update'), green 6/6 on the fix (both offsets, app only ignoring a nearby fs image, fs only, bare auto-pick, legacy layout refused before any write, no image stops). AC2 (the AC allows a host test): the same tests show the esptool write list has no erase (-e / erase_flash) and no 0x9000 (NVS, where the WiFi credentials are), so WiFi is kept by construction; not verified on a real unit (no USB run requested). AC3: manual 7.12a step 2 in EN (ch07) and NL (h07) names ./flash_otgw.sh --update --fs on Linux and macOS instead of 'no --update mode yet'; docs/MANUAL.md regenerated (scripts/build_manual.py), test_manual_generated green. Gates: 31 tests green (new file, test_flash_migrate_settings_compare, manual tests), evaluate.py --quick 89 passed 0 failed; bash -n flash_otgw.sh OK. No firmware change, no bump.
<!-- SECTION:FINAL_SUMMARY:END -->
