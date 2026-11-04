---
id: TASK-1263
title: >-
  Flash zip is a complete package: add .ino.bin, .littlefs.bin and the capture
  scripts
status: Done
assignee:
  - '@claude'
created_date: '2026-10-19 19:57'
updated_date: '2026-10-19 20:08'
labels:
  - build
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 393000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request 2026-10-19 after alpha.439: testers got only the flash zip and could not update over WiFi (/update needs the loose .ino.bin and .littlefs.bin) nor run flash_otgw --update (it looks for *.ino.bin / *.littlefs.bin next to itself). The zip must be a complete package: merged-full + migrate images (already), .ino.bin, .littlefs.bin, flash_otgw.bat/.sh (already), capture-mqtt-debug.bat (CRLF), capture-mqtt-debug.sh and capture-mqtt-debug-macos.sh (LF, executable), README EN/NL.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.bat produces a flash zip containing merged-full, both migrate images, .ino.bin, .littlefs.bin, flash_otgw.bat/.sh and the three capture scripts, each byte-identical to its build/ or scripts/ source (with the line-ending rules applied)
- [x] #2 flash_otgw --update auto-picks the .ino.bin and .littlefs.bin from an unpacked zip folder (dry check of the file selection)
- [x] #3 README.txt / README_NL.txt list the new files and no longer say the loose bins are published separately
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-19: build.bat (esp32-combo) exit 0, firmware+littlefs SUCCESS, zip 5513.7 KB with 12 entries: merged-full, migrate-0x0, migrate-rest, .ino.bin, .littlefs.bin (all byte-identical to build/), flash_otgw.bat + capture-mqtt-debug.bat (CRLF), flash_otgw.sh + capture-mqtt-debug.sh + capture-mqtt-debug-macos.sh (LF, mode 755), README EN/NL. Unpacked zip, ran flash_otgw.bat --update --port COM99: [OK] Firmware ...ino.bin -> 0x100000, [OK] Filesys ...littlefs.bin -> 0x330000, then safe abort 'Could not read the partition table' (no device on COM99). New tests/test_build.py TestDistributionZipComplete: 2 FAIL on HEAD build.py (no .ino.bin in zip; README 'release page publishes the loose'), 2 OK on the fix; test_build.py 13 OK, test_build_safeboot.py 11 OK; evaluate --quick 0 FAIL. README fs-OTA text now says the running firmware writes settings back (OTGW-ModUpdateServer-esp32.h:453) and keeps download-first as safety net; that restore is still not bench-proven.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The flash zip is now a complete package: USB images (merged-full, migrate pair), the loose .ino.bin and .littlefs.bin for WiFi /update and flash_otgw --update, flash tools, the three capture scripts and README EN/NL. AC1: real build.bat run, 12 entries byte-identical to sources with line-ending rules. AC2: unpacked zip, flash_otgw.bat --update auto-picked both images (COM99 dry run). AC3: README lists the new files; red/green unit test on the old vs new build.py. Not verified: flash_otgw.sh selection on Linux/macOS (same glob logic, not run).
<!-- SECTION:FINAL_SUMMARY:END -->
