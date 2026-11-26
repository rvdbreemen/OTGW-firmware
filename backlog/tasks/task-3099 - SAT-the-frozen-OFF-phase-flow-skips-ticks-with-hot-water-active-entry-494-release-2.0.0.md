---
id: TASK-3099
title: >-
  SAT: the frozen OFF-phase flow skips ticks with hot water active (entry 494,
  release 2.0.0)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:39:00Z'
labels: []
dependencies: []
ordinal: 1164000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D answered Q-444b with B (entry 494, read from 'Q446b: B', our interpretation): ticks with hot water active are skipped when we pick 'the last ON value'; the flow frozen for the PWM OFF phase (entry 446) comes from the last heating tick. Extends the known limit of TASK-2993. SATcontrol.ino:1711-1712 sets _pwm_lastOnFlowTemp on every ON-phase tick. NOTE: a tick with the DHW bit set never reaches satPwmSchedule() (the control loop returns early, SATcontrol.ino ~8241), so the literal condition bDhwActive is vacuous at :1711; the exact condition is in the plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan quotes thermo-nova (7beaaf5, opened this session) and Sergeant D entries 494, 444, 446, 437, 103, 105, 106 with agree or conflict per entry
- [ ] #2 Host test on the real PWM block: a hot-water draw at the end of the ON phase gives the long pause on the old code (red) and the short pause on the fix (green), reaching the real functions
- [ ] #3 One prerelease bump per src commit; assert-head.sh before any committed message; no push before the coordinator's slot
<!-- AC:END -->
