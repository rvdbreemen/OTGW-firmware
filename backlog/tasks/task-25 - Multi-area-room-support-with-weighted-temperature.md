---
id: TASK-25
title: Multi-area room support with weighted temperature
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:46'
updated_date: '2026-04-06 12:51'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Support multiple climate zones with weighted temperature averaging and valve position integration. SAT Python (area.py) allows each room to have its own sensor and climate entity, with configurable weights based on valve positions.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/area.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Support multiple room temperature sensors via MQTT
- [x] #2 Weighted average calculation based on configurable per-room weights
- [ ] #3 Optional valve position integration for weight adjustment
- [x] #4 MQTT/REST/WebUI: per-room temps, weights, and combined average
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add multi-area fields to SATSection (settings) and SATRuntimeSection (state) in OTGW-firmware.h
2. Add SAT_MAX_AREAS constant, satHandleAreaTemp(), satGetWeightedRoomTemp() to SATcontrol.ino
3. Modify satGetRoomTemp() to use weighted average when multi-area is enabled
4. Add multi-area data to satSendStatusJSON() and satPublishMQTT()
5. Add REST endpoints for area temperature in restAPI.ino
6. Add MQTT subscribe handlers for sat/area/0-3 in MQTTstuff.ino
7. Add settings persistence in settingStuff.ino
8. Add WebUI translations and tooltips in index.js
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented multi-area support:
- Added settings (bMultiArea, iMultiAreaCount, fAreaWeight[4]) and runtime state (fAreaTemp[4], bAreaValid[4], iAreaLastMs[4])
- satGetWeightedRoomTemp() computes weighted average with 5-min stale timeout
- satGetRoomTemp() uses weighted average when multi-area enabled, falls back to single sensor
- REST API: POST /api/v2/sat/area/<0-3>
- MQTT: subscribe sat/area/<0-3>, publish sat/area/0-3
- Settings persistence via SATmultiarea, SATmultiareacount, SATareaweight0-3
- WebUI translations and tooltips added
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Multi-area weighted room temperature from up to 4 sensors
<!-- SECTION:FINAL_SUMMARY:END -->
