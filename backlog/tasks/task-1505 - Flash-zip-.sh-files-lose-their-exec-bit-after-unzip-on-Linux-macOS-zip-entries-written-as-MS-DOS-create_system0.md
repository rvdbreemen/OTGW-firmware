---
id: TASK-1505
title: >-
  Flash zip: .sh files lose their exec bit after unzip on Linux/macOS (zip
  entries written as MS-DOS, create_system=0)
status: To Do
assignee: []
created_date: '2026-10-30T06:28:00Z'
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
- [ ] #1 Zip entries for *.sh (and any other executable) are written with create_system=3 and mode 0755; after unzip on Linux (WSL, Info-ZIP) ls -l shows -rwxr-xr-x (red on alpha.549's zip, green on the fix)
- [ ] #2 A test in tests/ reads a built zip's infolist and fails when a .sh entry is not create_system=3 with 0o755
- [ ] #3 macOS Archive Utility / unzip behaviour stated (verified by the macOS tester for TASK-1504, or 'not verified')
<!-- AC:END -->
