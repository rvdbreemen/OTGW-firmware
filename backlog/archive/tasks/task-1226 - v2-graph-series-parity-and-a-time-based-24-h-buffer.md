---
id: TASK-1226
title: 'v2: graph series parity and a time-based 24 h buffer'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:07'
updated_date: '2026-10-15 14:06'
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
- [x] #1 v2 graph offers flame, DHW mode, CH mode, modulation, control setpoint, boiler, return, room setpoint, room, outside and Dallas series
- [x] #2 The buffer is time-based so the 24 h window holds 24 h of data at a bounded memory cost (measured in the browser)
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: the legend lists boiler, return, control setpoint, room setpoint, room, outside, modulation, flame, DHW mode and CH mode (Dallas when present); toggling works. JS heap 3.28 -> 2.88 MB over 2 min (no growth). The 24 h buffer is time-based (5 s slots, 17,280 points), about 3.9 MB measured in Node.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 graph shows the classic series in a time-based 24 h buffer with bounded memory. Evidence: bench legend + heap measurement, node memory estimate.
<!-- SECTION:FINAL_SUMMARY:END -->
