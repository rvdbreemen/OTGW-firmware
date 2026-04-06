---
id: TASK-57
title: 'HA Binary Sensor: Relative Modulation Synchronization'
status: To Do
assignee: []
created_date: '2026-04-06 19:11'
labels:
  - ha-entity
  - binary-sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py
    (lines 125-159, SatRelativeModulationSyncSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatRelativeModulationSyncSensor` to MQTT auto-discovery. This binary sensor (device_class=PROBLEM) detects when the boiler's maximum relative modulation doesn't match what SAT set. Uses the same 60-second delay pattern as setpoint sync. Important for detecting boilers that ignore modulation commands (common with some manufacturers like Intergas, Nefit).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/modulation_sync published as binary ON/OFF
- [ ] #2 HA auto-discovery config with device_class=problem
- [ ] #3 Compares SAT's requested max modulation vs boiler's reported max modulation (as int)
- [ ] #4 60-second delay before reporting mismatch
- [ ] #5 Only available when relative modulation is supported
<!-- AC:END -->
