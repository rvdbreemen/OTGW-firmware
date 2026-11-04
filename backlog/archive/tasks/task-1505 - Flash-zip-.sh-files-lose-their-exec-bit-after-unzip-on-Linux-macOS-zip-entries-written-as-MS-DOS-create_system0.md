---
id: TASK-1505
title: >-
  Flash zip: .sh files lose their exec bit after unzip on Linux/macOS (zip
  entries written as MS-DOS, create_system=0)
status: Done
assignee: []
created_date: '2026-10-30T06:28:00Z'
updated_date: '2026-10-30T08:55:00Z'
labels:
  - tooling build release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 661000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-30 (coordinator, beta blocker for TASK-1504): in OTGW-firmware-esp32-combo-2.0.0-alpha.549+3981f79-flash.zip the entries capture-mqtt-debug.sh, capture-mqtt-debug-macos.sh and flash_otgw.sh carry mode 0755 in external_attr but create_system=0 (MS-DOS), so Info-ZIP unzip 6.00 in WSL Ubuntu ignores the mode: after unzip they are -rw-r--r-- and './capture-mqtt-debug.sh' fails with Permission denied. Also every entry is dated 1980-01-01. The zip is written by build.py on Windows.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Zip entries for *.sh (and any other executable) are written with create_system=3 and mode 0755; after unzip on Linux (WSL, Info-ZIP) ls -l shows -rwxr-xr-x (red on alpha.549's zip, green on the fix)
- [x] #2 A test in tests/ reads a built zip's infolist and fails when a .sh entry is not create_system=3 with 0o755
- [x] #3 macOS Archive Utility / unzip behaviour stated (verified by the macOS tester for TASK-1504, or 'not verified')
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 (coordinator otgw-firmware-b8). Cause: create_distribution_zip() (build.py ~:1251) set external_attr 0o100755<<16 for .sh but left create_system=0 (MS-DOS) on Windows, so Info-ZIP ignores the mode; other entries MS-DOS attrs, dated 1980. Fix: _zip_add() helper, create_system=3, explicit Unix mode (0755 .sh, 0644 rest; otgw_capture.py 0644 because the launchers start it via python), source mtime clamped >= 1980; no date pin existed (zip no longer byte-reproducible). tests/test_flash_zip_modes.py calls the real create_distribution_zip(): 4/5 red on the old build.py (create_system 0 != 3, 1980 dates, 0o666 vs 0o644), 5/5 green. WSL Info-ZIP unzip of a zip made by the real function: capture-mqtt-debug.sh, capture-mqtt-debug-macos.sh, flash_otgw.sh -rwxr-xr-x, the rest -rw-r--r--, real dates. Full tests folder 66 files, only the known TASK-1499 failure. macOS Archive Utility: not verified (no Mac here; the TASK-1504 macOS tester covers it). Real release-size zip check follows after the build.

Real release zip OTGW-firmware-esp32-combo-2.0.0-alpha.549+6e02216-flash.zip (pushed 6e02216bf, posted msg 1565649919828824198), WSL Ubuntu Info-ZIP: capture-mqtt-debug.sh/-macos.sh/flash_otgw.sh -rwxr-xr-x, otgw_capture.py and vendor wheel -rw-r--r--; launcher with no python on PATH and a fresh XDG_DATA_HOME: download, SHA-256 verified, unpack, tool --help, rc=0, runtime Python 3.12.16. evaluate --quick 79/0/0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
create_distribution_zip() writes every entry with create_system=3, an explicit Unix mode (0755 .sh, 0644 rest) and the source mtime. Evidence: tests/test_flash_zip_modes.py 4/5 red on the old build.py, 5/5 green; real alpha.549+6e02216 zip unzipped in WSL: the three .sh files -rwxr-xr-x and the launcher runs. Not verified: macOS Archive Utility (TASK-1504 macOS tester).
<!-- SECTION:FINAL_SUMMARY:END -->
