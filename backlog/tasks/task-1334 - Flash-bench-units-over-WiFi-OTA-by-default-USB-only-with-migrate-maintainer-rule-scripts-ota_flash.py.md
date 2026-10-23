---
id: TASK-1334
title: >-
  Flash bench units over WiFi OTA by default; USB only with --migrate
  (maintainer rule) + scripts/ota_flash.py
status: Done
assignee: []
created_date: '2026-10-23T22:15:00Z'
updated_date: '2026-10-23T23:14:00Z'
labels:
  - tooling
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 457000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24: flash via OTA from now on; USB only when OTA is impossible, and then always with the migrate option. The web filesystem OTA keeps settings.ini and other kept files (OTGW-ModUpdateServer-esp32.h _keepFiles/_restoreKeptFiles, writeSettings after remount); a raw USB fs write wipes them. Adds scripts/ota_flash.py (fs via POST /update?cmd=100&size=n, app via POST /safeboot then the safeboot /update upload, then settings diff) and updates CLAUDE.md Test automation + the implement-next-task skill.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CLAUDE.md and the implement-next-task skill state OTA first, USB only with --migrate --host
- [x] #2 scripts/ota_flash.py flashes app + filesystem on the bench unit over WiFi and reports the running version and an unchanged settings set (run output in the notes)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: CLAUDE.md Test automation + implement-next-task skill step 5 now say OTA first, USB only with flash_otgw --migrate --host (never --update --fs). scripts/ota_flash.py runs on the OTGW32 bench: filesystem OTA (POST /update?cmd=100&size=n) moved the unit from fs f5c09a8 to caa79e8, later c4447d1, with every setting kept (177 keys; the only diffs were the lat/lon rounding of TASK-1336 in writeSettings and satbleriskack, which Robert set himself in the UI); app OTA via POST /safeboot + the safeboot /update upload: first urllib attempt got the connection closed, so the app upload uses curl -F with an absolute path and up to 3 tries; final run 'try 1: Update OK ... HTTP 200', 'running: 2.0.0-alpha.479+c4447d1', 'settings: 177 keys, changed: none'.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Bench units are flashed over WiFi OTA by default; USB only when OTA cannot work, and then with flash_otgw --migrate --host (CLAUDE.md, implement-next-task skill, memory). New scripts/ota_flash.py does filesystem OTA through the main firmware and app OTA through the safeboot updater, then reports the running version and any changed settings. Evidence: AC1 docs diff; AC2 bench runs (app + fs over WiFi, 177 settings unchanged).
<!-- SECTION:FINAL_SUMMARY:END -->
