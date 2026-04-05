---
id: TASK-13
title: Improve boiler status evaluator per SAT Python state machine
status: To Do
assignee: []
created_date: '2026-04-05 10:07'
updated_date: '2026-04-05 10:22'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python has a more advanced boiler status evaluator with extra states and better detection. The current port is missing: anti-cycling detection (min 180s OFF before flame may restart), stalled ignition (if flame doesn't come after max(600s, 3x previous cycle duration)), pump starting detection, post-cycle settling, and demand hysteresis (0.7C above setpoint). Also missing are timing constants that are well documented in SAT Python. The improved evaluator gives users better insight into boiler behavior and helps with debugging.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Anti-cycling state: if flame off < 180s and there is demand -> SAT_BS_ANTI_CYCLING
- [ ] #2 Stalled ignition: if flame off > max(600s, 3x previous cycle duration) and there is demand -> SAT_BS_STALLED_IGNITION
- [ ] #3 Pump starting: first flame, flow temp dropping, delta > 6C -> SAT_BS_PUMP_STARTING
- [ ] #4 Post-cycle settling: flame just off, < 60s ago, no demand -> SAT_BS_POST_CYCLE
- [ ] #5 Demand detection: setpoint > flow_temp + 0.7C (hysteresis)
- [ ] #6 Ignition surge: within 30s of flame on, temp rise rate >= 0.5C/s
- [ ] #7 AT_SETPOINT band: |boilerTemp - setpoint| <= 1.5C (was 3.0C)
- [ ] #8 State timing tracking: _bs_flameOnMs, _bs_flameOffMs timestamps
- [ ] #9 REST API: boiler_status field now with string labels instead of just numbers
- [ ] #10 MQTT publish: sat/boiler_status as string label
- [ ] #11 WebUI: boiler status with descriptive text instead of number
- [ ] #12 HA auto-discovery: sensor entity with string values for boiler status
<!-- AC:END -->
