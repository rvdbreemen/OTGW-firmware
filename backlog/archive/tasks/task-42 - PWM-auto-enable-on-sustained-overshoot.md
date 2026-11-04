---
id: TASK-42
title: PWM auto-enable on sustained overshoot
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:05'
updated_date: '2026-04-05 22:53'
labels:
  - sat
  - feature
dependencies:
  - TASK-14
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/heating_control.py:262-305
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Automatically switch from continuous modulation to PWM when flow temperature >= requested+0.5C for 300s continuously. Guards: skip when DHW is active, heater is off, or setpoint is very low. Prevents sustained overshoot that wastes energy.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Auto-switch from continuous to PWM when flow >= requested+0.5C for 300s
- [x] #2 Guard: no switch when DHW is active
- [x] #3 Guard: no switch when heater is off
- [ ] #4 Guard: no switch when setpoint is below minimum threshold
- [ ] #5 Event/log emitted when auto-switch occurs
- [ ] #6 MQTT sensor reports current mode (PWM vs continuous)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Auto-switch continuous->PWM when boiler temp >= setpoint+0.5C sustained for 300s. Guards for heat pump (disabled) and PWM auto-switch setting.
<!-- SECTION:FINAL_SUMMARY:END -->
