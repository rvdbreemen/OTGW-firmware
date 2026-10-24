---
id: TASK-1367
title: >-
  SAT: continuous to PWM after 60 s of sustained overshoot, timed correctly
  (entry 41)
status: To Do
assignee: []
created_date: '2026-10-24T21:42:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 502000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q9, entry 41): 60 s. Settles conflict C1. Audit #12: the sustain timers count the first tick as 30 s, so the switch fires after ~30 s; fix the timing to a real 60 s (thermo-nova heating_control.py:280-304).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The overshoot switch fires after 60 s, not ~30 s (host test red/green)
<!-- AC:END -->
