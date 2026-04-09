---
id: TASK-211
title: >-
  SAT fix: MM=0 sent during PWM idle state instead of MM=max (modulation
  mismatch)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:25'
updated_date: '2026-04-09 05:37'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python RelativeModulationState.PWM_OFF: when PWM is enabled but status is IDLE (between ON/OFF cycles), Python sends MM=maximum_relative_modulation (e.g. 100). C++ always sends MM=0 when eControlMode==SAT_MODE_PWM regardless of whether flame is requested or not. Between PWM cycles (OFF phase), C++ incorrectly sends MM=0, which could cause the boiler to behave incorrectly when the flame is not active and the boiler is in idle. Fix: in PWM mode during the OFF phase, send MM=max rather than MM=0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 When in PWM mode and bPwmFlameRequested is false (OFF phase), MM=iMaxRelModulation is sent
- [x] #2 When in PWM mode and bPwmFlameRequested is true (ON phase), MM=0 is sent for modulation suppression
- [x] #3 Heat pump override (always MM=100) and Geminox quirk (min 10%) still apply
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed PWM OFF-phase modulation mismatch in SATcontrol.ino.

Changes:
- In the modulation compute block (~line 3102), split the SAT_MODE_PWM branch: when bPwmFlameRequested is true (ON phase), mmValue=0 (modulation suppression, unchanged); when false (OFF phase), mmValue=iMaxRelModulation (new, matches Python RelativeModulationState.PWM_OFF).
- Heat pump override (MM=100) and Geminox quirk (min 10%) are unaffected as they sit outside the changed branch.

User impact: boiler receives the correct modulation floor during PWM idle phases instead of MM=0, preventing incorrect boiler modulation behaviour between PWM cycles.

Risks/follow-ups: None — the change is a one-liner conditional that mirrors the established Python SAT behaviour.
<!-- SECTION:FINAL_SUMMARY:END -->
