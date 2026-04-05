---
id: TASK-13
title: Improve boiler status evaluator per SAT Python state machine
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:07'
updated_date: '2026-04-05 23:14'
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
- [x] #1 Anti-cycling state: if flame off < 180s and there is demand -> SAT_BS_ANTI_CYCLING
- [x] #2 Stalled ignition: if flame off > max(600s, 3x previous cycle duration) and there is demand -> SAT_BS_STALLED_IGNITION
- [x] #3 Pump starting: first flame, flow temp dropping, delta > 6C -> SAT_BS_PUMP_STARTING
- [x] #4 Post-cycle settling: flame just off, < 60s ago, no demand -> SAT_BS_POST_CYCLE
- [x] #5 Demand detection: setpoint > flow_temp + 0.7C (hysteresis)
- [x] #6 Ignition surge: within 30s of flame on, temp rise rate >= 0.5C/s
- [x] #7 AT_SETPOINT band: |boilerTemp - setpoint| <= 1.5C (was 3.0C)
- [x] #8 State timing tracking: _bs_flameOnMs, _bs_flameOffMs timestamps
- [x] #9 REST API: boiler_status field now with string labels instead of just numbers
- [x] #10 MQTT publish: sat/boiler_status as string label
- [x] #11 WebUI: boiler status with descriptive text instead of number
- [x] #12 HA auto-discovery: sensor entity with string values for boiler status
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add flame timing statics (_bs_flameOnMs, _bs_flameOffMs, _bs_lastCycleDurationMs)\n2. Rewrite satUpdateBoilerStatus with SAT Python state machine logic\n3. Add string label helper satGetBoilerStatusName()\n4. Update REST JSON to include string label\n5. Update MQTT to publish string label\n6. Update WebUI sat.js to show string labels\n7. Commit with descriptive title
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT thermo-nova boiler status evaluator reference (device/status.py:32-116):
14-state machine with states:
- PREHEATING: delta between setpoint and flow > 6C
- AT_SETPOINT_BAND: flow within +-1.5C of setpoint
- IGNITION_SURGE: flow rising > 0.5C/s (rapid temp spike on ignition)
- PUMP_STARTING: initial pump spin-up detection
- POST_CYCLE_SETTLING: 60s cooldown after flame off
- OVERSHOOT_COOLING: flow > setpoint + 3C (boiler thermal mass overshoot)
- ANTI_CYCLING: 180s minimum off time between cycles
- STALLED_IGNITION: heater off > 600s without expected ignition
- MODULATING_UP: modulation increasing
- MODULATING_DOWN: modulation decreasing
- Plus additional transitional states
- Hysteresis: 0.7C for state transitions to prevent chattering
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rewrote boiler status evaluator to match SAT Python (status.py) 14-state machine.\n\nChanges:\n- Added flame timing: _bs_flameOnMs, _bs_flameOffMs, _bs_lastCycleDurationMs\n- Anti-cycling: 180s min OFF between cycles\n- Stalled ignition: no flame after max(600s, 3x last cycle)\n- Pump starting: early flame with temp dropping and delta > 6C\n- Post-cycle settling: 60s cooldown after flame off\n- Demand hysteresis: setpoint > flow + 0.7C\n- Ignition surge: within 30s, temp rise >= 0.5C/s\n- AT_SETPOINT band: 1.5C (was 3.0C)\n- String labels: REST, MQTT, WebUI now use descriptive names\n- PROGMEM string table with satGetBoilerStatusName() helper\n- HA auto-discovery works with string state values\n\nFiles: SATcontrol.ino, sat.js
<!-- SECTION:FINAL_SUMMARY:END -->
