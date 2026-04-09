---
id: TASK-214
title: 'SAT fix: missing 65C global setpoint cap and PWM disable at maximum setpoint'
status: To Do
assignee: []
created_date: '2026-04-09 05:25'
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
- [ ] #1 A global maximum setpoint cap of 65C is enforced in satControlLoop() before applying control mode
- [ ] #2 When PID output >= system maximum setpoint, PWM is disabled and continuous mode is used
- [ ] #3 The 65C cap is configurable (maps to settings.sat.fMaxSetpoint) with system defaults
- [ ] #4 Pressure monitoring defaults (min=0.8, max=2.5, drop=0.3) verified in OTGW-firmware.h
<!-- AC:END -->
