---
id: TASK-1416
title: >-
  SAT PID: remove the firmware-only integral cap of +-20 and the 10 s minimum
  between updates (entry 75, Q25)
status: To Do
assignee: []
created_date: '2026-10-28T06:45:00Z'
updated_date: '2026-10-28T20:02:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1270
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 580000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 75, Q25: follow thermo-nova). Remove SAT_PID_INTEGRAL_ABS_MAX (SATpid.ino ~199-202), the zone +-20 cap (SATcontrol.ino ~2162-2163), SAT_PID_SAMPLE_TIME_LIMIT and its gate (SATpid.ino ~21, ~299-304). The unchanged-error gate stays with TASK-1368. Update ADR-197 Context 2 / Decision 3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: integral exceeds 20 when thermo-nova's would (red on HEAD, green on fix)
- [ ] #2 Two updates 5 s apart are both processed when the 60 s cadence allows (no 10 s gate)
- [ ] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Execution-graph update requested by Robert: serialize pending SATpid.ino writers after TASK-1368 -> TASK-1270: TASK-1416 -> TASK-1417 -> TASK-1418 -> TASK-1414 -> TASK-1423 -> TASK-1415. TASK-1381 remains a parallel branch after TASK-1270, gated separately by Q53. Revalidate the task premise after the preceding fix; if already addressed, record evidence and re-scope/close through the normal review, rather than applying a duplicate change. Persistence write cadence remains Robert's decision.
<!-- SECTION:NOTES:END -->
