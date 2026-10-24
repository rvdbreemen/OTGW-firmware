---
id: TASK-1356
title: >-
  SAT: ignition-stall threshold uses a last cycle duration that never expires
  (thermo-nova falls back to 600 s after 6 h)
status: To Do
assignee: []
created_date: '2026-10-24T16:44:00Z'
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
