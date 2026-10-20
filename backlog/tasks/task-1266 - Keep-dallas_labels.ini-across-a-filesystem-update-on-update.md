---
id: TASK-1266
title: Keep dallas_labels.ini across a filesystem update on /update
status: Done
assignee:
  - '@claude'
created_date: '2026-10-20 02:20'
updated_date: '2026-10-20 02:43'
labels:
  - ota
  - sensors
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 396000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1264 measured that settings.ini survives a WiFi filesystem upload because the updater writes the in-RAM settings back (OTGW-ModUpdateServer-esp32.h ~441-455). dallas_labels.ini (user-entered Dallas sensor names) is lost. Maintainer decision 2026-10-20: keep it the same way (read before the filesystem write, write back after the remount); heating-curve markers stay out of scope. Respect the filesystem-upload memory budget (file is small; cap the size) and the re-entrancy rules.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench: create dallas labels, do a WiFi filesystem upload, labels identical afterwards (before/after file diff); red on the old firmware
- [x] #2 A missing or oversized dallas_labels.ini does not break the filesystem update (test)
- [x] #3 Guide and zip README no longer list Dallas labels as lost
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation: OTGW-ModUpdateServer-esp32.h copies /dallas_labels.ini (<= 2048 B; 16 sensors is ~1 KB) to a heap buffer in _handleUploadStart just before LittleFS.end(), writes it back in _handleUploadEnd right after writeSettings()/settingsMarkClean(), and frees it on every other path (end with error, abort). A missing, empty, oversized or unreadable file, or a failed malloc, only skips the restore. Zip README (build.py) and docs/guides/WIFI_FIRMWARE_UPDATE.md updated: labels kept, curve markers still not.

2026-10-20 bench OTGW32 .88.61. RED alpha.439: 60-byte dallas_labels.ini uploaded and read back identical; filesystem OTA (HTTP 200, 11.4 s) -> file 404 after reboot. GREEN alpha.441: missing-file case: fs OTA HTTP 200, settings diff 0, file stays 404. Normal case: same 60-byte file, fs OTA HTTP 200 11.8 s, file byte-identical after reboot (cmp), telnet '[OTA] Restoring dallas_labels.ini (60 bytes): OK', settings diff 0. Oversized case: 3291-byte valid JSON, fs OTA HTTP 200 11.7 s, telnet 'dallas_labels.ini too large to keep (3291 bytes)', file gone, settings diff 0. End state alpha.441, settings 0 diffs vs baseline, test file removed. Evidence: %TEMP%\bench-1265-1266\red_*.txt, normal_telnet.txt, green_dl.ini, big_telnet.txt, missing_fsota.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A filesystem update through /update now keeps dallas_labels.ini: the updater copies it (<= 2 KB) to the heap before writing the image and restores it after the remount; missing or oversized files skip the restore without affecting the update. AC1: bench old-vs-fix on one unit (lost on alpha.439, byte-identical on alpha.441, telnet restore line). AC2: missing and 3291-byte cases both completed with HTTP 200 and 0 settings diffs. AC3: zip README and docs/guides/WIFI_FIRMWARE_UPDATE.md updated (markers still not kept).
<!-- SECTION:FINAL_SUMMARY:END -->
