---
id: TASK-1495
title: >-
  Filesystem OTA: an interrupted upload loses all settings at the next reboot
  (abort path skips the settings restore)
status: To Do
assignee: []
created_date: '2026-10-30T00:01:00Z'
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
- [ ] #1 _handleUploadAbort restores the runtime files (settings from RAM + kept files) after a successful remount, through one shared helper with _handleUploadEnd; host test red on HEAD
- [ ] #2 flushSettings keeps the dirty flag when writeSettings fails, and readSettings marks an empty or unreadable settings.ini dirty so the next flush rewrites it; host tests
- [ ] #3 Bench old-vs-fix: kill the client at ~90% of an FS upload, then GET /settings.ini before any reboot and after a reboot. Old serves the shipped defaults, the fix serves the user's settings
<!-- AC:END -->
