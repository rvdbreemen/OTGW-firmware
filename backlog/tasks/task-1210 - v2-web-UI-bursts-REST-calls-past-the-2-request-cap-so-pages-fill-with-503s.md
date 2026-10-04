---
id: TASK-1210
title: 'v2 web UI bursts REST calls past the 2-request cap, so pages fill with 503s'
status: To Do
assignee: []
created_date: '2026-10-04 12:53'
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
- [ ] #1 v2.js sends at most 2 same-origin /api/ requests at a time (a central queue as in ADR-184), with no Promise.all/map burst left
- [ ] #2 Reproduced old vs fix: loading each v2 page (Home, SAT, Monitor, Advanced, Settings) on the bench gives REST 503s on the old build and none on the fix, from the browser network log or hd_rest_503 in /api/v2/device/info
- [ ] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
