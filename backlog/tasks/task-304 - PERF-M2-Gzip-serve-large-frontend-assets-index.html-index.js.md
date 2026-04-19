---
id: TASK-304
title: '[PERF-M2] Gzip serve large frontend assets (index.html, index.js)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-18 19:20'
updated_date: '2026-04-19 07:23'
labels:
  - performance
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
index.html +22 KB to ~36 KB, index.js +52 KB to ~260 KB; served uncompressed via streamFile. No .gz variants. 70 percent reduction possible at cold cache.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Build step gzips index.html and index.js producing .gz siblings
- [ ] #2 FSexplorer.ino prefers the .gz variant when present and sets Content-Encoding: gzip
- [ ] #3 LittleFS image shrinks measurably; page still renders on all 3 target browsers
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Deferred during batch 5 session (2026-04-19): scope is larger than the other batch-5 tasks. Proper implementation needs (a) a build.py step that gzips index.html and index.js into .gz siblings in the LittleFS data staging area, (b) FSexplorer.ino handlers that prefer the .gz sibling and set Content-Encoding: gzip, and (c) verification that index.js Save button + index.html rendering still work on Chrome/Firefox/Safari. Estimated ~1h of careful work. Leaving as To Do so whoever picks it up starts from a clean slate.
<!-- SECTION:NOTES:END -->
