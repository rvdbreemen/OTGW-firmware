---
id: TASK-1866
title: >-
  SAT curve-only start: the restored integral is shown but not applied, and the
  SATpid.ino comment says only P and D are 0
status: To Do
assignee: []
created_date: '2026-11-12T11:54:00Z'
labels:
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 942000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the second reader of Q-P1 (gruijter capture 2026-11-12, msg https://discord.com/channels/812969634638725140/1514720723980259460/1570395420667023420). SATpid.ino:191-193 on origin/dev (entry 115 exception) says that before the first valid room temperature only P and D are 0 and the restored integral is kept; the code also leaves I out of the output (satPidCurveOnly returns the curve), while state.sat.fPidI still shows the restored I in the web UI. So the UI shows an I that is not applied. The behaviour itself is under Q-P1 (Sergeant D); this task is about making comment and display match what the code does once Q-P1 is answered.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After Q-P1 is answered and recorded as an entry, the SATpid.ino comment at the curve-only exception describes exactly which terms are in the output
- [ ] #2 The web UI and REST show the I term that is actually applied during the curve-only window (or mark it as not applied), with a host test on the real code
<!-- AC:END -->
