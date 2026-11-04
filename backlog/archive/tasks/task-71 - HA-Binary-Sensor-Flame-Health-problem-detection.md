---
id: TASK-71
title: 'HA Binary Sensor: Flame Health (problem detection)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:14'
updated_date: '2026-04-06 20:46'
labels:
  - ha-entity
  - binary-sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies:
  - TASK-70
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py
    (SatCycleHealthSensor pattern)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python Flame Health binary sensor. This is a device_class=PROBLEM binary sensor that turns ON when the flame status is anything other than HEALTHY or IDLE_OK. It becomes unavailable when flame status is INSUFFICIENT_DATA. This is a companion to the Flame Status sensor (task #70) - the sensor shows the detailed state, the binary sensor provides a simple problem/no-problem indicator for HA automations and alerts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT topic sat/flame_health published as binary ON/OFF
- [x] #2 HA auto-discovery config with device_class=problem
- [x] #3 ON when flame_status is STUCK_ON, STUCK_OFF, PWM_SHORT, or SHORT_CYCLING
- [x] #4 OFF when flame_status is HEALTHY or IDLE_OK
- [x] #5 Unavailable when INSUFFICIENT_DATA
- [x] #6 Depends on task #70 (Flame Status sensor) for state data
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Published sat/flame_health binary sensor ON/OFF. ON when flame status is STUCK_ON/STUCK_OFF/PWM_SHORT/SHORT_CYCLING. OFF when HEALTHY/IDLE_OK. Not published when INSUFFICIENT_DATA. HA binary_sensor discovery with device_class=problem.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Flame health binary sensor. ON for problematic states, OFF for healthy, unavailable when insufficient data.
<!-- SECTION:FINAL_SUMMARY:END -->
