---
id: TASK-58
title: 'HA Binary Sensor: Central Heating Synchronization'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:12'
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
    (lines 162-197, SatCentralHeatingSyncSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatCentralHeatingSyncSensor` to MQTT auto-discovery. This binary sensor (device_class=PROBLEM) detects when the boiler's heating state doesn't match the expected HVAC action. For example: SAT says HEATING but boiler is not active, or SAT says IDLE but boiler is still heating. Uses the same 60-second delay pattern.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT topic sat/ch_sync published as binary ON/OFF
- [x] #2 HA auto-discovery config with device_class=problem
- [x] #3 Validates: OFF+inactive=ok, IDLE+inactive=ok, HEATING+active=ok, anything else=mismatch
- [x] #4 60-second delay before reporting mismatch
- [x] #5 Turns OFF immediately when state matches expectations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CH sync binary sensor with 60s delay. Compares SAT active vs OT SlaveStatus CH active bit.
<!-- SECTION:FINAL_SUMMARY:END -->
