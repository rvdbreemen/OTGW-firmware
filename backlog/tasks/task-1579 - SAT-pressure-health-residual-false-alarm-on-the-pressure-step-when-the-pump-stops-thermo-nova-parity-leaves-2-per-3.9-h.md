---
id: TASK-1579
title: >-
  SAT pressure health: residual false alarm on the pressure step when the pump
  stops (thermo-nova parity leaves 2 per 3.9 h)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T04:03:00Z'
updated_date: '2026-11-04T15:46:00Z'
labels:
  - sat
dependencies: []
priority: low
ordinal: 725000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1511 (maintainer decision 2026-11-03: close 1511 on thermo-nova parity). Real capture 20261030-092718 (gruijter, alpha.549): current code 2 alarm episodes in 3.91 h, the same as the thermo-nova oracle with the CH-mode bit (entry 201). The residual comes from the pressure step when the pump stops, which thermo-nova does not filter either. Any filter deviates from thermo-nova, so it waits for Sergeant D (Q-1511a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer to Q-1511a is recorded in sergeant-d-corrections.md
- [x] #2 If he chooses a filter: the capture replay (OTGW_PRESSURE_CAPTURE) gives 0 episodes and the slow-leak test still alarms
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 maintainer: Q-1511a not asked (entry 201), task parked at low priority.

2026-11-04 Robert decided (maintainer decision, entries 257-262; Sergeant D informed msg 1567417361798144021, may object). See docs/sat/sergeant-d-corrections.md.

2026-11-04 entry 262 tested on gruijter's capture 20261030-092718 (sonnet agent, scratch copies via OTGW_SAT_INO; no tracked change). Both remaining episodes come from the DROP-RATE path (smoothed 1.79/1.77 bar, min 0.8; rates 0.857 and 1.291 bar/h vs 0.3). Timeline ep.1: flame-off 8071 s, CH-mode bit falls (pump stop, existing 600 s suspension + ring clear) 8701 s, pressure step 1.99->1.71 bar 8791 s (720 s after flame-off), suspension ends 9301 s with the pre-step samples + the step still in the fresh ring, alarm 9400 s; ep.2 the same pattern. Replay: current 2, flame-off 600 s suspension (with or without ring clear) 2, thermo-nova oracle 2. Condition (a) 0 episodes fails, so per entry 262 the outcome is parity, no firmware change. Cause, as thermo-nova binary_sensor.py:357-365: samples keep being recorded during the suspension. Not tried as a decision: dropping samples during the suspension (1 episode in an experiment, breaks the thermo-nova oracle at 3391 s). AC1: entry 262 (maintainer decision) replaced the Sergeant D question (Q-1511a withdrawn, entry 201). AC2: the filter condition was tested and failed; parity kept.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Entry 262's 600 s flame-off suspension does not remove the two remaining pump-stop pressure alarms on gruijter's real capture (2 -> 2; the step comes 720 s after flame-off, after the CH-off suspension, and the ring still holds pre-step samples), so per the decision the firmware stays at thermo-nova parity. Evidence: replay via tests/test_sat_pressure_health.py OTGW_PRESSURE_CAPTURE on scratch variants, timelines in the notes. A different filter (e.g. no samples during the suspension) would need a new maintainer decision.
<!-- SECTION:FINAL_SUMMARY:END -->
