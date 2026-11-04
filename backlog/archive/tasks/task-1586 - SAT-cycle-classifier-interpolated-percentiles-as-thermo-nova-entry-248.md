---
id: TASK-1586
title: 'SAT cycle classifier: interpolated percentiles, as thermo-nova (entry 248)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T07:52:00Z'
updated_date: '2026-11-03T09:09:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 731000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 248 (Q-PARITY-1 A, 2026-11-03): the tail percentiles (p10/p50/p90, and the control-error and requested-setpoint percentiles) interpolate between the two neighbouring sorted values as thermo-nova helpers.py:185-202 (percentile_interpolated), instead of the lower neighbour (SATcycles.ino:610, :640, and the TASK-1585 helper).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real SATcycles.ino: percentiles equal helpers.py percentile_interpolated on the same samples; the interp_* parity scenarios of tests/test_sat_classifier_parity.py agree (removed from the DIFFERENCES list); red on the old code
- [x] #2 No RAM change; int16 percentiles keep their hundredths resolution
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead): shared _percentilePos() (integer position math, lerp in the caller's type) for the four tail percentiles (float tail, int16 ctrlErr p50/p90, requested-setpoint p50 and err p90), as helpers.py:185-202; n==1 returns the value; no RAM change. _flowPercentile (whole-cycle p90/p10 for the 4 h window) is not a tail percentile and keeps the lower neighbour (spec says so). test_sat_tail_percentile now uses the real helpers.percentile_interpolated as oracle: old 7 FAIL, fix OK; parity old 5 FAIL, fix OK; cycle_classifier IP1/IP2 old 2 FAIL, fix OK.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The classifier's tail percentiles interpolate as thermo-nova helpers.py percentile_interpolated (entry 248), one shared helper, no RAM change. Evidence: oracle tests against the real upstream function, red on the old code (7+5+2 failures), green on the fix. Not bench-tested.
<!-- SECTION:FINAL_SUMMARY:END -->
