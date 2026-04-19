---
id: TASK-304
title: '[PERF-M2] Gzip serve large frontend assets (index.html, index.js)'
status: To Do
assignee: []
created_date: '2026-04-18 19:20'
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
