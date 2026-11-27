---
id: TASK-3124
title: >-
  Web UI: WebSocket reconnect with exponential backoff and jitter, pause after
  repeated flapping, fetchSeed throttled
status: Done
assignee:
  - '@claude'
created_date: '2026-11-27T12:48:00Z'
updated_date: '2026-11-27T22:34:00Z'
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
- [x] #1 A pure helper gives the delay sequence 5,10,20,40,60,60 s within +-20 % jitter; one-file webui test, red on origin/dev
- [x] #2 The attempt counter resets only after a socket stayed open for 60 s; a socket that opens and closes at once does not reset it
- [x] #3 3 opened-then-closed sockets within 60 s: no further automatic reconnect, a Reconnect button resumes; test covers pause and resume
- [x] #4 fetchSeed runs at most once per 30 s from onclose and the 20 s timer; test counts calls
- [x] #5 Evidence: before/after reconnect timeline (timestamps of connection attempts) against a refusing endpoint, plus a browser check in headless Chrome; nl/de/el strings for the new text
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence docs/evidence/2026-11-27_webui-3124/: before fixed 5 s gaps; after 4.2/8.6/20.7/44.3 s against a refusing endpoint; paused state + Reconnect button EN/NL screenshots; test ws-reconnect-backoff-3124 8/8, red on origin/dev. Real-device check open (B2/B3 bench).

UX review: paused row got the explanation sentence (en/nl/de/el), primary 44 px Reconnect button, 390 px screenshots EN+NL in the evidence folder.

2026-11-28 otgw-webui: all five ACs hold on origin/dev 8886aacbf. tests/webui/ws-reconnect-backoff-3124.test.mjs 8/8 (fresh run this session, Chrome 0 after). Delay sequence, 60 s reset rule, pause after 3 opened-then-closed sockets with Reconnect button, fetchSeed once per 30 s: covered by that test. Headless Chrome timeline before/after (fixed 5 s forever vs 5 10 20 40 60 with jitter) and EN/NL screenshots at 1400 and 390 px: docs/evidence/2026-11-27_webui-3124/README.md. nl/de/el strings in the packs. Landed on dev inside B2 (7d2063993). Not verified: a real device (bench browser check).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WebSocket reconnect backs off 5 10 20 40 then 60 s with +-20 % jitter, the attempt counter resets after a socket stayed open 60 s, three sockets that opened and closed within 60 s pause the page until the Reconnect button, fetchSeed from onclose and the 20 s timer runs at most once per 30 s. Evidence: ws-reconnect-backoff-3124.test.mjs 8/8 on origin/dev 8886aacbf, headless Chrome timeline and screenshots in docs/evidence/2026-11-27_webui-3124. Not verified on a real device.
<!-- SECTION:FINAL_SUMMARY:END -->
