---
id: TASK-1331
title: >-
  SAT: port thermo-nova's on_cycle_end PWM re-enable (continuous cycle
  classified OVERSHOOT)
status: To Do
assignee: []
created_date: '2026-10-23T19:43:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 454000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1327 review: thermo-nova re-enables PWM in two ways after a disable: sustained overshoot (heating_control.py:304, mirrored by satCycleCheckAutoSwitch) and on_cycle_end, where a continuous cycle classified OVERSHOOT calls pwm.enable() (pwm.py:219-223). The second path is not ported: no firmware code switches mode from the cycle classification (satSetControlMode is only called at SATcycles.ino ~980/1001/1017). SAT control behaviour: needs the thermo-nova trace with citations and Sergeant D's OK; check docs/sat/sergeant-d-corrections.md first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 thermo-nova on_cycle_end path traced with citations and the difference with the firmware recorded
- [ ] #2 Sergeant D's decision recorded in docs/sat/sergeant-d-corrections.md; implemented with a host test (red/green) if he agrees
<!-- AC:END -->
