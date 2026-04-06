---
id: TASK-51
title: 'HA Sensor Entity: Requested Setpoint'
status: To Do
assignee: []
created_date: '2026-04-06 19:11'
labels:
  - ha-entity
  - sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
    (lines 71-98, SatRequestedSetpoint)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 370-394, requested_setpoint property)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatRequestedSetpoint` sensor entity to MQTT auto-discovery. This sensor exposes the setpoint that SAT requests (before PWM/modulation adjustments), as a separate HA sensor entity with device_class=temperature and unit=C. Currently this value is computed but not published as a dedicated MQTT topic with HA discovery.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/requested_setpoint published with the pre-adjustment setpoint value
- [ ] #2 HA auto-discovery config added in mqttha.cfg for sensor with device_class=temperature, unit=C
- [ ] #3 Value matches SAT Python's requested_setpoint property (heating_curve + PID output, clamped)
<!-- AC:END -->
