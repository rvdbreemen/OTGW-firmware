---
id: TASK-1294
title: >-
  LittleFS margin on 4MB boards is down to 8 KiB: gzip the staged web assets or
  move static text out of the image
status: To Do
assignee: []
created_date: '2026-10-22T07:32:00Z'
labels:
  - build
  - webui
  - littlefs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 419000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-1281 (settings-help.json, 17.8 KB = 5 LittleFS blocks) build.bat reports 'LittleFS budget OK: 664 KiB assets + 96 KiB reserve <= 768 KiB partition' (was 640 KiB). The next web asset of any size fails the ADR-190 budget gate. The firmware already serves a .gz sibling with Content-Encoding (webSendFile(path, type, gzip) in webServerCompat.h) but build.py's staging only minifies .js/.css and never gzips. Options to weigh (research first, per the open-source rule): (a) gzip v2.js/v2-bundle.css/settings-help.json in staging and serve the .gz sibling (v2.js 243 KB minified would drop to ~60 KB), checking the file-serve gate, ETag/cache headers (ADR-139) and the FSexplorer download path; (b) ship rarely used text (help JSON, otgw_simulation.log 12.7 KB) in the firmware image instead of LittleFS; (c) drop dev fixtures from the shipped image. Needs an ADR amendment to ADR-190 if (a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (ADR amendment if the staging pipeline changes) with the measured sizes per option
- [ ] #2 build.bat budget line shows at least 64 KiB headroom on esp32-combo after the change, with the before/after lines quoted
- [ ] #3 Bench: the v2 UI loads fully from the new image in Chromium with no 503/decoding errors; FSexplorer download of a gzipped asset still yields the original file
<!-- AC:END -->
