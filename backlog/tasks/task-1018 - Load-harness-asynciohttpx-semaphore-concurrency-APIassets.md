---
id: TASK-1018
title: 'Load-harness: asyncio+httpx semaphore concurrency (API+assets)'
status: To Do
assignee: []
created_date: '2026-07-05 22:20'
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
- [ ] #1 Offered-concurrency exactly controllable (semaphore)
- [ ] #2 Mixed GET/PUT/asset workload with per-request latency+status log
<!-- AC:END -->
