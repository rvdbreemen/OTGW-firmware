---
id: TASK-231
title: MQTT humidity input for Summer Simmer Index thermal comfort
status: To Do
assignee: []
created_date: '2026-04-09 05:42'
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
- [ ] #1 Add MQTT subscription set/<nodeId>/sat/humidity that stores value in state.sat.fHumidity
- [ ] #2 Add sat_thermal_comfort setting (bool) and sat_humidity_timeout_s setting
- [ ] #3 In satGetRoomTemp(): when thermal comfort active and humidity fresh, return SSI instead of raw room temp
- [ ] #4 Publish sat/humidity and sat/ssi to MQTT for HA sensor entities
- [ ] #5 Add ESP8266 and OTGW32 build guards — SSI computation must be platform-neutral
<!-- AC:END -->
