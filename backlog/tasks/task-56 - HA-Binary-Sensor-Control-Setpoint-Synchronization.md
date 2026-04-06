---
id: TASK-56
title: 'HA Binary Sensor: Control Setpoint Synchronization'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:11'
updated_date: '2026-04-06 20:29'
labels:
  - ha-entity
  - binary-sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py
    (lines 91-122, SatControlSetpointSyncSensor)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py
    (lines 68-88, SatSyncSensor mixin)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatControlSetpointSyncSensor` to MQTT auto-discovery. This binary sensor (device_class=PROBLEM) detects when the boiler's actual control setpoint doesn't match what SAT requested. Uses a 60-second delay to avoid false positives during normal control transitions. The sensor turns ON (problem) when the mismatch persists for >60 seconds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT topic sat/setpoint_sync published as binary ON/OFF
- [x] #2 HA auto-discovery config with device_class=problem
- [x] #3 Compares SAT's requested setpoint vs boiler's reported setpoint (rounded to 0.1C)
- [x] #4 60-second delay before reporting mismatch (SatSyncSensor pattern)
- [x] #5 Turns OFF immediately when setpoints match again
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Setpoint sync binary sensor with 60s delay. Compares SAT setpoint vs OT TSet with 0.5C tolerance.
<!-- SECTION:FINAL_SUMMARY:END -->
