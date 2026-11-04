---
id: TASK-1019
title: 'Browser page-load harness (CDP headless-Edge, cold+warm)'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:20'
updated_date: '2026-07-05 22:48'
labels: []
dependencies: []
ordinal: 230000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Drive real page-load via CDP on headless Edge, navigate by IP. Cold (empty cache) + warm (304). Measure total page-load, per-asset parallel connections, DOMContentLoaded, 503 count in network log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cold + warm page-load measured with per-asset parallel-connection count
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Wrote scripts/loadtest_pageload.py: raw CDP-websocket harness (websocket-client, no selenium/playwright), reuses the same CDP protocol pattern already proven in scripts/capture-mqtt-debug.bat's browser-capture runspace. Cold=fresh --user-data-dir; warm=second navigation in a primed profile. peak_parallel_connections computed via a real sweep-line over each request's [start_ts,end_ts] CDP timeline interval (not a request-count proxy). Live-tested against 192.168.88.64: cold 13 requests/peak-3-parallel/2601ms, warm 9 requests (4 served from disk cache)/peak-3-parallel/2073ms — matches Edge's expected HTTP/1.1 per-origin connection cap and shows the warm-cache win. count_503 field ready for the Phase-1 capacity sweep (TASK-1022).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/loadtest_pageload.py drives a real headless-Edge page load via raw CDP (Network/Page domains) against http://<host>/v2.html, navigating by IP per project convention. Reports wall_elapsed_ms, requests_finished, peak_parallel_connections (true interval-overlap sweep, not a proxy), status_distribution incl. count_503, and cache-hit count, for both a cold profile and a warm (cache-primed) profile in one run. Verified live on 192.168.88.64.
<!-- SECTION:FINAL_SUMMARY:END -->
