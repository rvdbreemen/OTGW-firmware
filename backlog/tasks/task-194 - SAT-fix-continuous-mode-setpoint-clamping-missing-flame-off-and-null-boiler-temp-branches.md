---
id: TASK-194
title: >-
  SAT fix: continuous mode setpoint clamping missing flame-off and
  null-boiler-temp branches
status: To Do
assignee: []
created_date: '2026-04-09 05:19'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python _compute_continuous_control_setpoint() has 3 cases that bypass the boilerTemp-offset clamp: (1) boiler_temperature is None, (2) boiler_temperature <= requested_setpoint, (3) flame is off. In all three cases Python just uses requested_setpoint directly. C++ satApplyContinuous() always applies the minAllowed=boilerTemp-flowOffset clamp regardless of flame state or sensor availability. When flame is off, the clamp incorrectly prevents setpoint from following the PID output downward.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 When flame is off, satApplyContinuous() returns pidOutput directly without applying boilerTemp clamp
- [ ] #2 When boilerTemp is invalid (0 or out of range), satApplyContinuous() returns pidOutput directly
- [ ] #3 When boilerTemp <= pidOutput (setpoint below boiler temp), clamp is not applied
<!-- AC:END -->
