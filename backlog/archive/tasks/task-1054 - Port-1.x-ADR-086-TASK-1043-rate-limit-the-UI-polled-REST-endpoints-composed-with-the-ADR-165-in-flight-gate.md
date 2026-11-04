---
id: TASK-1054
title: >-
  Port 1.x ADR-086/TASK-1043: rate-limit the UI-polled REST endpoints, composed
  with the ADR-165 in-flight gate
status: Done
assignee: []
created_date: '2026-07-31 19:52'
updated_date: '2026-07-31 20:27'
labels: []
dependencies: []
ordinal: 249000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x commit 6828a0cc8 + ADR-086 (v1.7.2). dev restAPI.ino has 429 handling only on the discovery/republish cooldown; the UI-polled endpoints are unlimited. Needs a NEW dev ADR (next free number >= 170) rather than a copy of 1.x ADR-086: dev already has a throttle story in ADR-165 (REST_MAX_INFLIGHT/WEB_FILE_MAX_INFLIGHT capped at 2, tightening under heap pressure), so the dev ADR must state how a per-endpoint 1 req/s rate limit COMPOSES with the in-flight gate, not merely restate the 1.x reasoning. ORDERING: land TASK-1053 (client poll cut) first and confirm the UI is quiet, otherwise layering a 429 on top of a UI still polling at 1 Hz produces a self-inflicted error storm on first flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New dev ADR authored via adr-kit, citing 1.x ADR-086 and explaining composition with ADR-165
- [ ] #2 ADR passes the four adr-kit gates and is Accepted before the implementation lands
- [ ] #3 UI-polled endpoints return 429 with a retry hint above the configured rate
- [ ] #4 TASK-1053 is Done and verified quiet before this task starts implementation
- [ ] #5 build.bat green for esp32 target and evaluate.py --quick clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SUPERSEDED BY TASK-1037, not implemented separately. Created 2026-07-31 from an analysis of a local dev tree that was 6 commits behind origin/dev. origin/dev already carried 24be052f2 'feat(2.0.0): port 1.7.2-beta.4 hardening', which ported this change as ADR-172, adapted to the 2.0.0 architecture and with evaluate.py gates plus unit tests. Verified against the merged tree, not against the task description. No work remains here.
<!-- SECTION:NOTES:END -->
