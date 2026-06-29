---
id: TASK-948
title: >-
  Document session decisions: 4 Proposed ADRs (159-162) + REST API docs for 3
  new endpoints
status: To Do
assignee: []
created_date: '2026-06-29 10:44'
labels:
  - async-esp32s3
dependencies: []
ordinal: 161000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Capture this session's directly-landed architectural decisions as Proposed ADRs (the implement-next-task ADR-eval only covers loop-landed work; everything substantive this session was hand-driven, so it has code but no decision record). Pre-assigned numbers to avoid a 157/158-style collision: ADR-159 symmetric 0x26-WD presence gating (amends ADR-135; TASK-945/946); ADR-160 combo auto board-detect is per-boot + never persists, iBoardMode is manual-override-only, OTGW32 is the no-PIC bench default (amends ADR-127; TASK-947); ADR-161 dedicated BLE roster REST endpoint + write-only-secret contract (TASK-935/946); ADR-162 SAT test-hook policy (force-boiler debug backdoor, TASK-802). All Proposed (maintainer accepts later). A2: document the 3 new endpoints (/api/v2/mqtt/republish, /api/v2/sat/force-boiler, /api/v2/sat/ble/roster) in docs/api/openapi.yaml + README.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-159..162 drafted Proposed, each passing the 4 adr-quality gates (Completeness/Evidence/Clarity/Consistency), correct amends/related links, no number collision
- [ ] #2 docs/api/openapi.yaml + README.md document the 3 new REST endpoints (methods, params, responses, auth, the bindkey write-only note)
- [ ] #3 evaluate.py --quick green (ADR references resolve); docs-only, no firmware change
<!-- AC:END -->
