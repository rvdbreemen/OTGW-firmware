---
id: TASK-42
title: PWM auto-enable on sustained overshoot
status: To Do
assignee: []
created_date: '2026-04-05 21:05'
labels:
  - sat
  - feature
dependencies: []
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
- [ ] #1 Auto-switch from continuous to PWM when flow >= requested+0.5C for 300s
- [ ] #2 Guard: no switch when DHW is active
- [ ] #3 Guard: no switch when heater is off
- [ ] #4 Guard: no switch when setpoint is below minimum threshold
- [ ] #5 Event/log emitted when auto-switch occurs
- [ ] #6 MQTT sensor reports current mode (PWM vs continuous)
<!-- AC:END -->
