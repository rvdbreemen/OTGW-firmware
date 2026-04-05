---
id: TASK-32
title: Flame-off setpoint offset
status: To Do
assignee: []
created_date: '2026-04-05 20:47'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Apply a specific setpoint offset when the flame is off to prevent unnecessary boiler restarts. When flame goes off, adjust the control setpoint by a configurable offset to create a hysteresis band. This reduces short-cycling by requiring a larger temperature drop before the boiler restarts.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_FLAME_OFF_SETPOINT_OFFSET_CELSIUS)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Configurable flame-off setpoint offset (default e.g. 0.5C)
- [ ] #2 When flame off: control_setpoint += offset (raises threshold for restart)
- [ ] #3 When flame on: use normal setpoint calculation
- [ ] #4 MQTT/REST/WebUI: flame-off offset setting
<!-- AC:END -->
