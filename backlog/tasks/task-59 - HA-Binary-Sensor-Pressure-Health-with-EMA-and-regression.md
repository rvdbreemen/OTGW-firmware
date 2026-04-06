---
id: TASK-59
title: 'HA Binary Sensor: Pressure Health with EMA and regression'
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
    (lines 200-388, SatPressureHealthSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatPressureHealthSensor` to MQTT auto-discovery with full parity. The existing firmware has basic pressure monitoring, but SAT Python's implementation is more sophisticated: EMA smoothing (alpha=0.05), linear regression drop rate calculation, 120s problem confirmation delay, 600s drop rate suspension after heating state change, and 6 extra_state_attributes for debugging. This task upgrades the existing pressure logic to match.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 HA auto-discovery config for pressure_health binary sensor with device_class=problem
- [ ] #2 EMA smoothed pressure with alpha=0.05
- [ ] #3 Linear regression drop rate from pressure sample history (min 3 samples, 300s window)
- [ ] #4 120-second problem confirmation delay before reporting
- [ ] #5 600-second drop rate suspension after heating state transitions
- [ ] #6 JSON attributes: pressure, smoothed_pressure, pressure_drop_rate_bar_per_hour, last_pressure, last_pressure_timestamp, last_seen_pressure_timestamp
<!-- AC:END -->
