---
id: TASK-1696
title: >-
  Web heap: favicon from PROGMEM and at most one large file stream at a time
  (TASK-1679 option 3)
status: To Do
assignee: []
created_date: '2026-11-07T16:52:00Z'
labels:
  - webserver
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 811000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1679 (Robert 2026-11-07). The deepest two-tab dips (8-12 KB free on .61, beta.123) have 2 file streams in flight and an empty MQTT outbox, so they are the web path: per stream about 3.1 KB lwIP TX pbufs, 1.45 KB LittleFS chain, ~2 KB request/response objects, plus the 2.9 KB _send_buffer per _ack. Serve /favicon.ico (3627 B) from PROGMEM with a long Cache-Control (no LittleFS open, no file-gate slot), and admit at most one large body stream (e.g. > 16 KB) at a time while small files may run beside it. The earlier 40 KB free-heap threshold (beta.120, reverted) caused 12 s timeouts: use the file size at admission, not a heap threshold. Touches ADR-165 (cap 2) and ADR-194 (static assets from flash).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Two-tab A/B on .61 (5 runs, reboot each): deepest dip higher than beta.123 (8.1-18.8 KB) with 0 timeouts and p95 within 0.5 s of beta.123
- [ ] #2 Host fixture: the large-stream rule red on old code, green on fix
- [ ] #3 Favicon served from PROGMEM with the same bytes as the file
<!-- AC:END -->
