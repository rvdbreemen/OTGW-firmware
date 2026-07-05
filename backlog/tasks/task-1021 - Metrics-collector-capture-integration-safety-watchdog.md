---
id: TASK-1021
title: Metrics collector + capture integration + safety watchdog
status: To Do
assignee: []
created_date: '2026-07-05 22:21'
labels: []
dependencies: []
ordinal: 232000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Per-arm: poll /api/v2/stats + telnet 'z' reset + crashlog watch via capture-mqtt-debug.bat (one transcript per arm). Watchdog: telnet unreachable / crashlog / heap below floor -> abort arm, log incident, esptool recovery ready.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 One merged transcript per arm with stats+heap+503+crashlog
- [ ] #2 Watchdog aborts an arm on incident and records it
<!-- AC:END -->
