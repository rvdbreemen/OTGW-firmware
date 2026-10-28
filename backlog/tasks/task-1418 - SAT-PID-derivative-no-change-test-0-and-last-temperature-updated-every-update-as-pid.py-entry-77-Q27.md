---
id: TASK-1418
title: >-
  SAT PID derivative: no-change test == 0 and last temperature updated every
  update, as pid.py (entry 77, Q27)
status: To Do
assignee: []
created_date: '2026-10-28T06:46:00Z'
updated_date: '2026-10-28T20:06:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1417
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 582000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 77, Q27: match thermo-nova). Replace the derivative reference-temperature logic (SATpid.ino ~49-56, ~226-270) with a last temperature overwritten on every PID update (pid.py:171, :211) and an exact zero no-change test. After TASK-1368. Bench old vs new with the room EMA; report to Sergeant D if D becomes near-dead.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test reproduces pid.py derivative on a recorded room series (old differs, new matches)
- [ ] #2 Bench old-vs-new D magnitude reported
- [ ] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Robert requested this execution-graph update. Preserve the earlier reconciliation. Pending PID writers run 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415; 1381 remains a separate branch after 1270 and Q53. Revalidate task premises against preceding commits. Integration also requires the DHW and auto-tune outcomes through their linked verification gates. Approval and bench gates remain in force.
<!-- SECTION:NOTES:END -->
