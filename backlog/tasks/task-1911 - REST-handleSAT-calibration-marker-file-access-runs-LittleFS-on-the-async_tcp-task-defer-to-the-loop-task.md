---
id: TASK-1911
title: >-
  REST handleSAT calibration-marker file access runs LittleFS on the async_tcp
  task: defer to the loop task
status: To Do
assignee: []
created_date: '2026-11-13T19:16:00Z'
labels:
  - bug
  - stability
dependencies: []
priority: medium
ordinal: 977000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1899 (narrow scope, coordinator 2026-11-13). handleSAT() in restAPI.ino (the TASK-586 heating-curve calibration markers, GET/POST/DELETE on the markers file) calls LittleFS.exists/open/rename/remove on the async_tcp task: 9 calls, reads and writes. Same cross-core flash risk as the TASK-1865 and TASK-1899 watchdog cases, but user-triggered and rare, and it writes, so a cache does not fit: the file work has to move to the loop task (request flag or queue, answer when done or 202 plus a status read). The TASK-1899 gate tests/test_rest_no_littlefs_on_async_tcp.py allow-lists handleSAT with exactly 9 calls; this task removes that entry.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No LittleFS call in handleSAT runs on async_tcp; the markers file is read and written on the loop task
- [ ] #2 The TASK-1899 gate allow-list entry for handleSAT is removed and the gate stays green; red on the old code
- [ ] #3 Host or source test on the real handler; bench by the bench-tester: add, list and delete markers from the web UI under a polling load, no watchdog reset
<!-- AC:END -->
