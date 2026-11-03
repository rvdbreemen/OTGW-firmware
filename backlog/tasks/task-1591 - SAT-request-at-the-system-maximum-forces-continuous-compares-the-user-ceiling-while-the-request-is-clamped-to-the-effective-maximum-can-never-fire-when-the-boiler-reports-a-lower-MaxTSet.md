---
id: TASK-1591
title: >-
  SAT: 'request at the system maximum forces continuous' compares the user
  ceiling while the request is clamped to the effective maximum (can never fire
  when the boiler reports a lower MaxTSet)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T14:02:00Z'
updated_date: '2026-11-03T17:46:00Z'
labels:
  - sat
dependencies: []
priority: low
ordinal: 736000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-1412.7 (2026-11-03): the rule that ends PWM when the request reaches the system maximum compares against the user ceiling sysMax, but the request is clamped to the effective maximum (also bounded by the boiler's reported MaxTSet, MsgID 49/57). They differ only when the boiler reports a lower MaxTSet (e.g. a rejected SH=, TASK-1412.13); then the rule cannot fire. thermo-nova uses one value (heating_control.py:203-210). No recorded decision covers it. First: trace both with file:line, second reader, then a question to Sergeant D if real.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Both sides traced with citations and a second reader confirmed or refuted the difference
- [ ] #2 If real: decision recorded (Sergeant D) and implemented with a host test red on the old code
<!-- AC:END -->
