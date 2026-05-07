---
id: TASK-563
title: 'fix(webui): dedup WebSocket reconnect on initOTLogWebSocket'
status: To Do
assignee: []
created_date: '2026-05-07 17:48'
updated_date: '2026-05-07 18:01'
labels:
  - webui
  - websocket
  - frontend
  - 2.0.0
dependencies: []
priority: medium
ordinal: 26000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Frontend reconnect logic does not deduplicate against an in-flight handshake. SergeantD's browser console (alpha.3, 2026-05-07) shows initOTLogWebSocket() invoked from at least three call sites (updateOTLogResponsiveState @ 1541, initMainPage @ 3248, anonymous IIFE @ 1126) within the same second on page load — Connect attempts #1 and #2 fire within ~1 s of each other, and the attempt counter climbs to 30+ within ~4 minutes with most attempts failing code=1006. Independent of any server-side issue (TASK-529), this guarantees thrash and amplifies the visible 1006 storm. JS-only fix: refuse re-entry while readyState === CONNECTING || OPEN; cancel pending reconnect timer when a new init is requested only if the current socket is closed or errored. Cross-ref: TASK-529 server-side latency, TASK-431 WebUI freeze.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 initOTLogWebSocket() refuses to open a new WebSocket while the existing one is in CONNECTING or OPEN state; it returns early with a debug log line instead
- [ ] #2 If readyState is CLOSING or CLOSED, the helper proceeds to open a new socket as today
- [ ] #3 Pending reconnect timers are cancelled deterministically on every entry; no overlap of two scheduled reconnects
- [ ] #4 Verified by browser console capture: page load + 5 min idle on the OT log page produces at most 1 in-flight WS handshake at any time and connect-attempt counter increments only on actual disconnects
- [ ] #5 No regression on the OT log live-stream behaviour; existing reconnect-on-disconnect timing (5 s backoff) preserved
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 3** of the SergeantD-driven 2.0.0 fix sequence. **Re-scope required before coding**: data/index.js:1556 already has readyState gates at lines 1540, 1609, 1670, 1584-1587 — the remaining defect is more subtle. Read lines 530-700 + 1530-1700 in full; identify the precise call site that bypasses dedup OR the timer-vs-explicit-init race. Update ACs to match the actual gap.
<!-- SECTION:NOTES:END -->
