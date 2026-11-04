---
id: TASK-1463
title: sat/cycles_this_hour announced as total_increasing but resets every hour
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:21:00Z'
updated_date: '2026-10-31T05:42:00Z'
labels:
  - sat
  - mqtt
  - ha-discovery
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 623000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTTHaDiscovery.cpp:1341 announces state_class total_increasing; the value resets hourly, which HA reads as a meter reset each hour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 state_class matches the hourly-reset semantics (measurement), with the reason recorded
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 (session otgw-firmware-ea): lane wt-v1463 applied on dev as a958d397d (alpha.565). MQTT.md: dropped the lane's claim '0 outside PWM' (satCycleGetCyclesThisHour counts the rolling-hour ring regardless of mode, SATcycles.ino:365-371); the pre-existing CHANGELOG TASK-1401 line still says it reads 0 in continuous mode, which the same code does not support for the first hour after leaving PWM (not changed here). Evidence: tests/test_sat_energy_discovery.py old discovery table FAILED (1), fix OK; evaluate --quick Failed 0, 100%; esp32-combo build SUCCESS; OTA to OTGW32 .61 (173 settings kept); retained homeassistant/sensor/otgw-1020BA21B4F8/.../config for sat/cycles_this_hour carries "state_class":"measurement". Zip msg 1565964116013883405, report msg 1565964177628209273.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
sat/cycles_this_hour is announced with state_class measurement (was total_increasing): it counts PWM ON-phase starts in the rolling hour and falls as starts leave it, so HA read each drop as a meter reset. Evidence: host test old FAIL / fix OK; bench broker shows state_class measurement on alpha.565+a958d39. MQTT.md and CHANGELOG updated.
<!-- SECTION:FINAL_SUMMARY:END -->
