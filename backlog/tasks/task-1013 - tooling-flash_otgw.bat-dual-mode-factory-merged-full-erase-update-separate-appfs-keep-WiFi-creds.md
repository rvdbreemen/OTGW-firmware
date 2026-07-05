---
id: TASK-1013
title: >-
  tooling: flash_otgw.bat dual-mode - factory (merged, full erase) + update
  (separate app+fs, keep WiFi creds)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-05 12:17'
labels: []
dependencies: []
ordinal: 224000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make flash_otgw.bat the default flash tool with two modes: (1) factory reset = merged-full image + full chip erase (wipes WiFi creds + settings), and (2) update = separate firmware.bin + littlefs.bin written at 0x10000 / 0x270000 with NO erase, preserving NVS WiFi credentials. Auto-detect release-named bins (.ino.bin / .littlefs.bin / -merged-full.bin) and .pio/build fallbacks.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Factory mode: merged-full + write_flash -e 0x0 (existing behavior, wipes WiFi)
- [ ] #2 Update mode (--update): separate app @0x10000 + fs @0x270000, no erase, WiFi creds survive
- [ ] #3 Auto-detect: merged-full for factory; .ino.bin + .littlefs.bin (release) or .pio/build/*/firmware.bin+littlefs.bin for update; explicit --app/--fs/--bin overrides
- [ ] #4 Help text + ready-to-flash summary state which mode and whether WiFi creds are kept or wiped
- [ ] #5 Verified on device: update mode keeps WiFi creds (board rejoins LAN without re-provisioning)
<!-- AC:END -->
