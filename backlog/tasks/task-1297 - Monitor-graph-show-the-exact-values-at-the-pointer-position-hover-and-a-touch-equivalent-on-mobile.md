---
id: TASK-1297
title: >-
  Monitor graph: show the exact values at the pointer position (hover), and a
  touch equivalent on mobile
status: To Do
assignee: []
created_date: '2026-10-22T11:57:00Z'
labels:
  - webui
  - monitor
  - feature
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 422000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request gruijter 2026-10-22 08:12 (#alpha-testing, screenshot). On hover over the Monitor graph show a popup with the time and the values of the visible (switched-on) series at that x position; on touch, a tap or drag does the same. Keep it client-side (no new requests), use the existing samples and the legend's hidden-series state (gOff), textContent only, and the time axis of TASK-1267 for the x->time mapping.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Hovering the graph shows a readout with time and the values of visible series only; hidden series are omitted (bench screenshot)
- [ ] #2 On a 390 px touch viewport a tap/drag shows the same readout and does not block scrolling (screenshot)
- [ ] #3 A tests/webui test runs the real readout function from v2.js against sample data
<!-- AC:END -->
