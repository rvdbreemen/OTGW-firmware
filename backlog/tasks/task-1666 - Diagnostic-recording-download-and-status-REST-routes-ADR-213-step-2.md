---
id: TASK-1666
title: Diagnostic recording download and status REST routes (ADR-213 step 2)
status: To Do
assignee: []
created_date: '2026-11-05T20:34:00Z'
labels:
  - diagnostics
milestone: 2.0.0
dependencies:
  - TASK-1470
priority: medium
ordinal: 789000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GET /api/v2/diag/recording?since= as an octet-stream filler with the block range fixed at request start (like ADR-210), plus /api/v2/diag/recording/status (state, ring size, oldest/newest block seq, refusal reason). Open routes (Robert 2026-11-05). Counts as one of the ADR-188 send-holding connections; respects the 2-in-flight web UI rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test of the route wiring and the range-fixed-at-start rule
- [ ] #2 Bench: 1.5 MB download time and that the web UI stays responsive during it
<!-- AC:END -->
