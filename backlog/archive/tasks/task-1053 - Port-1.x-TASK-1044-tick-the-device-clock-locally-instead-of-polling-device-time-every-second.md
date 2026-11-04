---
id: TASK-1053
title: >-
  Port 1.x TASK-1044: tick the device clock locally instead of polling
  /device/time every second
status: Done
assignee: []
created_date: '2026-07-31 19:51'
updated_date: '2026-07-31 20:27'
labels: []
dependencies: []
ordinal: 248000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x commit 38bea0f22 (v1.7.2), scoped to dev by inspection. Classic index.js startTimeUpdates() (index.js:318) runs refreshDevTime() every 1000ms, and refreshDevTime() (index.js:4737) does a real fetch of /api/v2/device/time on every tick. startOTmonitorPolling() (index.js:305) fetches at 1 Hz as well. refreshGatewayMode() is already counter-throttled, so it is NOT part of this port. v2.js needs NO change: its tick() (v2.js:156) is already a pure local clock with no fetch, so only the classic UI is affected. Fix: fetch device time occasionally, advance the displayed clock locally between fetches. This matters more on dev than on 1.x because dev enforces a hard N<=2 in-flight gate (ADR-165) that answers bursts with a 503.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 index.js advances the clock display locally each second and fetches /api/v2/device/time only on a slow interval plus at page activation
- [ ] #2 OTmonitor poll rate reduced from 1 Hz
- [ ] #3 v2.js left untouched, with the reason recorded in the task notes
- [ ] #4 no more than 2 concurrent fetches at any time (ADR-165)
- [ ] #5 build.bat green (filesystem image rebuilt, not just firmware)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SUPERSEDED BY TASK-1037, not implemented separately. Created 2026-07-31 from an analysis of a local dev tree that was 6 commits behind origin/dev. origin/dev already carried 24be052f2 'feat(2.0.0): port 1.7.2-beta.4 hardening', which ported this change as ADR-173, adapted to the 2.0.0 architecture and with evaluate.py gates plus unit tests. Verified against the merged tree, not against the task description. No work remains here.
<!-- SECTION:NOTES:END -->
