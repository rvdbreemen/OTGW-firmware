---
id: TASK-3134
title: 'Remote telemetry: consent dialog and Settings row in the web UI'
status: To Do
assignee:
  - '@otgw-webui'
created_date: '2026-11-27T18:58:00Z'
updated_date: '2026-11-27T19:54:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1185000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owner: otgw-webui.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Standalone consent dialog on existing installs and in the wizard, text as approved (ADR-299 decision 4) with its version shown, NL/DE/EL translations
- [ ] #2 Settings > System > Remote telemetry row: switch, status, pseudonymous ID, link to what is collected, delete request; disabled with an explanation on a board without PSRAM
- [ ] #3 Experimental label visible; screenshots before/after with synthetic data
<!-- AC:END -->
