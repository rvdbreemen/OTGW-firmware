---
id: TASK-207
title: >-
  SAT fix: sensor staleness timeout mismatch (5 min C++ vs 6 hours Python
  default)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:24'
updated_date: '2026-04-09 06:15'
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
- [x] #1 satGetRoomTemp() uses settings.sat.iSensorMaxAgeS for external temp staleness check instead of hardcoded SAT_STALE_TEMP_MS
- [x] #2 Default value of settings.sat.iSensorMaxAgeS is 21600 (6 hours) matching Python
- [x] #3 BLE and outdoor sensor staleness remain on their own appropriate timeouts
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. SAT_STALE_TEMP_MS=300000ms (5 min) is used in satGetRoomTemp() for external indoor and BLE staleness checks
2. settings.sat.iSensorMaxAgeS already exists and defaults to 21600s (6 hours) - AC#2 confirmed
3. Change external indoor temp staleness check (lines 813, 829) to use settings.sat.iSensorMaxAgeS * 1000UL
4. BLE staleness stays on SAT_STALE_TEMP_MS (its own appropriate timeout per AC#3)
5. Rename SAT_STALE_TEMP_MS to clarify it is now for BLE only
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Changed satGetRoomTemp() external indoor temp staleness check from hardcoded SAT_STALE_TEMP_MS (5 min) to settings.sat.iSensorMaxAgeS * 1000UL. The iSensorMaxAgeS field already exists (TASK-82) and defaults to 21600s (6 hours), matching Python CONF_SENSOR_MAX_VALUE_AGE. BLE sensor retains its own SAT_STALE_TEMP_BLE_MS = 5 min constant (renamed for clarity). Outdoor sensor staleness unchanged (SAT_STALE_OUTDOOR_MS = 10 min).
<!-- SECTION:FINAL_SUMMARY:END -->
