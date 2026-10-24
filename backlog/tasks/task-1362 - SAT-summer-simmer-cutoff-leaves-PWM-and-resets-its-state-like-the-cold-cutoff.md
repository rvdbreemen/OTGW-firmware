---
id: TASK-1362
title: 'SAT: summer simmer cutoff leaves PWM and resets its state like the cold cutoff'
status: To Do
assignee: []
created_date: '2026-10-24T20:26:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 497000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-199 open question 2), audit #49: the firmware-only summer simmer path (SATcontrol.ino ~4930-4942) sends CS=10/CH=0 but does not call satPwmDisableOnHeaterOff() as the cold cutoff and valves-closed paths do, so one odd PWM period follows when summer mode ends. Call it there too (PWM off, state reset, MM back to max).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a summer simmer cutoff PWM is off and its state reset (host test red on HEAD, green)
<!-- AC:END -->
