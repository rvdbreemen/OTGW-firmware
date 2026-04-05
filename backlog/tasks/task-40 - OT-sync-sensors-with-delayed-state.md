---
id: TASK-40
title: OT sync sensors with delayed state
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:05'
updated_date: '2026-04-05 23:25'
labels:
  - sat
  - feature
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py:68-198
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Detect rounding mismatches between setpoint sent to boiler vs reported back. Expose as binary sensor with 60s delay to prevent flickering on transient differences. Useful for diagnosing OT communication issues.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Binary sensor detects mismatch between sent setpoint and reported setpoint
- [x] #2 60s delay before declaring mismatch to prevent flickering
- [x] #3 Sensor exposed via MQTT
- [x] #4 Mismatch details available in sensor attributes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT setpoint sync sensor with delayed mismatch detection.\n\nChanges:\n- Compares sent CS= setpoint vs boiler-reported TSet\n- 60s confirmation delay prevents flickering on transient differences\n- Mismatch threshold: > 1.0C difference\n- MQTT: sat/setpoint_mismatch, REST: setpoint_mismatch\n\nFiles: OTGW-firmware.h, SATcontrol.ino
<!-- SECTION:FINAL_SUMMARY:END -->
