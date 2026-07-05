---
id: TASK-959
title: >-
  Guard firmware OTA on single-app-slot OTGW32 — it corrupts the running
  partition (app OTA is USB-only on 4MB)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-30 18:37'
updated_date: '2026-07-01 21:22'
labels: []
dependencies: []
ordinal: 171000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Empirically proven on bench OTGW32 @.39 (alpha.297/298): FILESYSTEM OTA (POST /update?cmd=0... cmd=100) works over WiFi (version.hash flips), but FIRMWARE/app OTA (cmd=0) does NOT install and is UNSAFE. The 4MB partition table (partitions_otgw_esp32.csv) has a SINGLE app slot (app0/ota_0, 0x260000) and no ota_1. Update.begin(U_FLASH) targets esp_ota_get_next_update_partition() which returns ota_0 = the RUNNING partition (freeSketch=2490368=0x260000=ota_0 size confirms). Update.write() then overwrites the executing app's early .text at 0x10000; after ~3s/a few KB the upload task faults, the connection drops (HTTP=100, no End: success), fwversion never changes, and the partially-overwritten running slot can fail to boot on next reset (no fallback slot) -> USB recovery required. Two attempts left .39 needing a USB re-flash. The Flash Utility currently OFFERS firmware flash on this board, so a user can brick their device. Dual-slot (ota_0+ota_1) is the proper fix but does NOT fit on 4MB (2x1.876MB app + 1MB assets > 4MB); needs an 8MB S3-WROOM-1-N8 module (pin-compatible BOM swap) on a future PCB rev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 _handleUploadStart rejects cmd=0 (firmware target) with a clear error BEFORE any Update.begin/write when the partition table has no spare app slot (single ota_X == running), so nothing is ever written to the running partition
- [ ] #2 The Flash Utility UI hides/disables the firmware-flash option (or shows 'app update is USB-only on this board') when single-slot; filesystem flash stays available
- [ ] #3 Doc note: app updates = USB (flash_otgw.bat), filesystem updates = OTA; dual-slot firmware OTA needs an 8MB module
- [ ] #4 ADR drafted: single-slot app-OTA policy + the 8MB-module path to dual-slot OTA with rollback
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 implemented (alpha.311, OTGW-ModUpdateServer-esp32.h): firmware/app OTA now rejected BEFORE any Update.begin when single-app-slot (esp_ota_get_next_update_partition(nullptr)==esp_ota_get_running_partition() or null). Sets _updaterError, returns early; the POST completion handler returns 'Flash error: app update is USB-only on this board (single app slot); use flash_otgw.bat over USB' and clears bESPactive — board stays alive, no write reaches flash. Empirically the prior code bricked (720KB written to running ota_0). Also removed the merged-4MB OTA branch entirely (merged=USB-only). Dynamic: a future 8MB dual-slot table auto-re-enables app OTA. Adversarial review (5 agents): 0 confirmed defects on the reject/detect/merged-removal logic. Remaining: AC#2 (UI hide firmware-flash on single-slot), AC#3 (doc note), AC#4 (ADR).

AC#1 verified ON-DEVICE (combo .64, alpha.311): firmware OTA POST /update?cmd=0 -> HTTP 200 'Flash error: app update is USB-only on this board (single app slot); use flash_otgw.bat over USB'; 3x /version.hash probes over ~9s all returned, board stayed alive, app slot untouched (no brick, no recovery needed). Contrast: pre-fix alpha.310 wrote 720KB into running ota_0 and required USB recovery. Remaining: AC#2 (UI hide firmware-flash on single-slot), AC#3 (doc note app=USB/fs=OTA/8MB-dual-slot), AC#4 (single-slot-OTA-policy ADR).
<!-- SECTION:NOTES:END -->
