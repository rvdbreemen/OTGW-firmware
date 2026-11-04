---
id: TASK-59
title: 'HA Binary Sensor: Pressure Health with EMA and regression'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:12'
updated_date: '2026-04-07 06:10'
labels:
  - ha-entity
  - binary-sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py
    (lines 200-388, SatPressureHealthSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatPressureHealthSensor` to MQTT auto-discovery with full parity. The existing firmware has basic pressure monitoring, but SAT Python's implementation is more sophisticated: EMA smoothing (alpha=0.05), linear regression drop rate calculation, 120s problem confirmation delay, 600s drop rate suspension after heating state change, and 6 extra_state_attributes for debugging. This task upgrades the existing pressure logic to match.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HA auto-discovery config for pressure_health binary sensor with device_class=problem
- [x] #2 EMA smoothed pressure with alpha=0.05
- [x] #3 Linear regression drop rate from pressure sample history (min 3 samples, 300s window)
- [x] #4 120-second problem confirmation delay before reporting
- [x] #5 600-second drop rate suspension after heating state transitions
- [x] #6 JSON attributes: pressure, smoothed_pressure, pressure_drop_rate_bar_per_hour, last_pressure, last_pressure_timestamp, last_seen_pressure_timestamp
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SAT Python SatPressureHealthSensor implementation for reference
2. Review existing pressure monitoring in SATcontrol.ino (satUpdatePressure)
3. Upgrade to match SAT Python: EMA alpha=0.05, linear regression drop rate
4. Add 120s problem confirmation delay, 600s suspension after CH state change
5. Publish JSON attributes via MQTT
6. Add HA discovery entry to mqttha.cfg
7. Build and verify
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Upgraded pressure health binary sensor to match SAT Python implementation.

Changes:
- Replaced flame-on-only suspension with CH active state transition tracking (600s suspension on any on/off transition, matching SAT Python _track_active_state)
- Changed ring buffer to store raw pressure samples instead of smoothed (matching SAT Python _record_pressure_sample)
- Reduced minimum sample count from 4 to 3 and added 300s minimum elapsed window check (matching PRESSURE_DROP_RATE_MIN_WINDOW_SECONDS)
- Added 3600s sample eviction window (matching SAT Python drop_window_seconds)
- Added JSON attributes topic (sat/pressure_health_attr) with all 6 attributes: pressure, smoothed_pressure, pressure_drop_rate_bar_per_hour, last_pressure, last_pressure_timestamp, last_seen_pressure_timestamp
- Added HA auto-discovery entry for pressure_health binary sensor with device_class=problem and json_attr_t
- Added state fields fLastPressure, iLastPressureMs, iLastSeenPressureMs to OTGW-firmware.h

Files changed: SATcontrol.ino, OTGW-firmware.h, data/mqttha.cfg
Build: ESP8266 compilation verified (734140 bytes flash, 62848 bytes RAM)
<!-- SECTION:FINAL_SUMMARY:END -->
