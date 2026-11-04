---
id: TASK-1013
title: >-
  tooling: flash_otgw.bat dual-mode - factory (merged, full erase) + update
  (separate app+fs, keep WiFi creds)
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 12:17'
updated_date: '2026-07-05 17:01'
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
- [x] #1 Factory mode: merged-full + write_flash -e 0x0 (existing behavior, wipes WiFi)
- [x] #2 Update mode (--update): separate app @0x10000 + fs @0x270000, no erase, WiFi creds survive
- [x] #3 Auto-detect: merged-full for factory; .ino.bin + .littlefs.bin (release) or .pio/build/*/firmware.bin+littlefs.bin for update; explicit --app/--fs/--bin overrides
- [x] #4 Help text + ready-to-flash summary state which mode and whether WiFi creds are kept or wiped
- [x] #5 Verified on device: update mode keeps WiFi creds (board rejoins LAN without re-provisioning)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented + committed 401674417 (pushed). Two modes: factory (merged-full + write_flash -e 0x0, wipes WiFi) and --update (app @0x10000 + fs @0x270000, no erase, NVS/WiFi preserved). Auto-detect release .ino.bin/.littlefs.bin + .pio/build fallback; explicit --app/--fs/--bin. VALIDATED ON HARDWARE (COM8 classic-S3): --update wrote app+fs, both hashes verified, NO erase step -> NVS untouched by construction. Fixed 2 batch traps (paren-in-echo double-branch, stale %VAR% in block) + CRLF. AC5 'rejoins LAN' needs the board provisioned to a reachable network first (currently has stale creds -> AP-fallback); NVS-preservation itself is proven.

AC5 CLOSED on hardware (2026-07-05): with the board provisioned + on the LAN at 192.168.1.219, ran flash_otgw.bat --update (app+fs, no erase). After hard-reset the board rejoined the SAME network at the SAME IP (.219, HTTP 200 within 4s) with NO re-provisioning — WiFi creds survived the no-erase flash. Both modes now fully HW-validated: factory (chip erase + merged-full, wipes creds -> open config portal) and update (app+fs, keeps creds -> auto-rejoin). Port 80 came up immediately on the update-reboot, confirming TASK-961 is specific to the provisioning boot.
<!-- SECTION:NOTES:END -->
