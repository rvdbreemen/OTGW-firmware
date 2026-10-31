---
id: TASK-1531
title: >-
  SAT: treat the Hybrid appliance type as a heat pump (scope per Sergeant D
  Q-1531a)
status: To Do
assignee: []
created_date: '2026-10-31T18:51:00Z'
labels:
  - sat
  - beta.1
dependencies: []
priority: medium
ordinal: 687000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #alpha-testing 2026-10-31 15:27 UTC (msg 1566111477927116892), entry 186: 'Hybrid Appliance type should be considered as heat pump'. Today hybrid follows the gas path except satComputeMM (SATcontrol.ino:471-482): PWM period 3600/cycles per hour (SATcontrol.ino:440-446), hourly start limit, max flow defaults 62/50 instead of the heat-pump cap 40, and the PWM card shows cycles per hour (TASK-1529). thermo-nova has no hybrid type (not found in the code; only HeatingSystem.HEAT_PUMP, config_flow.py:686). Scope asked as Q-1531a: A) PWM cycle and the PWM card only, B) every heat-pump rule. Blocked until his answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer to Q-1531a recorded in sergeant-d-corrections.md
- [ ] #2 Host tests against the real SAT code: hybrid gets the agreed heat-pump behaviour (red on the old code, green on the fix)
- [ ] #3 PWM card shows the heat-pump cycle for Hybrid (browser test old vs new)
- [ ] #4 Bench run in SAT simulation with source Hybrid shows the agreed PWM period
<!-- AC:END -->
