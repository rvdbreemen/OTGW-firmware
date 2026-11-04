---
id: TASK-231
title: MQTT humidity input for Summer Simmer Index thermal comfort
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:42'
updated_date: '2026-04-09 10:42'
labels:
  - sat
  - future-sprint
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python SAT feeds the Summer Simmer Index (SSI = f(room_temp, humidity)) into the PID as the effective room temperature when thermal_comfort mode is enabled. The OT protocol has no humidity message. Solution: add a MQTT subscription to set/<nodeId>/sat/humidity so any external sensor (Zigbee, ESPHome, BLE on OTGW32) can push humidity in. Store in state.sat.fHumidity and use it in satGetRoomTemp() when thermal comfort mode is active.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add MQTT subscription set/<nodeId>/sat/humidity that stores value in state.sat.fHumidity
- [x] #2 Add sat_thermal_comfort setting (bool) and sat_humidity_timeout_s setting
- [x] #3 In satGetRoomTemp(): when thermal comfort active and humidity fresh, return SSI instead of raw room temp
- [x] #4 Publish sat/humidity and sat/ssi to MQTT for HA sensor entities
- [x] #5 Add ESP8266 and OTGW32 build guards — SSI computation must be platform-neutral
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Audit existing code: AC#1 (MQTT sub) and AC#3 (SSI in satGetRoomTemp) are already done from TASK-204.
2. AC#2: Add iHumidityTimeoutS to SATSection in OTGW-firmware.h (bThermalComfort already exists). Persist it in settingStuff.ino. Add MQTT subscription for thermal_comfort and humidity_timeout_s settings.
3. AC#3: Replace hardcoded 1800000UL in satGetRoomTemp() with settings.sat.iHumidityTimeoutS * 1000UL.
4. AC#4: Add sat/ssi MQTT publish alongside existing sat/summer_simmer_index. Also add sat/humidity_timeout_s and sat/thermal_comfort_enable to the retained settings publish block.
5. AC#5: No platform guards needed - float math is neutral. Verify trivially.
6. Add MQTT subscription for thermal_comfort setting toggle.
7. Commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- AC#1: sat/humidity MQTT subscription already existed (satHandleHumidity), confirmed working
- AC#2: Added iHumidityTimeoutS (uint16_t, default 1800) to SATSection; bThermalComfort already existed. Wired both into settingStuff.ino write+parse. Added MQTT subscriptions for thermal_comfort and humidity_timeout_s.
- AC#3: Replaced hardcoded 1800000UL in satGetRoomTemp() with settings.sat.iHumidityTimeoutS * 1000UL
- AC#4: Added sat/ssi (2 decimals) alongside existing sat/summer_simmer_index. Upgraded sat/humidity publish from 0 to 1 decimal. Added sat/humidity_timeout_s to retained settings publish block.
- AC#5: Pure float math, no platform guards needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added MQTT humidity input for Summer Simmer Index (SSI) thermal comfort calculation.

Most of the groundwork from TASK-204 was already in place. This task closes the remaining gaps:

- OTGW-firmware.h: Added iHumidityTimeoutS (uint16_t, default 1800s) to SATSection next to bThermalComfort.
- settingStuff.ino: Added persist/load for SATthermalcomfort and SAThumiditytimeout (both read and write).
- MQTTstuff.ino: Added MQTT subscriptions for set/<nodeId>/sat/thermal_comfort and set/<nodeId>/sat/humidity_timeout_s alongside the existing sat/humidity handler.
- SATcontrol.ino:
  - satGetRoomTemp(): replaced hardcoded 1800000UL with settings.sat.iHumidityTimeoutS * 1000UL.
  - satPublishMQTT(): added sat/ssi (2 decimals) alongside sat/summer_simmer_index; upgraded sat/humidity from 0 to 1 decimal per spec; added sat/humidity_timeout_s to retained settings publish block.

No new platform guards needed - all changes are plain float math compatible with ESP8266 and ESP32.
<!-- SECTION:FINAL_SUMMARY:END -->
