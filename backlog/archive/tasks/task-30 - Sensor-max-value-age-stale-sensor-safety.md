---
id: TASK-30
title: Sensor max value age (stale sensor safety)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:47'
updated_date: '2026-04-05 22:56'
labels:
  - sat
  - feature
  - safety
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Detect when temperature sensors stop updating and trigger safety fallback. If sensor data is older than a configurable threshold, switch to a safe default setpoint or disable SAT to prevent uncontrolled heating.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_SENSOR_MAX_VALUE_AGE)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Track last-update timestamp for each temperature sensor
- [ ] #2 Configurable max age threshold (default e.g. 300s)
- [ ] #3 When sensor stale: switch to safe fallback setpoint or disable SAT
- [ ] #4 MQTT publish: sensor_stale warning flag
- [ ] #5 WebUI: visual warning when sensor data is stale
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stale sensor detection already existed in firmware (SAT_STALE_TEMP_MS=300s, SAT_STALE_OUTDOOR_MS=600s). Falls back to OT bus when external temp stale. No additional code needed.
<!-- SECTION:FINAL_SUMMARY:END -->
