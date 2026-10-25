---
id: TASK-1385
title: >-
  SAT PWM: reseed the effective boiler temperature in duty range 5 as
  thermo-nova
status: To Do
assignee: []
created_date: '2026-10-25T10:40:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 524000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 review 2026-10-22: range 5 (continuous ON) does not reseed the effective boiler temperature (noted as accepted gap in sat-comparison.md). Needs a citation check against pwm.py and Sergeant D's OK before change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Range 5 matches pwm.py's effective-temperature handling with citation (host test)
- [ ] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->
