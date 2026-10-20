---
id: TASK-1268
title: >-
  SAT adds the +18 flame-off offset in every mode, so PWM-OFF sends CS=28 with
  CH=1 and the boiler short-cycles
status: To Do
assignee: []
created_date: '2026-10-20 18:15'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 398000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SATcontrol.ino ~4679-4687 adds fFlameOffOffset (18) to finalSetpoint whenever the flame is off, in any mode. thermo-nova (heating_control.py:385-408) uses return+offset only as a REPLACEMENT in the PWM-ON flame-off phase, and only when overshoot_protection is on (default off). Effects: PWM-OFF minimum 10 becomes 28 and wantHeating (finalSetpoint > SAT_MIN_SETPOINT) sends CH=1, so the boiler relights at flow 28; graph CSVs show chSet only at 10 / 28.0 / 45 and ~20 CH bursts per hour against cycles_per_hour=3; continuous startup jumps to 40.3+18 clamped to 45 (telnet 11:39:49 'pid=40.3 final=45.0'); the PWM wait step gets the offset twice. Found independently by the field-log analysis and the thermo-nova comparison.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PWM-OFF sends CS=10 and CH=0, continuous mode sends the PID setpoint without +18, the PWM flame-off hold still uses return+offset (host test or simulator, red on old code)
- [ ] #2 Bench or simulator run shows no relight at CS=28 during PWM OFF
<!-- AC:END -->
