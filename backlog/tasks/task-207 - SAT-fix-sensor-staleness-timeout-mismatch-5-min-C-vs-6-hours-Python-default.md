---
id: TASK-207
title: >-
  SAT fix: sensor staleness timeout mismatch (5 min C++ vs 6 hours Python
  default)
status: To Do
assignee: []
created_date: '2026-04-09 05:24'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python CONF_SENSOR_MAX_VALUE_AGE defaults to 6 hours (21600s). C++ SAT_STALE_TEMP_MS=300000ms (5 minutes). When an indoor temperature sensor stops updating, Python tolerates up to 6 hours before treating the sensor as stale, while C++ trips in 5 minutes. This causes C++ to be much more aggressive about falling back or skipping control cycles. The C++ value should be made configurable (settings.sat.iSensorMaxAgeS exists per Task #82) and actually used in satGetRoomTemp() staleness check instead of the hardcoded SAT_STALE_TEMP_MS.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satGetRoomTemp() uses settings.sat.iSensorMaxAgeS for external temp staleness check instead of hardcoded SAT_STALE_TEMP_MS
- [ ] #2 Default value of settings.sat.iSensorMaxAgeS is 21600 (6 hours) matching Python
- [ ] #3 BLE and outdoor sensor staleness remain on their own appropriate timeouts
<!-- AC:END -->
