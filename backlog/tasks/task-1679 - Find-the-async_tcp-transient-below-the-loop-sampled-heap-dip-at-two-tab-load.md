---
id: TASK-1679
title: Find the async_tcp transient below the loop-sampled heap dip at two-tab load
status: To Do
assignee: []
created_date: '2026-11-07T04:05:00Z'
labels:
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 801000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1673 (ADR-216). With the per-connection send cap, the native minimum free heap is still about 10 KB below what the loop task samples at the dip (one run 3252 B). That transient sits on the async_tcp task outside the lwIP send queues (candidates from the TASK-1673 research: the 2872 B ESPAsyncWebServer response copy buffer plus pbuf copies during tcp_write, WiFi TX/RX buffers, request/header objects, 13 TIME_WAIT pcbs ~3.2 KB).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Measure the transient on the async_tcp task (a heap low-water hook or sampling inside the response path) during the two-tab load
- [ ] #2 Decide with Robert whether a reduction is worth its cost, with the numbers
<!-- AC:END -->
