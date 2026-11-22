---
id: TASK-3024
title: >-
  Heap low-water under a /ws stall plus REST load: hd_min_free_heap 116-524 B
  and 503 low-heap on sat/status
status: To Do
assignee: []
created_date: '2026-11-21T19:51:00Z'
updated_date: '2026-11-22T03:15:00Z'
labels:
  - heap
  - websocket
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1138000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen twice on .61 (OTGW32, no PSRAM): (1) beta.413+da42aca: hd_min_free_heap 144 B / max_block 500 already at uptime 0:55 (before the capture), 116 B later (capture e2e 21 Nov, OTGW-field-data/bench-otgw32/2026-11-21_task3006-capture-1c16a65/). (2) TASK-3017 bench beta.416+0bac665: hd_min_free_heap 740 -> 524 B with a stalled /ws reader, a browser page and a WEB CONN CAP 503 at 20:44:16 ('5 connections hold TX (0 ws, cap 4)'); a sat/status request got 503 'low heap' (OTGW-field-data/bench-otgw32/2026-11-21_task3017-0bac665/). logHeapStats around the 3017 stall showed ~47 KB free, so the dips are short, deep and unexplained; hd_min is a monotone watermark, not a trend. Candidates to measure, not conclusions: the per-client WS queue (32 shared buffers per stalled client), chunked REST responses held by connections at the ADR-188 TX cap, MQTT outbox bursts (ADR-266 work).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduction plan executed: 09's load recipe (a stalled /ws reader + a browser page + REST polling at the TX cap) on .61 with a heap capture (hd_min_free_heap, max block, per-event deltas) that reproduces a dip below 2 KB, or shows it does not reproduce
- [ ] #2 The consumer that takes the heap at the dip is named from the capture (allocation site or subsystem), with evidence
- [ ] #3 A fix or a bounded limit keeps hd_min_free_heap above an agreed floor under the same load (floor decided with Robert), red/green on the reproduction
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22 timeline from the TASK-3017 AC3 run (.61, beta.417, info poller 30 s): hd_min_free_heap 4788 after boot -> 2264 (21:55:33, CRITICAL 1) -> 1236 -> 1028 (WiFi drop 21:59) -> 548 -> 200 (22:02:33) -> 192 -> 172 (23:59); CRITICAL entered 10x in 3 h while freeheap read 31-46 KB every 30 s (short deep dips). The dip snapshot (hd_dip_*): free 2404, maxblk 788, ws 0, rest 0, files 0, tx 0, mqtt queue 0, mqtt tx 558 B, 5 TIME_WAIT pcbs: the snapshot names no consumer. The first CRITICAL is at 21:55, before the WiFi drop. The dips trigger the /ws stall of TASK-3017.

2026-11-22: child TASK-3025 (ADR-107 action 1 aborts the /ws TCP instead of a WS close). TASK-3017 bench (4846f20): hd_min_free_heap 784 -> 304 during the A/B/C/D runs; ADR-107 recovery ran at 02:13:24 and 02:38:27 (before=2252), the second closed the healthy client D.

2026-11-22 045e1fd bench: ADR-107 heap recovery fired at 03:21 (before=4180) and 04:08 (before=2868, delta 0) with one or two /ws clients: the dips also occur at low load. Next after TASK-3017 lands, with TASK-3025.
<!-- SECTION:NOTES:END -->
