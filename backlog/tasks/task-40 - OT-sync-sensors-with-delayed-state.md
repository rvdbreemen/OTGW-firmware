---
id: TASK-40
title: OT sync sensors with delayed state
status: To Do
assignee: []
created_date: '2026-04-05 21:05'
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
- [ ] #1 Binary sensor detects mismatch between sent setpoint and reported setpoint
- [ ] #2 60s delay before declaring mismatch to prevent flickering
- [ ] #3 Sensor exposed via MQTT
- [ ] #4 Mismatch details available in sensor attributes
<!-- AC:END -->
