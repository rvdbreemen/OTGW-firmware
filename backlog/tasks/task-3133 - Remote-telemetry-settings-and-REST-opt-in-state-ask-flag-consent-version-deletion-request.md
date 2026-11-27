---
id: TASK-3133
title: >-
  Remote telemetry: settings and REST (opt-in state, ask flag, consent version,
  deletion request)
status: To Do
assignee:
  - '@otgw-poc-dev'
created_date: '2026-11-27T18:53:00Z'
updated_date: '2026-11-27T19:54:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1184000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owner: otgw-poc-dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 NVS telemetry namespace: optin, offsrc, offrel per ADR-299, plus the consent text version the user agreed to
- [ ] #2 /api/v2/health carries remote_telemetry.ask and the state; POST /api/v2/settings remotetelemetry with source prompt|settings per ADR-299; no settings.ini key
- [ ] #3 A delete-my-data request path as the new ADR decides; host tests red/green on the real policy function
<!-- AC:END -->
