---
id: TASK-1669
title: Free heap drifts ~500 B/h on the Classic+S3 during the beta.95 24 h soak
status: To Do
assignee: []
created_date: '2026-11-05T23:33:00Z'
labels:
  - heap
  - soak
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 792000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1544 soak on .63 (Classic + S3 Mini, PSRAM, MQTT connected, beta.95+1fbd5cf, boot 148): freeheap 54,176 B at 19:13 -> 51,572 B at 00:28 (2026-11-05/06), a steady ~500 B/h decline; maxfreeblock flat at 31,732, hd_min_free_heap 20,760 -> 19,736. No restart. At this rate ~12 KB/day: a slow leak or a growing structure (graph history ring is in PSRAM there, so not that). Sampler log: lead scratchpad soak24-b95/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Full 24 h curve evaluated (linear vs levelling off) when the soak ends ~2026-11-06 15:00
- [ ] #2 If linear: the growing allocation identified (heap tracing or per-consumer counters) and fixed, red/green
<!-- AC:END -->
