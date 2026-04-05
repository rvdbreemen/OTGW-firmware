---
id: TASK-27
title: Automatic PID gains self-tuning
status: To Do
assignee: []
created_date: '2026-04-05 20:46'
updated_date: '2026-04-05 21:07'
labels:
  - sat
  - feature
  - pid
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Self-tuning PID coefficients based on system response. SAT Python (automatic_gains) adjusts P/I/D gains automatically by observing how the system responds to setpoint changes, reducing the need for manual tuning.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_AUTOMATIC_GAINS)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Observe system response to setpoint changes (overshoot, settling time)
- [ ] #2 Auto-adjust P/I/D gains based on observed response
- [ ] #3 Configurable enable/disable for auto-tuning
- [ ] #4 MQTT/REST/WebUI: current auto-tuned gain values
- [ ] #5 Persist tuned gains to LittleFS
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT thermo-nova auto PID gains reference (pid.py:62-93):
Automatic gain calculation formulas:
- Kp = (coefficient * curve_value) / 4
  - Use divisor 3 instead of 4 for underfloor heating (slower response)
- Ki = Kp / 8400
  - 8400 = typical integral time constant in seconds (~2.3 hours)
- Kd = 0.07 * 8400 * Kp
  - 0.07 = derivative damping factor
  - Results in Kd proportional to Kp with time-constant scaling
- curve_value comes from heating curve calculation
- coefficient is the user-configured heating curve coefficient
<!-- SECTION:NOTES:END -->
