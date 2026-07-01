---
id: TASK-977
title: >-
  Fix FS-OTA over API: use full partition size + drop multipart contentLength
  fallback
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 20:48'
updated_date: '2026-07-01 21:08'
labels: []
dependencies: []
ordinal: 189000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Empirically found while debugging OTA via the API (bench combo .64): filesystem.bin OTA (POST /update?cmd=100) via a bare curl was rejected with 'filesystem image too large'. Two root causes in OTGW-ModUpdateServer-esp32.h: (1) the size guard + Update.begin used LittleFS.totalBytes() (usable bytes < partition) instead of the full spiffs partition size, and (2) _parseUploadTotalSize fell back to request->contentLength() (the whole multipart body = file + boundary overhead > partition) when the size= query param was absent. A full-partition mklittlefs image therefore tripped the guard. Fix mirrors the 1.x ESP8266 path (_FS_end - _FS_start / size-arg-only): use esp_partition_find_first(DATA,SPIFFS)->size for the FS begin+guard, and return 0 (not contentLength) when size= is absent so begin() sizes from the partition. FS OTA now works from a bare curl too, not only the browser form.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 FS-OTA begin+guard use the real spiffs partition size (esp_partition), not LittleFS.totalBytes()
- [x] #2 _parseUploadTotalSize no longer falls back to contentLength; returns 0 when size= absent
- [ ] #3 FS OTA via bare curl (no size=) succeeds; version.hash flips; verified on device
- [x] #4 Build green (esp32-combo + esp32-classic) + evaluate --quick clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1,2,4 done (alpha.311): FS begin+guard use esp_partition_find_first(DATA,SPIFFS)->size; _parseUploadTotalSize returns 0 (no contentLength fallback) when size= absent. Builds green (esp32-combo 1.92MB / esp32-classic 1.82MB / esp32 1.88MB), evaluate --quick 0 failures. AC#3 (bare-curl FS OTA verified on device) pending the classic-S3 board IP after its re-provision.
<!-- SECTION:NOTES:END -->
