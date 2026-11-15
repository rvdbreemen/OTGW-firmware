---
id: TASK-1989
title: >-
  Periodic loop stalls of about 600 ms every 10-20 min (gruijter beta.250): find
  the cause on a current build
status: To Do
assignee: []
created_date: '2026-11-15T21:23:00Z'
updated_date: '2026-11-15T21:28:00Z'
labels:
  - performance
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1017000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter beta.250 (Classic with PIC, SAT on), 25.5 h: 92 '[loop-stall]' lines, median 420 ms, max 607 ms (15:15:30), about 3-4 per hour, often at :x5:30/:x0:30; just before the 607 ms one a CS=10.0 send and a MsgID 2 Write-Ack. Far below the 30 s task watchdog, no visible effect. Not covered by the earlier weather-fetch stall fixes (TASK-1478, TASK-1604).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on a current build (bench Classic with SAT, or the replay) and name the code that runs during a 400-600 ms stall, with evidence (telnet timing or a loop profiler)
- [ ] #2 Fix it or record why it stays (bounded, harmless), with old-vs-fix numbers when fixed
<!-- AC:END -->
