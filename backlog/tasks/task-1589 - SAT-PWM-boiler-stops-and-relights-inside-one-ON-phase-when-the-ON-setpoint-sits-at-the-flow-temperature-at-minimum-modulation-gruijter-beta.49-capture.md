---
id: TASK-1589
title: >-
  SAT PWM: boiler stops and relights inside one ON phase when the ON setpoint
  sits at the flow temperature at minimum modulation (gruijter beta.49 capture)
status: To Do
assignee: []
created_date: '2026-11-03T13:40:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 734000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the peer analysis of gruijter's capture 20261103-101925 (beta.49, Classic PIC, Intergas; Discord msg 1567167044309352500, otframes.log in the zip): PWM ON phase 13:13:17-13:17:47 had two flames. The boiler stopped at 13:15:28 at minimum modulation (MM=0) with flow 36.3 C vs CS 35.3 C; SAT then set CS=44.3 (board return 26.3 + 18, the restart rule of entries 45/149) and the boiler relit at 13:16:35 (67 s off). Question: is an ON-phase setpoint at or below the flow at MM=0 expected (thermo-nova pwm.py ON-phase setpoint, cite file:line), and should the firmware differ. First: trace thermo-nova's ON-phase setpoint and the entries 45/149 rule; a second reader checks both citations; only then a question to Sergeant D.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 thermo-nova ON-phase setpoint traced with file:line and compared with the firmware on the capture's frames (timeline)
- [ ] #2 Second reader confirmed the difference (or refuted it); if real, Q queued for Sergeant D with A/B/C and advice
<!-- AC:END -->
