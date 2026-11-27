---
id: TASK-3132
title: >-
  Remote telemetry: merge the recorder from lane-poc-merge into dev (normal
  build, PSRAM gate, experimental)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-27T18:48:00Z'
updated_date: '2026-11-27T19:53:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1183000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owner: otgw-dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The remote telemetry recorder and uploader from lane-poc-merge are in the normal esp32-combo build on dev; no HAS_CLOUD_REC-only env needed
- [ ] #2 On a board without PSRAM no TLS handshake, no ring in PSRAM, no upload; an on-write is ignored (ADR-218, ADR-299); host test of the gate red/green
- [ ] #3 Off by default after an update and on a fresh install; host test
- [ ] #4 diagRec removed in the same change (ADR-268, TASK-3046/2984)
- [ ] #5 App image size reported against the slot it ships in (old 2240K or 2304K) and under the gate in force; evaluate.py --quick no new FAIL
<!-- AC:END -->
