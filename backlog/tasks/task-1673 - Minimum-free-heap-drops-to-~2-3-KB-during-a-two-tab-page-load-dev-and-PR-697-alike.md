---
id: TASK-1673
title: >-
  Minimum free heap drops to ~2-3 KB during a two-tab page load (dev and PR #697
  alike)
status: To Do
assignee: []
created_date: '2026-11-06T10:57:00Z'
labels:
  - heap
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 796000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .61 2026-11-06: hd_min_free_heap went to 2612 B (dev beta.106+069ae27, after three headless-Chrome page loads of the asset script) and to 3296/3172 B (PR #697 build beta.107+742f969, two-tab load test: 2 parallel tabs x 7 assets x 25 rounds, all 350 requests 200). maxfreeblock stays 31,732; free heap recovers to ~68-71 KB after. A dip this deep is within a few KB of an allocation failure. Not PR-specific (both builds). Scripts: session scratchpad b103/twotab_load.py, queue_burst.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Find what holds the memory at the minimum (per-connection AsyncTCP/lwIP buffers, response objects, JSON writers, WebSocket) with a heap trace or per-consumer counters during a two-tab load
- [ ] #2 Raise the minimum to a safe margin (target >= 8 KB, the HEAP_LOW level) under the same two-tab load, red/green on the bench
<!-- AC:END -->
