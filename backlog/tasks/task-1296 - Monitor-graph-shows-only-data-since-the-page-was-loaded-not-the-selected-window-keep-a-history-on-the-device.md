---
id: TASK-1296
title: >-
  Monitor graph shows only data since the page was loaded, not the selected
  window; keep a history on the device
status: To Do
assignee: []
created_date: '2026-10-22T11:57:00Z'
labels:
  - webui
  - monitor
  - feature
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 421000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Report gruijter 2026-10-22 08:20 (#alpha-testing, screenshot): with the 1 h window selected the graph (and the CSV export) only holds data from the moment the page was opened or refreshed. Cause: the v2 graph samples are collected client-side from the WebSocket stream (v2.js pushSample() ~549, SAMPLES_MAX 1800) and there is no history endpoint (TASK-986 design note). Proposal: a small device-side ring buffer (for example 1 h at 10 s = 360 samples of flow/return/control setpoint/thermostat setpoint/room/outside/modulation/flame/DHW/CH, ~20 B each, ~7 KB RAM, static per ADR-053) exposed as GET /api/v2/graph/history, loaded once when the Monitor page opens and merged with the live samples; the CSV export then covers the same window. Keep the 2-in-flight rule (one request). Record the RAM and the heap impact on the bench.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Opening the Monitor graph after the device has run for an hour shows the whole selected window (bench screenshot before/after)
- [ ] #2 CSV export covers the same window as the graph
- [ ] #3 RAM/heap before and after measured on the bench; the buffer is static (ADR-053) and bounded
- [ ] #4 docs/api openapi + ch03/h03 updated; a host test covers the history endpoint JSON shape
<!-- AC:END -->
