---
id: TASK-1226
title: 'v2: graph series parity and a time-based 24 h buffer'
status: To Do
assignee: []
created_date: '2026-10-15 05:07'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 364000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md row 28: classic graphs 10 series plus Dallas over 24 h (graph.js); v2 graphs 4 series with 1800 samples per frame change, so the 24 h chip cannot hold 24 h on a busy bus.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 graph offers flame, DHW mode, CH mode, modulation, control setpoint, boiler, return, room setpoint, room, outside and Dallas series
- [ ] #2 The buffer is time-based so the 24 h window holds 24 h of data at a bounded memory cost (measured in the browser)
- [ ] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->
