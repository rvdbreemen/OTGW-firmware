---
id: TASK-1414
title: >-
  SAT PID: reset integral and raw derivative on every target, preset, window and
  HVAC change as thermo-nova (entry 73, Q23)
status: To Do
assignee: []
created_date: '2026-10-28T06:44:00Z'
updated_date: '2026-10-28T20:00:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1270
  - TASK-1418
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency replaced by TASK-1270: entry 73 Open says the reset must not reintroduce the TASK-1270 first-tick behaviour; own host-test AC#1.

Execution-graph update requested by Robert: serialize pending SATpid.ino writers after TASK-1368 -> TASK-1270: TASK-1416 -> TASK-1417 -> TASK-1418 -> TASK-1414 -> TASK-1423 -> TASK-1415. TASK-1381 remains a parallel branch after TASK-1270, gated separately by Q53. Revalidate the task premise after the preceding fix; if already addressed, record evidence and re-scope/close through the normal review, rather than applying a duplicate change. Persistence write cadence remains Robert's decision.
<!-- SECTION:NOTES:END -->
