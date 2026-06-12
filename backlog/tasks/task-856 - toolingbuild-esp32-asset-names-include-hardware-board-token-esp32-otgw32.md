---
id: TASK-856
title: 'tooling(build): esp32 asset names include hardware board token (esp32-otgw32)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-11 05:33'
updated_date: '2026-06-11 16:04'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Asset naming pattern: OTGW-firmware-esp32-<hardware board>-<semver+githash>-*. esp32-classic already complies; the OTGW32 target produces OTGW-firmware-esp32-2.0.0-... without a board token. Add a per-target asset slug in build.py so the esp32 target emits esp32-otgw32 asset names. PlatformIO env names and --target CLI values stay unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 esp32 target assets named OTGW-firmware-esp32-otgw32-<semver+githash>.* (ino.bin, littlefs.bin, merged, flash.zip, elf)
- [x] #2 esp32-classic and esp8266 names unchanged
- [x] #3 Rebuild produces correctly named flash zips for all three targets
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py: asset_slug() helper; esp32 target emits esp32-otgw32 asset names (OTGW-firmware-esp32-otgw32-<semver>+<githash>-*), esp32-classic and esp8266 unchanged. PlatformIO env names and --target CLI values unchanged. Verified: esp32 rebuild produced the full renamed set incl. flash zip. CLAUDE.md Build Commands documents the three targets + naming.
<!-- SECTION:FINAL_SUMMARY:END -->
