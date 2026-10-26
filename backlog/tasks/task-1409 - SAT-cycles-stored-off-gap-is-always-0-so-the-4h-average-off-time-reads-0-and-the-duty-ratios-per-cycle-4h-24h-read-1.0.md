---
id: TASK-1409
title: >-
  SAT cycles: stored off-gap is always 0, so the 4h average off-time reads 0 and
  the duty ratios (per cycle, 4h, 24h) read 1.0
status: To Do
assignee: []
created_date: '2026-10-26T20:12:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
ordinal: 547000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-26 while answering Alexw (flame tracking), second reader confirmed: the off-gap stored with each cycle record is always 0, so reporting shows avg off-time 0 and duty ratio 1.0 (per cycle, 4h window, 24h window). Reporting only; no control path reads it. See workflow wf_4f8a262f-33d research 'flame-tracking' for the exact lines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Off-gap per cycle = time from the previous flame-off to this flame-on (host test red on HEAD)
- [ ] #2 4h average off-time and duty ratios show real values on a replay or bench run
<!-- AC:END -->
