---
id: TASK-1385
title: >-
  SAT PWM: reseed the effective boiler temperature in duty range 5 as
  thermo-nova
status: To Do
assignee: []
created_date: '2026-10-25T10:40:00Z'
updated_date: '2026-10-29T19:14:00Z'
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

TASK-1412.1 records review (R20): 2026-10-29. revalidation against TASK-1401 (TASK-1412.1 R20; read-only audit, no test run). The gap in this description is already closed in code. Since 5b2afb580 (TASK-1401/1369, alpha.514), the duty range-5 branch starts its ON phase through satPwmBeginOn() (SATcontrol.ino:1087-1103, call at :1097). With the flame lit, that call reseeds BT from the live flow (:887); with the flame off, the ignition sync reseeds it (:1068-1076). This matches thermo-nova's ON start, pwm.py:164-167. Do not implement a second reseed. Still open: (1) no host test asserts BT after a range-5 start; the start_limit range5/range5burn/entry5 cases check the count and the anchor only. (2) Thermo-nova ends a range-5 phase at 2 x upper and restarts it, reseeding and counting again (pwm.py:205-215, :155-167, :296-301). The firmware never ends a range-5 phase. That is the open item of entry 80 ('Not asked') and is not decided here; derived, not run: the 300 s saturation exit normally comes first (heating_control.py:357-377). (3) No Sergeant D entry names the range-5 reseed. Entries 1 and 91 support the thermo-nova behaviour, so AC#2 stays unchecked.
<!-- SECTION:NOTES:END -->
