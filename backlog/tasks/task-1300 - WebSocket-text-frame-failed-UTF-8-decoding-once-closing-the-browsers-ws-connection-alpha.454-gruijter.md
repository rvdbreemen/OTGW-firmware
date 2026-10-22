---
id: TASK-1300
title: >-
  WebSocket text frame failed UTF-8 decoding once, closing the browser's /ws
  connection (alpha.454, gruijter)
status: To Do
assignee: []
created_date: '2026-10-22T12:01:00Z'
labels:
  - websocket
  - webui
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 425000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
browser.log 2026-10-22 11:19:05.567: 'WebSocket connection to ws://10.0.40.73/ws failed: Could not decode a text frame as UTF-8' (one occurrence in 1 h 44). A WS text frame carried a byte sequence that is not valid UTF-8, so Chrome closed the socket; the page reconnects, but the live log/graph lost the frames in between. Find which sender can emit non-UTF-8 bytes on the WS text path (PIC byte stream verbatim per ADR-177 into the OT log line? a Dallas label? event_report?) and either send such frames as binary or sanitise at the WS boundary without touching the verbatim PIC path. Reproduce with a replay line containing a 0x80-0xFF byte.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The byte source is identified with evidence (replay or capture) and recorded
- [ ] #2 A replay with a non-UTF-8 byte no longer closes the browser WS connection (bench, browser console)
- [ ] #3 ADR-177 (verbatim PIC stream) stays intact on the serial/log path
<!-- AC:END -->
