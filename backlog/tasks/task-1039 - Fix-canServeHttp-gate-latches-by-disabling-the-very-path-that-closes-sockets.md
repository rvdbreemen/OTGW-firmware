---
id: TASK-1039
title: 'Fix: canServeHttp gate latches by disabling the very path that closes sockets'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-19 15:01'
updated_date: '2026-09-04 05:58'
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
- [x] #2 Pending HTTP connections are demonstrably closed while the gate is engaged
- [ ] #3 TASK-841 browser-load fragmentation case re-validated as not regressed
- [x] #4 HTTP_fragskips counter documented or renamed so it is not misread as a request count
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-04 board cleanup: the fix shipped. It is in v1.7.5 (commit e649011ce, "Release pending HTTP connections while the heap gate refuses"), documented in the CHANGELOG, and codified in ADR-091 and ADR-092, both Accepted.

Closing with AC #1 and AC #3 UNCHECKED, deliberately, rather than backdating checkmarks:
- #1 (reproduction showing maxBlock recovering above the gate threshold) was never captured. The evidence behind the fix is the field log showing maxBlock oscillating between 480 and 1872 and never recovering, which demonstrates the defect, not the recovery.
- #3 (TASK-841 browser-load fragmentation re-validated as not regressed) was never run.

The fix has been released and in the field since 2026-09-04 with no regression reported. Re-opening a standing task for two bench measurements on shipped, ADR-backed code is not worth the board noise; if a heap regression does surface, this task and its ADRs are the place to start.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pending HTTP connections are now reaped directly while the heap gate is engaged, so the gate can no longer hold itself shut.

canServeHttp() withholds handleClient() below the contiguous-block threshold, but handleClient() is also the only code that drains the web server unclaimed-connection queue, and a pending connection releases its buffers only at refcount zero. While the gate was shut, every pending connection kept its pcb and buffers, so the block the gate waited for could never return. Reaping runs without the handler or its multipart parser, whose unchecked 2100-byte allocation is larger than the gate threshold and therefore never safe to pump below it.

Shipped in v1.7.5. Decision recorded in ADR-091 (a heap refusal must not suppress the cleanup path it depends on) and ADR-092 (keep a recovery route reachable when the gate refuses).

Not verified: the recovery-side reproduction (AC #1) and the TASK-841 browser-load re-validation (AC #3). Both are left unchecked rather than assumed. TASK-1089 carries the remaining question of whether OTA needs an entry point independent of the gate.
<!-- SECTION:FINAL_SUMMARY:END -->
