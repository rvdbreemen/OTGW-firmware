---
id: TASK-1818
title: >-
  Replay of a real gruijter dataset on marvin (.61, beta.182): 11 Nov
  08:00-08:45, timed
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T20:33:00Z'
updated_date: '2026-11-11T20:34:00Z'
labels:
  - bench
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 911000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert asked (2026-11-11 ~20:40) for a replay of a real gruijter dataset on marvin. Source: capture-gruijter-1111 otframes.log (beta.171, 10 Nov 21:18 to 11 Nov 17:26, 178k frames). Window 11 Nov 08:00-08:45 (busiest hour: 40 flame transitions, DHW demand), 7024 frames, host-time column dropped and device time cut to ms (ADR-209 format), 169 KB to /otgw_simulation.log; POST /api/v2/simulate/start -> source file, timing timed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Replay runs on .61 with the recorded gaps
- [x] #2 Device values match the dataset
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 1 (20:49): /api/v2/otgw/otmonitor vs dataset frames around 08:00:30: flow 45.09 = 45.09, room 17.83 = 17.83, room setpoint 19 = 19.0, CS 48 = 48.0, modulation 7 % = 7.0, flame on = on.

Evidence 2 (21:40): /api/v2/graph/history (30 s slots) last 40 slots vs the dataset binned per 30 s (MsgID 25 boiler READ_ACK): mean |diff| 0.37 C at the best offset (bin 50; next best 0.44, 0.45). A first cyclic alignment over all 116 slots gave 15.4 C because the history also holds slots from before the replay and the loop restart; the non-cyclic window is the valid comparison. Files: scratchpad replay61/ (graph_history*.json, otmonitor.json, compare*.txt).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: simulate status source=file timing=timed on .61 from 20:48. AC2: live values equal the dataset at 08:00:30 (6 fields) and the device's own graph history matches the dataset within 0.37 C mean over 20 min. Not checked: SAT on, MQTT/HA side (Robert asked only for the replay). Replay stopped for the TASK-1771 bench on Robert's choice (record, then bench).
<!-- SECTION:FINAL_SUMMARY:END -->
