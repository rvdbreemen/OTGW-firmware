---
id: TASK-76
title: 'WebUI: SAT Expert view (heating control specialist)'
status: To Do
assignee: []
created_date: '2026-04-06 19:15'
labels:
  - webui
  - sat-dashboard
milestone: SAT WebUI Dashboard
dependencies:
  - TASK-74
references:
  - >-
    src/OTGW-firmware/data/index.html (lines 399-455, current control/PID/PWM
    sections)
  - src/OTGW-firmware/data/sat.js (updateDashboard function)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Design and implement the "Expert" view for the SAT dashboard. This view is for users who understand heating systems and want to monitor and tune the SAT control algorithm. Shows everything the Thermostat view shows plus detailed control information.

Layout (top to bottom):
1. **Temperature cards row**: Room / Target (+/- controls) / Outside / Boiler Setpoint (same as current)
2. **Preset + Mode controls**: Preset buttons + Continuous/PWM mode toggle + Enable/Disable
3. **Heating Curve section**: Interactive chart showing outdoor temp vs setpoint curve, with coefficient display
4. **Control Status grid**: Mode, boiler status, heating system, manufacturer, coefficient, deadband, overshoot margin, modulation %, max modulation, curve recommendation
5. **PID Controller grid**: P/I/D terms, Kp/Ki/Kd gains, error, raw derivative
6. **PWM & Cycles grid**: Duty cycle, flame state, cycle count, last cycle class, max flow temp, overshoot fraction
7. **Temperature History chart**: Full chart with setpoint, flow, room, outside, PID overlay
8. **Weather section** (if enabled): Outdoor temp, humidity, wind
9. **Smart Features status**: Solar gain active, summer simmer, thermal learning coeff, comfort offset, multi-area temps

Hidden in this view: Pressure regression details, sync sensors, detailed cycle tail statistics, simulation controls (those are for Diagnostics).

Design guidelines:
- Compact grid layout to fit more data
- Color-coded values (green=normal, yellow=warning, red=alarm)
- Tooltips on labels explaining what each value means
- Collapsible sections for less-used data
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All Thermostat view elements visible plus control details
- [ ] #2 Heating curve chart with coefficient and system type display
- [ ] #3 Control Status grid: mode, boiler status, manufacturer, coefficient, deadband, overshoot, modulation
- [ ] #4 PID Controller grid: P/I/D terms, Kp/Ki/Kd, error, raw derivative
- [ ] #5 PWM and Cycles grid: duty, flame, count, last class, max flow, overshoot fraction
- [ ] #6 Full temperature history chart with multiple series
- [ ] #7 Weather section visible when enabled
- [ ] #8 Smart features status row: solar gain, summer simmer, thermal coeff, comfort offset
- [ ] #9 Curve recommendation displayed (increase/decrease/hold)
- [ ] #10 Collapsible sections for PID and PWM details
<!-- AC:END -->
