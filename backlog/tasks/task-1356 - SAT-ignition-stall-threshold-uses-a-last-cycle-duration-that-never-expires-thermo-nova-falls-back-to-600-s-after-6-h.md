---
id: TASK-1356
title: >-
  SAT: ignition-stall threshold uses a last cycle duration that never expires
  (thermo-nova falls back to 600 s after 6 h)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T16:44:00Z'
updated_date: '2026-11-04T07:13:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 491000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1352: SATcontrol.ino:678 bases the stalled-ignition threshold on fLastCycleDuration, which never expires; thermo-nova device/status.py:135-162 is_ignition_stalled uses last_cycle.duration and falls back to a 600 s floor when last_cycle is None (older than 6 h, cycles/history.py:48-57). Boiler status SAT_BS_STALLED_IGNITION may reach control, so this is a SAT behaviour change: plan with citations, Sergeant D's OK first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with thermo-nova citations agreed by Sergeant D (entry recorded); then threshold matches thermo-nova (host test red/green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: SATcontrol.ino:833-836 fLastCycleDuration no 6 h expiry. Plan to Sergeant D.

2026-11-04 Robert decided (maintainer decision, entries 257-262; Sergeant D informed msg 1567417361798144021, may object). See docs/sat/sergeant-d-corrections.md.

2026-11-04 implemented (sonnet agent; lead reran on the integration tree): tests/test_sat_status_display.py 8 OK (old code 7 failures), mqtt_contract_1485 15, sim_port_equivalence 4, last_cycle_expiry 10, overshoot_sustain 62 OK, evaluate 0 failed. Tag beta.67 (one commit for TASK-1360, TASK-1356, TASK-1361).
<!-- SECTION:NOTES:END -->
