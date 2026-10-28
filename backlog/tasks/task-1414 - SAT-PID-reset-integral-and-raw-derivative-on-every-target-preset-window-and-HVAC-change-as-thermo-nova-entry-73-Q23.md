---
id: TASK-1414
title: >-
  SAT PID: reset integral and raw derivative on every target, preset, window and
  HVAC change as thermo-nova (entry 73, Q23)
status: To Do
assignee: []
created_date: '2026-10-28T06:44:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 578000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 73, Q23: follow thermo-nova). Reset integral + raw derivative (main and zone PIDs) on every real target change, preset change/clear, window switch/restore, HVAC on/off and reset_integral, as thermo-nova reset_control_state (pid.py:134-137). New reset that zeros only integral and raw derivative; do NOT reuse satPidReset() (clears timers, the TASK-1270 failure). Coordinate with TASK-1270 and TASK-1412.10. Update ADR-197 Decision 8.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test per trigger (target, preset, window, HVAC, reset_integral): integral and raw derivative are 0 after it; red on HEAD for the triggers HEAD misses
- [ ] #2 Timers and the initialised flag are not cleared by this reset
- [ ] #3 ADR-197 Decision 8 / open question updated
- [ ] #4 build + evaluate green
<!-- AC:END -->
