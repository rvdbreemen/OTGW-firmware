---
id: TASK-591
title: >-
  feat(sat): replace raw-max zone aggregation with 75th-percentile plus
  overshoot cap for multi-zone SAT
status: To Do
assignee: []
created_date: '2026-05-08 16:50'
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
- [ ] #1 Zone aggregation in satZonePidStep uses P75 of active zone outputs plus 5 degree headroom
- [ ] #2 Overshoot cap computed when any zone temperature exceeds its setpoint; cap applied to aggregate output
- [ ] #3 Single-zone behavior unchanged (P75 of one value = that value)
- [ ] #4 New setting sat.fZoneAggregationHeadroom with default 5.0 degrees
- [ ] #5 Backward compat: falls back to max when fewer than 2 zones active
<!-- AC:END -->
