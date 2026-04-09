---
id: TASK-194
title: >-
  SAT fix: continuous mode setpoint clamping missing flame-off and
  null-boiler-temp branches
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:19'
updated_date: '2026-04-09 06:14'
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
- [x] #1 When flame is off, satApplyContinuous() returns pidOutput directly without applying boilerTemp clamp
- [x] #2 When boilerTemp is invalid (0 or out of range), satApplyContinuous() returns pidOutput directly
- [x] #3 When boilerTemp <= pidOutput (setpoint below boiler temp), clamp is not applied
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. In satApplyContinuous(): add flame-off bypass: read flame from OTcurrentSystemState.Statusflags & 0x08; if !flame, return pidOutput directly
2. Add boilerTemp validity bypass: if boilerTemp <= 0.0f or boilerTemp > 100.0f, return pidOutput
3. Add boilerTemp <= pidOutput bypass: if boilerTemp <= pidOutput, clamp is not needed, return pidOutput
4. Only then apply the existing minAllowed clamp logic
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satApplyContinuous() reworked to add three early-return bypass cases before applying the boilerTemp-offset clamp: (1) flame off -- Statusflags bit 3 check; (2) boilerTemp invalid (<=0 or >100); (3) boilerTemp <= pidOutput (setpoint is above boiler, no correction needed). The original condition 'pidOutput < minAllowed && boilerTemp > pidOutput + flowOffset' is simplified to 'pidOutput < minAllowed' since the other guard is now covered by case 3. Matches Python _compute_continuous_control_setpoint() exactly.
<!-- SECTION:FINAL_SUMMARY:END -->
