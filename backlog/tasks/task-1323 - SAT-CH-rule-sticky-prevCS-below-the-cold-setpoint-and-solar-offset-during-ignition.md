---
id: TASK-1323
title: >-
  SAT CH rule: sticky prevCS below the cold setpoint and solar offset during
  ignition
status: To Do
assignee: []
created_date: '2026-10-23T15:56:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 446000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1316 review (opus), SAT behaviour, needs Sergeant D: (1) PWM mode with overshoot protection off (or pidOutput == cold): satApplyPWM returns prevCS (_sat_lastControlCS, written back every tick), e.g. a suppression CS of 24 on radiators stays; pwmDrivesSetpoint is false, so CH follows the cold rule (24 < 28.2) -> CS=24 held with CH=0 until a mode change (HEAD sent CH=1). Options: decide CH on pidOutput vs the cold setpoint on that path, or reset/floor prevCS. (2) Solar during PWM ON: hold minus fSolarSetpointOffset can clamp to CS=10 while CH=1 (SATcontrol.ino ~5000-5003): the boiler never lights and the 180 s ignition timeout fires; the CS side predates TASK-1316.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's choice for both cases recorded with citations
- [ ] #2 Implemented with a host test per case (red before, green after)
<!-- AC:END -->
