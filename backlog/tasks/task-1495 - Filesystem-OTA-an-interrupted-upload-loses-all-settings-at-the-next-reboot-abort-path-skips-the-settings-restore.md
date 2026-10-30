---
id: TASK-1495
title: >-
  Filesystem OTA: an interrupted upload loses all settings at the next reboot
  (abort path skips the settings restore)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-30T00:01:00Z'
updated_date: '2026-10-30T00:19:00Z'
labels:
  - ota
  - settings
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 653000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench incident 2026-10-30 on .63 (read-only investigation). OTGW-ModUpdateServer-esp32.h: _handleUploadEnd remounts and calls writeSettings(false) + _restoreKeptFiles (:449-479) before the 200; _handleUploadAbort (:488-510) only ends Update, drops the kept files and remounts. It does not restore, and the sectors already written stay written (no rollback for U_SPIFFS). After a dropped connection the new image's shipped data/settings.ini (hostname OTGW2, MQTT off, homeassistant.local, port25238 off) is on flash while RAM still holds the user's settings. RAM is not dirty, so flushSettings() at the next restart is a no-op (helperStuff.ino:641, settingStuff.ino:62), and the next boot parses the shipped defaults. Fingerprint: ui_onboarded and mqttuselegacyottopics true from the readSettings sentinel branches. Field risk: any interrupted browser FS upload (WiFi glitch, closed tab, AsyncTCP drop) silently loses hostname, MQTT, SAT tuning and the BLE MAC at the next reboot. Not caused by alpha.542 (the update server is identical in 521 and 542). Related flaws: flushSettings clears dirty when writeSettings bailed (:60-66); readSettings returns early on an empty file without marking dirty (:528-532), which makes an empty settings.ini self-perpetuating, and GET /settings.ini then gives HTTP 500 'Invalid data in handler'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 _handleUploadAbort restores the runtime files (settings from RAM + kept files) after a successful remount, through one shared helper with _handleUploadEnd; host test red on HEAD
- [x] #2 flushSettings keeps the dirty flag when writeSettings fails, and readSettings marks an empty or unreadable settings.ini dirty so the next flush rewrites it; host tests
- [ ] #3 Bench old-vs-fix: kill the client at ~90% of an FS upload, then GET /settings.ini before any reboot and after a reboot. Old serves the shipped defaults, the fix serves the user's settings
- [x] #4 The firmware reports an interrupted or short filesystem update: marker /.fs_update_incomplete (not in KEEP_LIST, so a complete image removes it), a telnet log line, and device/info fsupdateincomplete; the end handler treats written != size as an error instead of trusting version.hash (coordinator 75, d5 bench: early blocks incl. version.hash are new, later blocks of large files are not)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. OTGW-ModUpdateServer-esp32.h: _restoreRuntimeFiles() = writeSettings + settingsMarkClean + _restoreKeptFiles + saveOtSupportFilesForced + satPersistAllToFs; End calls it in the remount-OK branch; Abort calls it after a successful remount, before bESPactive=false, and drops the heap copies only after it. 2. _markFsUpdateIncomplete() writes /.fs_update_incomplete on abort after >0 bytes and on a short upload at End (written != ?size); device/info fsupdateincomplete; docs/api README + openapi. 3. writeSettings returns bool: false on open failure and on a short write (tail print != 3, or the size read back != file.position()); flushSettings clears dirty only on success; readSettings marks an unopenable or empty settings.ini dirty. 4. evaluate.py settings-JSON gate accepts the bool signature. 5. Source tests in tests/test_fs_update_keep.py (the update server needs the ESP32 core, so the C++ is not run); bench old-vs-fix (AC#3) after the coordinator's build on .63.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (2026-10-30, wt-1495 on origin/dev f406d6890): tests/test_fs_update_keep.py on the fix: 33 run, 1 failure = test_savers_write_unconditionally (satSavePidState guard), which also fails on HEAD and comes from f07db1ec5 (TASK-1482/1390), not this task. Against HEAD's sources (OTGW_SRC_ROOT=git archive of HEAD): 10 failures + 1 error; the 10 new TASK-1495 tests are all red there and all green on the fix. evaluate.py --quick: 79 passed, 0 warn, 0 fail (it was red until the settings-JSON gate accepted the bool signature and the tail print stayed a plain file.print). tests/test_evaluate.py 80 OK. tests/test_settings_float_roundtrip.py fails on HEAD and on the fix with the same error (SATtargettemp: no clamp range), so it is unrelated. NOT verified: a compile (no build in this lane; the coordinator builds the batch), file.position()/Print return values on ESP32 LittleFS at runtime, and the abort path on hardware (AC#3, bench .63 after the build, coordinated with d5). Source tests pin structure and order; they do not run the C++.

Review fix (same day): _restoreRuntimeFiles() cleared dirty also when writeSettings failed, which on the abort path (half-written partition) would bring the defaults back at the reboot. Now: bool saved = writeSettings(false); settingsMarkClean(); if (!saved) settingsMarkDirty(); (new settingsMarkDirty() in settingStuff.ino). Tests: 34 run on the fix, only the pre-existing satSavePidState failure; on 389b9ce7f the 2 new/changed tests are red. evaluate --quick 79/0/0. All writeSettings declarations checked (OTGW-firmware.h, update server extern): bool.
<!-- SECTION:NOTES:END -->
