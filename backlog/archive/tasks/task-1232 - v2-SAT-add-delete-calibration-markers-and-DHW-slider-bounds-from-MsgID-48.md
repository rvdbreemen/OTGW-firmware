---
id: TASK-1232
title: 'v2 SAT: add/delete calibration markers and DHW slider bounds from MsgID 48'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:10'
updated_date: '2026-10-15 14:06'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: low
ordinal: 370000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md rows 57, 58: v2 lists markers but cannot add or delete them (sat/markers POST/DELETE); the DHW slider has fixed 40-65 bounds instead of the boiler's MsgID 48 range. Maintainer decided 2026-10-15: keep v2's persistent dhw_setpoint semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 can add a marker by clicking the curve and delete one
- [x] #2 The DHW slider bounds come from MsgID 48 when the boiler reports them, else 40-65
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: a click on the curve added marker id 1 (3.9/52.6), DELETE removed it, and markers are back to []. DHW slider 40-65 because messages/48 has value 0 (bus offline), so the fallback was confirmed; MsgID 48 bounds were shown in the harness. Open: the firmware clamps SATdhwsetpoint to 0..60 while the slider allows 65 (maintainer decision).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 SAT: add/delete calibration markers by clicking the curve; the DHW slider takes MsgID 48 bounds with a 40-65 fallback, and dhw_setpoint stays persistent. Evidence: bench marker add/delete, slider fallback.
<!-- SECTION:FINAL_SUMMARY:END -->
