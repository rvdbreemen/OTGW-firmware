---
id: TASK-1307
title: >-
  Attribute the 3 s loop gap and the uptime lag seen on the 2.0.0 bench during
  replay
status: To Do
assignee: []
created_date: '2026-10-23T06:28:00Z'
labels:
  - heap
  - bench
  - loop
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 430000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen during TASK-1296 bench runs (OTGW32 no PSRAM, alpha.461 working tree, replay active): hd_max_loop_gap_ms 3038-3110 ms, and state.uptime lagging the wall clock (5:05 reported after ~6 h; doTaskEvery1s() ticks are skipped when loop() stalls, while millis()-clocked code such as the graph history ring stays right). Only new builds were read, so the graph history (graphHistoryTick, /api/v2/graph/history) is not yet ruled out as a cause.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A/B on the same bench unit with the same replay, about 15 min each, telnet z to zero the watermark first: hd_max_loop_gap_ms and uptime drift recorded on alpha.461 (before TASK-1296) and alpha.462
- [ ] #2 The source of the >1 s loop stall is named with evidence (telnet timing or a capture), or recorded as pre-existing with a follow-up task
<!-- AC:END -->
