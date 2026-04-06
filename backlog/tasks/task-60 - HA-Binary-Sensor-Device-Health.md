---
id: TASK-60
title: 'HA Binary Sensor: Device Health'
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
    (lines 391-411, SatDeviceHealthSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatDeviceHealthSensor` to MQTT auto-discovery. This binary sensor (device_class=PROBLEM) turns ON when the device status is INSUFFICIENT_DATA, indicating the boiler is not providing enough data for SAT to function properly. Simple but important diagnostic indicator.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/device_health published as binary ON/OFF
- [ ] #2 HA auto-discovery config with device_class=problem
- [ ] #3 ON when boiler_status == INSUFFICIENT_DATA
- [ ] #4 OFF when any other valid boiler status is reported
<!-- AC:END -->
