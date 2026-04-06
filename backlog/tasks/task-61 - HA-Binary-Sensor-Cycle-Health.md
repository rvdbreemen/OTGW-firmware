---
id: TASK-61
title: 'HA Binary Sensor: Cycle Health'
status: To Do
assignee: []
created_date: '2026-04-06 19:12'
labels:
  - ha-entity
  - binary-sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py
    (lines 414-448, SatCycleHealthSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatCycleHealthSensor` to MQTT auto-discovery. This binary sensor (device_class=PROBLEM) turns ON when the last completed heating cycle was classified as problematic (OVERSHOOT, UNDERHEAT, or SHORT_CYCLING). GOOD, UNCERTAIN, and INSUFFICIENT_DATA are considered healthy. Updated on cycle end events.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/cycle_health published as binary ON/OFF
- [ ] #2 HA auto-discovery config with device_class=problem
- [ ] #3 ON when last_cycle classification is OVERSHOOT, UNDERHEAT, or SHORT_CYCLING
- [ ] #4 OFF when GOOD, UNCERTAIN, or INSUFFICIENT_DATA
- [ ] #5 Updated at end of each heating cycle
<!-- AC:END -->
