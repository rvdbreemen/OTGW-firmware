---
id: TASK-1210
title: 'v2 web UI bursts REST calls past the 2-request cap, so pages fill with 503s'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-04 12:53'
updated_date: '2026-10-04 21:23'
labels:
  - webui
  - v2
  - rest
dependencies: []
priority: high
ordinal: 331000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Sergeant D (#alpha-testing, 2026-10-03, alpha.404+220fca7 combo, BLE off). The browser console shows many 503 Service Unavailable on /api/v2/settings, sat/markers, sat/weather, sat/status, sat/target, sat/ble/discovery, otdirect/overrides, health, debug, pic/settings, pic/update-check and simulate. The telnet log has 19 'REST BUSY: 2/2 in-flight (cap 2) => 503' lines while freeheap stays at 31-43 KB and the heap prefix never drops below 47816 / 22516, so this is the ADR-165 concurrency gate, not memory pressure.
Cause in code: the classic bundle wraps window.fetch in a FIFO queue with at most 2 /api/ requests in flight (ADR-184, index.js). v2.js has 51 fetch( calls and no such queue (only a device/info dedup at v2.js:2382). CLAUDE.md already notes 'The v2 bundle is not covered'. TASK-1193 fixed the same class of burst on the classic SAT page.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2.js sends at most 2 same-origin /api/ requests at a time (a central queue as in ADR-184), with no Promise.all/map burst left
- [x] #2 Reproduced old vs fix: loading each v2 page (Home, SAT, Monitor, Advanced, Settings) on the bench gives REST 503s on the old build and none on the fix, from the browser network log or hd_rest_503 in /api/v2/device/info
- [ ] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Copy the ADR-184 fetch queue from index.js to the top of v2.js. v2.html does not load index.js, so v2 has no cap today. Same rules: max 2 same-origin /api/ requests, FIFO, slot freed when the body arrived, 20 s stall valve. fetchWithRetry and all 51 fetch calls then go through it. Verify: node unit check with a fake fetch, then a bench old-vs-fix page-load comparison (hd_rest_503 delta per v2 page) after the TASK-1036 soak frees the bench.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-04: queue added to the top of v2.js (working tree, not yet committed, not yet on a device). Evidence so far: node --check OK. A node harness (C:\Users\rvdbr\AppData\Local\Temp/t1210_queue.js) evals the queue IIFE from v2.js against a fake fetch: 8 concurrent /api/ calls give a peak of 2 in flight and 8 served, and the non-API request bypasses the queue. HEAD's v2.js has no queue. evaluate.py --quick 0 failures. Pending: bench AC#2 (old vs fix per v2 page). That needs a filesystem flash, which resets bench settings, and the bench is reserved for the TASK-1036 soak until about 23:10.

Bench 2026-10-04, OTGW32 combo app alpha.412+8ad028c. Only the LittleFS image changed. Playwright probe (.playwright-mcp/t1210_pages.js, not committed) loads /v2.html and opens Home, SAT, Monitor, Settings and Advanced. Per page it records the peak number of concurrent /api/ requests, browser-side 503s and the device hd_rest_503 delta.
- OLD (v2.js without the queue): peak in flight 2/2/3/5/2/4 (start/home/sat/monitor/settings/advanced); browser 503s 1+0+1+3+0+3 = 8; device delta sat 1, advanced 3.
- FIX (queue): peak at most 2 on every page; browser 503s 0; device delta 0 everywhere; window.__otgwApiQueue present.
Node unit check: 8 concurrent calls give a peak of 2 with all served.
<!-- SECTION:NOTES:END -->
