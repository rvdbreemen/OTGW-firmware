---
id: TASK-3136
title: >-
  Remote telemetry: user text and docs (what is sent, retention, deletion,
  opt-out)
status: To Do
assignee: []
created_date: '2026-11-27T19:04:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1187000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owner: otgw-docs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A docs page in plain words: what is collected, what never, where it is stored (EU, self-hosted), 30 days retention, how to delete and opt out, experimental, PSRAM boards only
- [ ] #2 Release notes line for the beta; consent dialog links to the page; tests/test_docs_no_em_dash.py green
<!-- AC:END -->
