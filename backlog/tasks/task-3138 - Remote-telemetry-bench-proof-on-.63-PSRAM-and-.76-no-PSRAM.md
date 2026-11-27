---
id: TASK-3138
title: 'Remote telemetry: bench proof on .63 (PSRAM) and .76 (no PSRAM)'
status: To Do
assignee:
  - '@otgw-bench-tester'
created_date: '2026-11-27T19:10:00Z'
updated_date: '2026-11-27T19:54:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1188000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owner: otgw-bench-tester.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 .63: off after the update; on via the dialog; uploads arrive at the collection point; a block pauses with back-off; switch-off stops at once; 60 min soak, heap flat, no restart
- [ ] #2 .76: no dialog, row disabled, an on-write ignored, no TLS attempt (telnet and counters)
- [ ] #3 A recorded sample checked against the never-collected list
<!-- AC:END -->
