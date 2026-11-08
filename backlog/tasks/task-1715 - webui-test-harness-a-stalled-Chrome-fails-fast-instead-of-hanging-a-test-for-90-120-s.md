---
id: TASK-1715
title: >-
  webui test harness: a stalled Chrome fails fast instead of hanging a test for
  90-120 s
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T00:24:00Z'
updated_date: '2026-11-08T00:37:00Z'
labels:
  - tests
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 824000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tests/webui/lib/page-harness.mjs send() waits forever for a CDP reply, and until() counts loop iterations, not time. When headless Chrome stalls under load (seen 2026-11-07/08 at 84-92% commit charge), a test hangs until the node test timeout (90-120 s) and fails with no hint; the untouched theme-toggle-labels test hung the same way (rc=124, 153 s). Found while verifying TASK-1700.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 send() rejects with the CDP method name after a bounded time; until() is bounded by wall time
- [x] #2 A harness self-test shows a stalled CDP call fails within the bound with a clear message (red on the old harness: it hangs), and the existing webui tests stay green
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: tests/webui/lib/page-harness.mjs send() now rejects after CDP_TIMEOUT_MS (20 s, OTGW_CDP_TIMEOUT_MS) with 'CDP <method> got no reply within N ms (Chrome stalled?)'; until() is bounded by wall time (default 15 s, one final check unless the last call timed out). AC2: new tests/webui/harness-stall.test.mjs stalls the real renderer with for(;;){}: old harness FAIL 'evalJs still hangs after 30013 ms on a stalled renderer'; fix PASS in 8.6 s (evalJs rejects naming Runtime.evaluate within the 3 s test timeout; until(...,1000) returns within budget). All 7 page-harness users green on the fix (connection-map 3/3, diag-recording 3/3, harness-stall 1/1, home-heatpump 3/3, log-follow 2/2, settings-reboot-badge 1/1, theme-toggle 4/4). Found on the way: connection-map-text-fits ignored until()'s result and relied on the old loop stretching while a cold Chrome was slow (failed at 1343 px on the fix, passed on the old harness); it now asserts both waits with 20 s; other callers that ignore the result are covered by the 15 s default (the old effective default was 5 s plus the stretch). Not changed: tests with their own CDP code (not using page-harness).
<!-- SECTION:FINAL_SUMMARY:END -->
