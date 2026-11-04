---
id: TASK-60
title: 'HA Binary Sensor: Device Health'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:12'
updated_date: '2026-04-06 20:10'
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
- [x] #1 MQTT topic sat/device_health published as binary ON/OFF
- [x] #2 HA auto-discovery config with device_class=problem
- [x] #3 ON when boiler_status == INSUFFICIENT_DATA
- [x] #4 OFF when any other valid boiler status is reported
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Published sat/device_health as ON when eBoilerStatus==SAT_BS_OFF, OFF otherwise. SAT_BS_OFF maps to Python's INSUFFICIENT_DATA.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Device Health binary sensor publishes sat/device_health ON/OFF (1b472b60)
<!-- SECTION:FINAL_SUMMARY:END -->
