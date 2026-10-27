---
id: TASK-1385
title: >-
  SAT PWM: reseed the effective boiler temperature in duty range 5 as
  thermo-nova
status: To Do
assignee: []
created_date: '2026-10-25T10:40:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.1
  - TASK-1412.3
references:
  - TASK-1412
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
