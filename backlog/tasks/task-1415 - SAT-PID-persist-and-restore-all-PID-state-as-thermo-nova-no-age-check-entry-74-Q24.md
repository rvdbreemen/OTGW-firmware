---
id: TASK-1415
title: >-
  SAT PID: persist and restore all PID state as thermo-nova, no age check (entry
  74, Q24)
status: To Do
assignee: []
created_date: '2026-10-28T06:45:00Z'
updated_date: '2026-10-28T21:26:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1423
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
  - docs/design/pid-checkpoint-ten-year-spec.md
modified_files:
  - docs/design/pid-checkpoint-ten-year-spec.md
priority: medium
ordinal: 579000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 74, Q24: follow thermo-nova). Save integral, last error, raw derivative, last temperature and derivative time; restore all with no 30-min age check (pid.py:141-157). Store the derivative time as wall-clock time. Handle last error NAN from satPidCurveOnly (entry 46). Write cadence vs LittleFS wear is a maintainer call (ask Robert in the plan). Depends on TASK-1368. Update ADR-197 Decision 7.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: save then restore round-trips all five fields; a file older than 30 min is still restored
- [ ] #2 NAN last error is saved and restored without corrupting the file
- [x] #3 Write cadence decided by Robert and recorded in the task
- [ ] #4 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Execution-graph update requested by Robert: serialize pending SATpid.ino writers after TASK-1368 -> TASK-1270: TASK-1416 -> TASK-1417 -> TASK-1418 -> TASK-1414 -> TASK-1423 -> TASK-1415. TASK-1381 remains a parallel branch after TASK-1270, gated separately by Q53. Revalidate the task premise after the preceding fix; if already addressed, record evidence and re-scope/close through the normal review, rather than applying a duplicate change. Persistence write cadence remains Robert's decision.

2026-10-28: Robert requested a handoff implementation specification for a ten-year PID checkpoint service target after read-only USB inspection and LittleFS wear experiments. A storage specification is being attached at docs/design/pid-checkpoint-ten-year-spec.md. This session writes documentation only; TASK-1415 remains To Do, its dependencies and SAT writer ordering are preserved, and firmware implementation/verification remain for the receiving session. Cadence and storage choices in the specification are proposed defaults for the requested handoff, not a new Sergeant D decision.

2026-10-28 handoff specification written and independently reviewed: docs/design/pid-checkpoint-ten-year-spec.md. Proposed design: one dedicated-directory atomic binary snapshot, 52 B main-only or 180 B main plus four zones; five-minute ordinary attempt interval; full TASK-1415 field/time/NaN semantics; ten-year qualification ceilings of 40000 added erases per sector for the declared PID workload and 50000 for the bounded combined LittleFS workload. The spec contains concrete migration/reset decision points, operating assumptions, failure/OTA limits and acceptance tests. The earlier 62-byte simulation does not qualify this new format. Documentation-only work; no firmware implementation, build or new hardware validation was performed in this handoff turn. Task remains To Do pending its existing predecessor chain and required maintainer decisions.

2026-10-28 maintainer decision (Robert, in session): write cadence: write the PID checkpoint only when a stored value has changed (no fixed interval). Read with docs/design/pid-checkpoint-ten-year-spec.md, which proposed a 5-minute maximum. Risk reported to Robert in the session: with the 60 s PID (TASK-1368) integral, error and temperature change on almost every step while heating, so 'only on change' is close to one write per minute during the heating season; the spec's 62-byte simulation (1538 max erases per 100k updates, dedicated dir + tmp/rename) puts that near or above the 50,000-erase ten-year budget. The endurance gate of the spec (section 10) must be measured on the final record before Done.
<!-- SECTION:NOTES:END -->
