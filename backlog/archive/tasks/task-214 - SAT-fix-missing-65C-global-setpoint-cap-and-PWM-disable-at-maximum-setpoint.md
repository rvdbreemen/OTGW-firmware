---
id: TASK-214
title: 'SAT fix: missing 65C global setpoint cap and PWM disable at maximum setpoint'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:25'
updated_date: '2026-04-09 06:15'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python has MAXIMUM_SETPOINT=65C as a global safety ceiling for ALL heating systems. C++ only has system-specific limits (radiators=62, underfloor=45, heat_pump=40) with SAT_HARD_MAX_RAD=80C as the absolute ceiling. Gap 1: C++ allows setpoints up to 80C for radiators while Python caps at 65C. Gap 2: Python disables PWM when requested_setpoint >= maximum_setpoint. C++ has no equivalent guard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A global maximum setpoint cap of 65C is enforced in satControlLoop() before applying control mode
- [x] #2 When PID output >= system maximum setpoint, PWM is disabled and continuous mode is used
- [x] #3 The 65C cap is configurable (maps to settings.sat.fMaxSetpoint) with system defaults
- [x] #4 Pressure monitoring defaults (min=0.8, max=2.5, drop=0.3) verified in OTGW-firmware.h
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add fMaxSetpoint=65.0f to SATSection struct in OTGW-firmware.h
2. In satControlLoop(), after PID output clamping, apply global max cap before satApplyPWM/satApplyContinuous
3. AC#2: if pidOutput >= system max setpoint (satGetMaxSetpoint()), switch to continuous mode (disable PWM)
4. The global cap uses settings.sat.fMaxSetpoint (default 65C) matching Python MAXIMUM_SETPOINT
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added global 65C safety ceiling matching Python MAXIMUM_SETPOINT. New field settings.sat.fMaxSetpoint (float, default 65.0) added to SATSection in OTGW-firmware.h. In satControlLoop(), after system-type limits are applied, globalMax further constrains maxSetpoint to settings.sat.fMaxSetpoint (with sanity bounds). AC#2: when pidOutput >= sysMax (system-specific maximum), switches from PWM to continuous mode, matching Python behavior where requested_setpoint >= maximum_setpoint disables PWM. AC#4 (pressure monitoring defaults) already verified correct in OTGW-firmware.h (min=0.8, max=2.5, drop=0.3).
<!-- SECTION:FINAL_SUMMARY:END -->
