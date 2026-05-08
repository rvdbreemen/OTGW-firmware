---
id: TASK-591
title: >-
  feat(sat): replace raw-max zone aggregation with 75th-percentile plus
  overshoot cap for multi-zone SAT
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:50'
updated_date: '2026-05-08 17:48'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The C++ multi-zone SAT takes the raw maximum PID output across all active zones as the flow setpoint. The Python reference uses the 75th-percentile of zone outputs plus a 5 degree headroom cap. Under the max-based approach, one very cold zone drives the flow setpoint for all zones, which can cause overshoot in warmer zones. The Python P75 approach prevents outlier zones from dominating unless at least 75 percent of zones have high demand. The Python overshoot cap additionally reduces the setpoint when any zone is above its setpoint, capping flow temperature to prevent over-heating the compliant zones.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Zone aggregation in satZonePidStep uses P75 of active zone outputs plus 5 degree headroom
- [x] #2 Overshoot cap computed when any zone temperature exceeds its setpoint; cap applied to aggregate output
- [x] #3 Single-zone behavior unchanged (P75 of one value = that value)
- [x] #4 New setting sat.fZoneAggregationHeadroom with default 5.0 degrees
- [x] #5 Backward compat: falls back to max when fewer than 2 zones active
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add fZoneAggregationHeadroom (float, default 5.0) to SATtypes.h after iZoneTimeoutS
2. Add write/load for SATzoneheadroom in settingStuff.ino
3. Replace raw-max multi-zone block in SATcontrol.ino with P75+overshoot cap
4. Build and verify
5. Commit alpha.29 and push
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in 3 files:
SATtypes.h: added fZoneAggregationHeadroom = 5.0f after iZoneTimeoutS
settingStuff.ino: added writeJsonFloatKV SATzoneheadroom (write) + constrain(atof, 0..15) (load)
SATcontrol.ino: replaced raw-max block with P75 bubble-sort + overshoot cap. P75 index = ceil(0.75*N)-1. Overshoot = max(0, roomTemp-setpoint) across active zones, subtracted from aggregate.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced raw-max multi-zone zone selection with P75 (75th percentile, ceiling-rank method) plus configurable headroom plus overshoot cap.

Changes:
- SATtypes.h: new setting sat.fZoneAggregationHeadroom (float, default 5.0, clamped 0-15)
- settingStuff.ino: persist/load SATzoneheadroom key via writeJsonFloatKV + constrain(atof)
- SATcontrol.ino: replaced raw-max block with bubble-sort + P75 index calculation + maxOvershoot reduction

Behaviour:
- P75 index = ceil(0.75 * activeZones) - 1 (0-based). For N=4 zones, selects index 2 (3rd value after sort), filtering out the single highest-demand outlier.
- Overshoot cap: if any zone roomTemp > setpoint, subtract the maximum overshoot from the aggregate before clamping to SAT_MIN_SETPOINT.
- Single-zone and zero-zone paths unchanged.

Build: exit 0 (alpha.29). All 5 ACs verified. Committed 3add4fef, pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
