---
id: TASK-1039
title: 'Fix: canServeHttp gate latches by disabling the very path that closes sockets'
status: To Do
assignee: []
created_date: '2026-07-19 15:01'
labels: []
dependencies: []
priority: high
ordinal: 158000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Evidence from martreides 1.7.1 captures (TASK-1037). canServeHttp() (helperStuff.ino:1118) skips httpServer.handleClient() once maxBlock drops below HTTP_SERVE_MIN_MAXBLOCK (2048). handleClient() is also the only code that accepts, services and closes pending HTTP connections, so while the gate is closed their lwIP pcbs and receive buffers stay allocated. In otgw-171-2.log maxBlock then oscillates between 480 and 1872 and never returns above 2048 for the rest of the run: the gate never reopens. HTTP_fragskips climbs 279 -> 4430 -> 9678 -> 14902 in roughly 40 seconds, which is loop ticks at the ~1 kHz delay(1) cadence, not requests.

The gate was calibrated in TASK-841 against browser-load fragmentation, where skipping serving does let the heap coalesce because the browser stops issuing new requests. It appears not to hold when the pressure comes from somewhere else: skipping then removes the only cleanup path while the real consumer keeps allocating.

Scope: determine whether the gate should still drain and close connections while refusing to serve bodies (accept + close, or handleClient with a refusing handler), or whether it needs an escape hatch that forces one serve pass after N consecutive skips so sockets get reaped. Must not regress the TASK-841 browser-load case.

Note: raising HTTP_SERVE_MIN_MAXBLOCK is not the fix, it makes the latch engage earlier.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduction showing maxBlock recovering above the gate threshold after the gate engages, in a run where it currently never recovers
- [ ] #2 Pending HTTP connections are demonstrably closed while the gate is engaged
- [ ] #3 TASK-841 browser-load fragmentation case re-validated as not regressed
- [ ] #4 HTTP_fragskips counter documented or renamed so it is not misread as a request count
<!-- AC:END -->
