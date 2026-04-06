---
id: TASK-70
title: 'HA Sensor Entity: Flame Status (health enum)'
status: To Do
assignee: []
created_date: '2026-04-06 19:14'
labels:
  - ha-entity
  - sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/coordinator/__init__.py
    (flame property)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/types.py
    (FlameStatus enum if present)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python Flame Status sensor entity. SAT Python tracks flame health as a dedicated sensor with states: HEALTHY, IDLE_OK, STUCK_ON, STUCK_OFF, PWM_SHORT, SHORT_CYCLING, INSUFFICIENT_DATA. This is separate from the boiler status and provides flame-specific diagnostics. The firmware currently doesn't have a dedicated flame health tracker with these states.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/flame_status published with FlameStatus enum name string
- [ ] #2 HA auto-discovery config for sensor entity
- [ ] #3 States: HEALTHY, IDLE_OK, STUCK_ON, STUCK_OFF, PWM_SHORT, SHORT_CYCLING, INSUFFICIENT_DATA
- [ ] #4 HEALTHY when flame responds correctly to SAT commands
- [ ] #5 STUCK_ON when flame stays on after SAT requests off
- [ ] #6 STUCK_OFF when flame doesn't ignite after SAT requests heat
- [ ] #7 SHORT_CYCLING when flame cycles too rapidly
<!-- AC:END -->
