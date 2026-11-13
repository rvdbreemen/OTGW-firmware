---
id: TASK-1912
title: >-
  REST Dallas label handlers (getDallasLabels, updateAllDallasLabels) run
  LittleFS on the async_tcp task: defer to the loop task
status: To Do
assignee: []
created_date: '2026-11-13T19:17:00Z'
labels:
  - bug
  - stability
dependencies: []
priority: medium
ordinal: 978000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1899 (narrow scope, coordinator 2026-11-13). getDallasLabels() (GET, LittleFS.exists plus the file read) and updateAllDallasLabels() (LittleFS.open for write of /dallas_labels.ini) in restAPI.ino run on the async_tcp task. User-triggered and rare, with a write, so the file work has to move to the loop task rather than be cached. The TASK-1899 gate tests/test_rest_no_littlefs_on_async_tcp.py allow-lists both with exactly 1 call each; this task removes those entries.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No LittleFS call in the Dallas label handlers runs on async_tcp; /dallas_labels.ini is read and written on the loop task
- [ ] #2 The TASK-1899 gate allow-list entries for getDallasLabels and updateAllDallasLabels are removed and the gate stays green; red on the old code
- [ ] #3 Host or source test on the real handlers; bench by the bench-tester: read and save sensor labels from the web UI, no watchdog reset
<!-- AC:END -->
