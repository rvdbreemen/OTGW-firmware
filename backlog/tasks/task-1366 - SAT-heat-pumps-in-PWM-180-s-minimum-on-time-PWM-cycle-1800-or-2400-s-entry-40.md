---
id: TASK-1366
title: >-
  SAT heat pumps in PWM: 180 s minimum on time, PWM cycle 1800 or 2400 s (entry
  40)
status: To Do
assignee: []
created_date: '2026-10-24T21:42:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 501000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q8, entry 40): heat pump minimum ON time 180 s; PWM cycle length 30 min (1800 s) or 40 min (2400 s). Fixes the duty-mapper collapse (audit #9) where 1800/2400 were used as min-on.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Heat pump PWM uses min ON 180 s and a 1800/2400 s cycle; duty mapper no longer collapses (host test red/green)
<!-- AC:END -->
