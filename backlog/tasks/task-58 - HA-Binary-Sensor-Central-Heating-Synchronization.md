---
id: TASK-58
title: 'HA Binary Sensor: Central Heating Synchronization'
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
    (lines 162-197, SatCentralHeatingSyncSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatCentralHeatingSyncSensor` to MQTT auto-discovery. This binary sensor (device_class=PROBLEM) detects when the boiler's heating state doesn't match the expected HVAC action. For example: SAT says HEATING but boiler is not active, or SAT says IDLE but boiler is still heating. Uses the same 60-second delay pattern.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/ch_sync published as binary ON/OFF
- [ ] #2 HA auto-discovery config with device_class=problem
- [ ] #3 Validates: OFF+inactive=ok, IDLE+inactive=ok, HEATING+active=ok, anything else=mismatch
- [ ] #4 60-second delay before reporting mismatch
- [ ] #5 Turns OFF immediately when state matches expectations
<!-- AC:END -->
