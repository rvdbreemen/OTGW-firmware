---
id: TASK-70
title: 'HA Sensor Entity: Flame Status (health enum)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:14'
updated_date: '2026-04-06 20:46'
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
- [x] #1 MQTT topic sat/flame_status published with FlameStatus enum name string
- [x] #2 HA auto-discovery config for sensor entity
- [x] #3 States: HEALTHY, IDLE_OK, STUCK_ON, STUCK_OFF, PWM_SHORT, SHORT_CYCLING, INSUFFICIENT_DATA
- [x] #4 HEALTHY when flame responds correctly to SAT commands
- [x] #5 STUCK_ON when flame stays on after SAT requests off
- [x] #6 STUCK_OFF when flame doesn't ignite after SAT requests heat
- [x] #7 SHORT_CYCLING when flame cycles too rapidly
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added SATFlameStatus enum and satUpdateFlameStatus() function. Tracks flame health based on SAT active state, safety trips, cycle classification, and flame/setpoint mismatch. Publishes sat/flame_status. HA sensor discovery added.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Flame status sensor with 7 states. 10s update, detects stuck on/off, short cycling, PWM short.
<!-- SECTION:FINAL_SUMMARY:END -->
