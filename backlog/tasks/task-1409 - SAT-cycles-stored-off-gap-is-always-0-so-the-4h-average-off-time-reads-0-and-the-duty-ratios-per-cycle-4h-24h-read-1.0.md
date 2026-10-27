---
id: TASK-1409
title: >-
  SAT cycles: stored off-gap is always 0, so the 4h average off-time reads 0 and
  the duty ratios (per cycle, 4h, 24h) read 1.0
status: To Do
assignee: []
created_date: '2026-10-26T20:12:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
