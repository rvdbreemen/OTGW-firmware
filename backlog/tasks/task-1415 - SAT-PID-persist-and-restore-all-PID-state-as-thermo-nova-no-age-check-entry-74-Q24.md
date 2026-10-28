---
id: TASK-1415
title: >-
  SAT PID: persist and restore all PID state as thermo-nova, no age check (entry
  74, Q24)
status: To Do
assignee: []
created_date: '2026-10-28T06:45:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 579000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 74, Q24: follow thermo-nova). Save integral, last error, raw derivative, last temperature and derivative time; restore all with no 30-min age check (pid.py:141-157). Store the derivative time as wall-clock time. Handle last error NAN from satPidCurveOnly (entry 46). Write cadence vs LittleFS wear is a maintainer call (ask Robert in the plan). Depends on TASK-1368. Update ADR-197 Decision 7.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: save then restore round-trips all five fields; a file older than 30 min is still restored
- [ ] #2 NAN last error is saved and restored without corrupting the file
- [ ] #3 Write cadence decided by Robert and recorded in the task
- [ ] #4 build + evaluate green
<!-- AC:END -->
