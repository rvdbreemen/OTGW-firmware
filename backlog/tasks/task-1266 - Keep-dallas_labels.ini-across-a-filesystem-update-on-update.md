---
id: TASK-1266
title: Keep dallas_labels.ini across a filesystem update on /update
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-20 02:20'
updated_date: '2026-10-20 02:26'
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
- [ ] #1 Bench: create dallas labels, do a WiFi filesystem upload, labels identical afterwards (before/after file diff); red on the old firmware
- [ ] #2 A missing or oversized dallas_labels.ini does not break the filesystem update (test)
- [ ] #3 Guide and zip README no longer list Dallas labels as lost
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation: OTGW-ModUpdateServer-esp32.h copies /dallas_labels.ini (<= 2048 B; 16 sensors is ~1 KB) to a heap buffer in _handleUploadStart just before LittleFS.end(), writes it back in _handleUploadEnd right after writeSettings()/settingsMarkClean(), and frees it on every other path (end with error, abort). A missing, empty, oversized or unreadable file, or a failed malloc, only skips the restore. Zip README (build.py) and docs/guides/WIFI_FIRMWARE_UPDATE.md updated: labels kept, curve markers still not.
<!-- SECTION:NOTES:END -->
