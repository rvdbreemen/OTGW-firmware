---
id: TASK-3124
title: >-
  Web UI: WebSocket reconnect with exponential backoff and jitter, pause after
  repeated flapping, fetchSeed throttled
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T12:48:00Z'
updated_date: '2026-11-27T13:14:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
ordinal: 1033000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
v2.js connectWs/scheduleReconnect retries every fixed 5 s and every close also calls fetchSeed, so a dead or flapping device is hammered (architect proposal, coordinator 2026-11-27). Change: delay 5,10,20,40 s then cap 60 s with +-20 % jitter; the attempt counter resets only when a socket stayed open 60 s; 3 opened-then-closed sockets within 60 s put the page in a paused state (no automatic reconnect, a Reconnect button in the connection detail); fetchSeed from onclose and the 20 s interval at most once per 30 s. Web UI only (data/v2.js, i18n packs). ADR-184 queue untouched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A pure helper gives the delay sequence 5,10,20,40,60,60 s within +-20 % jitter; one-file webui test, red on origin/dev
- [ ] #2 The attempt counter resets only after a socket stayed open for 60 s; a socket that opens and closes at once does not reset it
- [ ] #3 3 opened-then-closed sockets within 60 s: no further automatic reconnect, a Reconnect button resumes; test covers pause and resume
- [ ] #4 fetchSeed runs at most once per 30 s from onclose and the 20 s timer; test counts calls
- [ ] #5 Evidence: before/after reconnect timeline (timestamps of connection attempts) against a refusing endpoint, plus a browser check in headless Chrome; nl/de/el strings for the new text
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence docs/evidence/2026-11-27_webui-3124/: before fixed 5 s gaps; after 4.2/8.6/20.7/44.3 s against a refusing endpoint; paused state + Reconnect button EN/NL screenshots; test ws-reconnect-backoff-3124 8/8, red on origin/dev. Real-device check open (B2/B3 bench).
<!-- SECTION:NOTES:END -->
