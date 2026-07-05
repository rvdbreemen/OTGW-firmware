---
id: TASK-1018
title: 'Load-harness: asyncio+httpx semaphore concurrency (API+assets)'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:20'
updated_date: '2026-07-05 22:46'
labels: []
dependencies: []
ordinal: 229000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Python asyncio/httpx harness with semaphore-controlled offered-concurrency. Mix: /api/v2 GETs (device/info, sat/status, health) + PUTs (settings-save pattern) + static assets (v2.html/css/js, ds-tokens.css). Log per-request status/latency + peak client open sockets.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Offered-concurrency exactly controllable (semaphore)
- [x] #2 Mixed GET/PUT/asset workload with per-request latency+status log
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Wrote scripts/loadtest_harness.py (asyncio + httpx, semaphore-bound offered concurrency, deterministic weighted round-robin mix — no randomness so runs are reproducible). Smoke-tested live against 192.168.88.64 (Classic-S3, provisioned in TASK-1016): concurrency=2, 20 requests, 200 across the board, peak-concurrent tracker read back exactly 2 (matches offered-N). NDJSON log verified: GET device/info, sat/status, health, POST settings (ui_onboarded=true, idempotent/safe), GET v2.html/v2.css/ds-tokens.css/v2.js all present with per-request ts/status/latency_ms.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/loadtest_harness.py: host-side load harness for the TASK-1015 parallelism study. asyncio.Semaphore gives exact offered-concurrency control (AC#1, verified: peak-tracker==concurrency on a live smoke run). Mixed workload — 3 REST GETs (device/info, sat/status, health), 1 REST POST (settings, safe idempotent value, mirrors the frontend's postSetting() body shape), 4 static-asset GETs (v2.html/css, ds-tokens.css, v2.js) — logged per-request as NDJSON with status+latency_ms (AC#2). CLI: --host/--concurrency/--duration|--requests/--log. Depends on TASK-1017's gate-cap stats for the next phase (capacity sweep) to read HWM/503 counters back.
<!-- SECTION:FINAL_SUMMARY:END -->
