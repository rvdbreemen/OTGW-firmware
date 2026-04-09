---
id: TASK-232
title: Boiler power and gas consumption estimation
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
Python SAT estimates boiler gas consumption as modulation% x rated_power x efficiency x time. OT MsgID 17 provides modulation level but not rated power. Solution: add a user-configurable sat_boiler_rated_kw setting (default 0 = disabled). When set, accumulate estimated kWh into a dedicated LittleFS file (/sat_energy_estimate.json). Expose via sat/energy_estimated_kwh MQTT topic and HA energy dashboard entity. OT MsgIDs 25/26 (flow/return temp) + MsgID 74 (flow rate) could enable thermal power calculation but flow rate is rarely supported by boilers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Add sat_boiler_rated_kw (float, 0=disabled) and sat_boiler_efficiency (float, default 0.92) settings
- [ ] #2 Accumulate estimated kWh: modulation% / 100 * rated_kW * efficiency * dt each loop cycle
- [ ] #3 Persist accumulated value to /sat_energy_estimate.json (save every 0.1 kWh)
- [ ] #4 Publish sat/energy_estimated_kwh to MQTT as retained, expose as HA energy sensor
- [ ] #5 If MsgID 74 (flow rate) is available from boiler: use thermal power calculation instead of modulation estimate
<!-- AC:END -->
