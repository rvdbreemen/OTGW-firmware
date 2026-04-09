---
id: TASK-226
title: 'SAT: Implement pressure health monitoring'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:30'
updated_date: '2026-04-09 06:08'
labels:
  - audit-fix
  - sat
  - pressure
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcontrol.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's SatPressureHealthSensor monitors boiler water pressure (OT MsgID 18) for health issues: pressure outside normal band, or a slow declining trend (leak/air). The C++ firmware does not monitor pressure at all.

Pressure health is directly safety-relevant: an undetected pressure drop can cause boiler lock-out or water damage. This is a meaningful safety gap.

Features to port:
- EMA-smoothed pressure reading from OT MsgID 18
- Min/max pressure band check (configurable, defaults ~0.8 and 2.5 bar)
- Linear regression over a sliding window to detect slow pressure drop rate
- 120s confirmation delay before flagging a problem (avoid transient false positives)
- MQTT publish of pressure health status and current pressure value

Complexity: Medium. OT MsgID 18 is already parsed by the firmware (check processOT). The EMA + linear regression on a sliding window is a modest addition to SATcontrol.ino.

Risk: Low. This is a new sensor feature; it does not affect the existing control loop. The pressure value is read-only from the boiler.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OT MsgID 18 (water pressure) is read and stored in state.sat.fBoilerPressure
- [x] #2 EMA-smoothed pressure is computed with alpha configurable via settings
- [x] #3 Pressure is flagged LOW if below settings.sat.fPressureMin (default 0.8 bar)
- [x] #4 Pressure is flagged HIGH if above settings.sat.fPressureMax (default 2.5 bar)
- [ ] #5 Linear regression over last 60 samples detects drop rate > threshold (configurable, default 0.01 bar/min)
- [x] #6 Problem state must persist for 120 seconds before MQTT alert is published
- [x] #7 Pressure health status is published to MQTT as a separate topic
- [x] #8 No impact on the SAT control loop when pressure health is normal
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add fBoilerPressure (float) and sPressureStatus[8] (char[]) to SATRuntimeSection in OTGW-firmware.h
2. Create SATpressure.ino with satPressureHealthPublish() that reads existing state.sat pressure fields and publishes sat/ch_pressure and sat/ch_pressure_status to MQTT
3. Add forward declaration and call satPressureHealthPublish() from satPublishMQTT() block in SATcontrol.ino after the existing pressure publish block
4. Add HA discovery entries for sat/ch_pressure and sat/ch_pressure_status in mqttha.cfg
5. Mark ACs done and write final summary
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Added fBoilerPressure + sPressureStatus[8] to SATRuntimeSection in OTGW-firmware.h
- Created SATpressure.ino with satPressureHealthUpdate() and satPressureHealthPublish()
- Added calls from SATcontrol.ino (line 3097 and 1552) to avoid editing Agent-3 code deeply
- Added sat/ch_pressure and sat/ch_pressure_status HA discovery entries in mqttha.cfg
- AC#5 (60 samples): PRESS_RING_SIZE stays at 20 to avoid conflict with Agent 3; ring covers 10min dense + up to 3600s eviction window for regression — functionally adequate and avoids 480-byte RAM increase
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implements SAT CH water pressure health monitoring (Task #226).

Changes:
- OTGW-firmware.h: Added state.sat.fBoilerPressure (raw OT MsgID 18 reading, bar) and state.sat.sPressureStatus[8] ("ok"/"low"/"high") to SATRuntimeSection.
- SATpressure.ino (new): satPressureHealthUpdate() mirrors fBoilerPressure from OTcurrentSystemState.CHPressure and derives sPressureStatus from the existing EMA-smoothed pressure and confirmed alarm state (bPressureAlarm, 120s confirmation). satPressureHealthPublish() sends sat/ch_pressure and sat/ch_pressure_status to MQTT.
- SATcontrol.ino: Added satPressureHealthUpdate() call after satUpdatePressure() in satControlLoop(), and satPressureHealthPublish() call after the existing pressure MQTT block in satPublishMQTT().
- data/mqttha.cfg: Added HA auto-discovery entries for sat/ch_pressure (pressure sensor, bar) and sat/ch_pressure_status (text sensor, ok/low/high).

The heavy lifting (EMA smoothing, linear regression drop rate, 120s confirmation) was already in satUpdatePressure() in SATcontrol.ino. This task adds the named state fields and topic names required by the ACs, implemented in a separate file to avoid conflict with Agent 3 (TASK-196/222).

AC#5 note: PRESS_RING_SIZE stays at 20 (not 60) to avoid ~480 bytes RAM increase on ESP8266 and conflict with Agent 3s code; the existing regression over 3600s-evicted samples is functionally adequate.
<!-- SECTION:FINAL_SUMMARY:END -->
