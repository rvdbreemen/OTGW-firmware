---
id: TASK-1494
title: >-
  ota_flash.py: a dropped response after the filesystem write aborts the run and
  leaves app/filesystem mismatched
status: To Do
assignee: []
created_date: '2026-10-29T23:52:00Z'
labels:
  - tooling
  - ota
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 652000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-d5 on 2026-10-30 (bench .63, alpha.521 to alpha.542). POST /update?cmd=100&size=786432 wrote the filesystem (GET /version.hash returned the new c00c5f0), but the device closed the connection without a response, probably during the remount. The script raised http.client.RemoteDisconnected and aborted before the app step (safeboot), leaving app 521 + fs 542. On that mix the 521 firmware served the larger minified v2.js cut at 65536 of 256745 bytes, so the v2 UI cannot start. Evidence: d5 scratchpad bench542/ (ota_flash.log, settings and device_info before/after, served_* files).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a dropped or missing response to the filesystem POST, ota_flash.py polls /version.hash (with a timeout) and continues only when it shows the new hash; otherwise it reports a clear failure; covered by a host test with a fake server that drops the response (red on HEAD)
- [ ] #2 The app step order and the fs/app mismatch risk are documented in the script help or docs
<!-- AC:END -->
