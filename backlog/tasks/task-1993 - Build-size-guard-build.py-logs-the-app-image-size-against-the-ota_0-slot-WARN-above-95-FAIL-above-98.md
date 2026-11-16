---
id: TASK-1993
title: >-
  Build size guard: build.py logs the app image size against the ota_0 slot,
  WARN above 95 %, FAIL above 98 %
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T04:34:00Z'
updated_date: '2026-11-16T04:58:00Z'
labels:
  - build
  - flash-size
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1019000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1955 AC2: the coordinator chose option A (WARN 95 % / FAIL 98 %) under Robert's delegation on 2026-11-15. The esp32-combo app image sits at about 93-94 % of the ota_0 slot (2 293 760 B, partitions_otgw_esp32.csv, ADR-191) and grows about 5 KB per day. Today the only limit is PlatformIO's upload.maximum_size, which fails at 100 % with no warning before. The guard makes the headroom visible on every build and stops a build before the slot is nearly full.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py prints for every esp32 build: app bytes, slot bytes, percent and free bytes, with the slot size read from partitions_otgw_esp32.csv (ota_0)
- [x] #2 Above 95 % the build prints a WARN line and continues; above 98 % it fails with exit 1
- [x] #3 A host test runs the real check on fake images at 94, 96 and 99 % of the slot (pass, warn, fail) and goes red on the code from before this task
- [x] #4 A real esp32-combo build shows the new size line
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
scripts/check_app_size.py <app.bin> <slot_bytes>: prints 'app size: N B of S B (P %), free F B'; P > 95 -> WARN line, exit 0; P > 98 -> FAIL line, exit 1 (same pattern as scripts/check_rtc_layout.py, ADR-239). build.py: right after the .ino.bin is collected for a target with bootloader_offset, read ota_0 size from read_partition_table() (fallback tcfg app_size), run the script, sys.exit(1) on rc!=0. tests/test_app_size_check.py: runs the real script on fake files at 94/96/99 % (exit 0/0/1 + WARN/FAIL text) and asserts build.py calls it; red before (script and call absent). No ADR: a build check inside the existing post-build check pattern; the thresholds are TASK-1955's recorded decision. Open source: trivial glue, no search needed. No firmware change, no bump. Verify: test red->green, real build shows the line.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation: scripts/check_app_size.py (size, slot, %, free; WARN > 95 %, FAIL > 98 % exit 1, usage/IO error exit 2); build.py collect_pio_artifacts runs it right after the .ino.bin is copied, for targets with bootloader_offset (esp32 family), slot = read_partition_table()['app0'] (ota_0, 0x230000) with tcfg app_size as fallback, sys.exit(1) on rc != 0 (same pattern as check_rtc_layout.py, ADR-239). No ADR: a post-build check inside the existing pattern; thresholds are TASK-1955's recorded decision (option A). Open source: trivial glue, no search. Evidence: tests/test_app_size_check.py before the script/call existed: Ran 5, FAILED (failures=5); after: Ran 5, OK (94 % pass with numbers, 96 % WARN rc 0, 98.0 % WARN rc 0, 99 % FAIL rc 1, build.py call at source level). Real image: check_app_size.py on beta.339+92fc8fe.ino.bin -> 'app size: 2139376 B of 2293760 B (93.3 %), free 154384 B', rc 0. Other build.py tests green (test_build, _app_version_check, _config_path, _jobs, _lock, _log_tee, _safeboot, _staging, discord_post_build, flash_zip_modes, fs_image_settings_defaults, progmem_assets_deflated, submodule_check); evaluate --quick 88 pass 0 fail.

AC4: real esp32-combo build of d29995f1f (2026-11-16 05:49-05:57, build.bat, exit 0, 'Build completed successfully') prints 'app size: 2139376 B of 2293760 B (93.3 %), free 154384 B [OTGW-firmware-esp32-combo.ino.bin]'; image equal in size to beta.339+92fc8fe (tooling-only change). Second proof: 92's 06:45 release build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: build.py runs scripts/check_app_size.py on every esp32 .ino.bin with the app0 (ota_0) slot from partitions_otgw_esp32.csv; real build line shown under AC4. AC2: WARN > 95 % (exit 0), FAIL > 98 % (exit 1, build stops) - test cases 96/98 % WARN, 99 % FAIL. AC3: tests/test_app_size_check.py red 5/5 before the script and call existed, green 5/5 after; 13 other build tests green; evaluate --quick 88 pass 0 fail. AC4: build d29995f1f printed 'app size: 2139376 B of 2293760 B (93.3 %), free 154384 B', exit 0. Not verified: a real build above 95 % (only fake images); no firmware change, no bump.
<!-- SECTION:FINAL_SUMMARY:END -->
