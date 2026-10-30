---
id: TASK-1495
title: >-
  Filesystem OTA: an interrupted upload loses all settings at the next reboot
  (abort path skips the settings restore)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T00:01:00Z'
updated_date: '2026-10-30T06:49:00Z'
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
- [x] #3 Bench old-vs-fix: kill the client at ~90% of an FS upload, then GET /settings.ini before any reboot and after a reboot. Old serves the shipped defaults, the fix serves the user's settings
- [x] #4 The firmware reports an interrupted or short filesystem update: marker /.fs_update_incomplete (not in KEEP_LIST, so a complete image removes it), a telnet log line, and device/info fsupdateincomplete; the end handler treats written != size as an error instead of trusting version.hash (coordinator 75, d5 bench: early blocks incl. version.hash are new, later blocks of large files are not)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Format on a partial filesystem write (Robert 2026-10-30). In OTGW-ModUpdateServer-esp32.h: one helper used by the abort path and by the end path's short-upload branch: when _uploadWrittenBytes > 0 and the image is incomplete, LittleFS.format() (the partition is unmounted since the start's LittleFS.end()), then LittleFS.begin(), then _restoreRuntimeFiles() (settings from RAM, kept files from the heap, OT profiles, SAT state) and _markFsUpdateIncomplete(). Abort with 0 bytes written: remount only (old FS intact). Equivalent to what a complete FS OTA keeps (keep list + RAM). Web assets are gone; the firmware fallback page offers the Flash Utility. Watchdogs: TWDT 30 s (OTGW-Core.ino:1513), format runs on async_tcp; erase time measured on the bench. Verification: source-text test red on 8ded885eb; full tests folder; build; bench old (alpha.548 = settings lost) vs fix (settings.ini with user values before and after reboot, fsupdateincomplete true, format duration from telnet, no TWDT reset).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (2026-10-30, wt-1495 on origin/dev f406d6890): tests/test_fs_update_keep.py on the fix: 33 run, 1 failure = test_savers_write_unconditionally (satSavePidState guard), which also fails on HEAD and comes from f07db1ec5 (TASK-1482/1390), not this task. Against HEAD's sources (OTGW_SRC_ROOT=git archive of HEAD): 10 failures + 1 error; the 10 new TASK-1495 tests are all red there and all green on the fix. evaluate.py --quick: 79 passed, 0 warn, 0 fail (it was red until the settings-JSON gate accepted the bool signature and the tail print stayed a plain file.print). tests/test_evaluate.py 80 OK. tests/test_settings_float_roundtrip.py fails on HEAD and on the fix with the same error (SATtargettemp: no clamp range), so it is unrelated. NOT verified: a compile (no build in this lane; the coordinator builds the batch), file.position()/Print return values on ESP32 LittleFS at runtime, and the abort path on hardware (AC#3, bench .63 after the build, coordinated with d5). Source tests pin structure and order; they do not run the C++.

Review fix (same day): _restoreRuntimeFiles() cleared dirty also when writeSettings failed, which on the abort path (half-written partition) would bring the defaults back at the reboot. Now: bool saved = writeSettings(false); settingsMarkClean(); if (!saved) settingsMarkDirty(); (new settingsMarkDirty() in settingStuff.ino). Tests: 34 run on the fix, only the pre-existing satSavePidState failure; on 389b9ce7f the 2 new/changed tests are red. evaluate --quick 79/0/0. All writeSettings declarations checked (OTGW-firmware.h, update server extern): bool.

Bench 2026-10-30 (coordinator otgw-firmware-b8, .63). OLD alpha.535: fs_abort.py (scratchpad; same multipart as scripts/ota_flash.py, 90% then TCP RST): before reboot /settings.ini gone (FSexplorer fallback page), after reboot HTTP 500 + defaults (mqttbroker homeassistant.local). alpha.547 (commit 810e9c2b6): same result; telnet shows '[OTA] Abort: after 707788 bytes' and nothing else. Root cause 1: _handleUploadStart keeps LittleFSmounted true across LittleFS.end() (dhwWaterMeter.ino:228, isFlashing()), so the abort gate '&& !LittleFSmounted' never held; the source-text test could not see it. Fixed in alpha.548 (8ded885eb, test red on 6240b05, green on fix). alpha.548 bench: telnet now shows remount OK, Restoring settings, kept files restored, incomplete marker; fsupdateincomplete=true. Root cause 2: '[Settings] Error: /settings.ini short write (0 of 1512 bytes)' on every retry: the half-written image leaves a LittleFS that accepts no writes, so after reboot defaults again (HTTP 500). Robert 2026-10-30: format LittleFS on abort (option A over an NVS backup), and publish alpha.548 meanwhile (done, msg 1565603481086066738).

Bench 2026-10-30 alpha.549+3981f79 (format on partial write, commit 3981f7983 rebased to cfd0ecad3, src identical). Settings restored from backup, baseline saved. fs_abort.py 90% + RST, telnet: '[OTA] Abort: after 707788 bytes' / 'Filesystem image incomplete: formatting LittleFS' / 'LittleFS format OK (53 ms)' / 'Abort: LittleFS remount OK' / 'Restoring settings to filesystem' / '[Settings] State: File write complete' (no short write) / reboot_count + reboot_log restored / marker. Before reboot: /settings.ini path serves the 450 B FSexplorer fallback page (web assets gone after the format; the file itself is not served statically then), fsupdateincomplete=true. After POST /ReBoot: /api/v2/settings deep-equal to the baseline (0 differing keys; mqttbroker 192.168.88.32, hostname OTGW, mqttuniqueid otgw-AC276ECE45D8). Recovery: complete FS OTA (ota_flash.py --no-fs... --fs) -> v2.html 200 53759 B, v2.js complete 256638 B, fsupdateincomplete=false, 174 settings keys unchanged. OLD for comparison: alpha.535 and alpha.548 both came back with defaults (HTTP 500 on settings.ini). Format time 53 ms, far inside the 30 s TWDT.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
An interrupted or short filesystem OTA no longer loses the settings. Two root causes, both found on the bench: (1) the abort path's remount/restore was gated on LittleFSmounted, which the upload start keeps true across LittleFS.end(), so it never ran (fixed alpha.548, 8ded885eb); (2) the half-written image leaves a LittleFS that accepts no writes ('short write 0 of 1512 bytes'), so the partition is now formatted before the restore (alpha.549, cfd0ecad3, Robert's choice over an NVS backup). AC#1/AC#2: tests/test_fs_update_keep.py 37 OK (new tests red on 8ded885eb/6240b05, green on fix); full tests folder 62/63 (only the pre-existing TASK-1499). AC#3 bench old-vs-fix on .63, upload cut at 90% by fs_abort.py: alpha.535 and alpha.548 came back with defaults (settings.ini HTTP 500); alpha.549: telnet 'LittleFS format OK (53 ms)', restore without write errors, after reboot /api/v2/settings deep-equal to the baseline, fsupdateincomplete=true; a complete FS OTA then restored v2.html/v2.js with 174 keys unchanged. Build esp32-combo alpha.549+cfd0eca, evaluate 79/0/0. Not verified: OT-Direct board (no unit on the bench); a real network drop instead of a TCP RST.
<!-- SECTION:FINAL_SUMMARY:END -->
