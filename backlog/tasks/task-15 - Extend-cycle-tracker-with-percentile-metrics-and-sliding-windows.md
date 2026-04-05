---
id: TASK-15
title: Extend cycle tracker with percentile metrics and sliding windows
status: To Do
assignee: []
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 10:23'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python has an extensive cycle tracker with: (1) CycleShapeMetrics: time_in_band, total_overshoot_seconds, time_to_first_overshoot, max_flow_control_setpoint_error. (2) Sliding windows: recent (4h) and daily (24h) with percentile calculations (p50, p90). (3) Cycle kind detection: CENTRAL_HEATING, DOMESTIC_HOT_WATER, MIXED, UNKNOWN. (4) Tail window: last 180s of a cycle for classification. The current port only has basic classification. For the ESP, the full sliding window approach is too heavy on RAM. Implement a lightweight variant with rolling averages instead of percentile arrays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Cycle kind tracking: CH, DHW, MIXED (based on OT status flags fraction)
- [ ] #2 Rolling averages instead of deque arrays: overwrite-ring of 16 records (was 8)
- [ ] #3 Per cycle record expanded: kind, flow/return delta, flow-setpoint error
- [ ] #4 Lightweight sliding window: recent_overshoot_fraction, recent_underheat_fraction (exponential decay)
- [ ] #5 Duty ratio tracking: sum(cycle durations) / window_seconds over last hour
- [ ] #6 Long cycle fraction: cycles > 600s / total
- [ ] #7 State fields: state.sat.fDutyRatio, state.sat.fOvershootFraction, state.sat.fUnderheatFraction
- [ ] #8 REST API: GET /api/v2/sat/status includes duty_ratio, overshoot_fraction, underheat_fraction, long_cycle_fraction
- [ ] #9 MQTT publish: sat/duty_ratio, sat/overshoot_fraction
- [ ] #10 WebUI: cycle statistics section in dashboard
- [ ] #11 HA auto-discovery: sensor entities for cycle health metrics
<!-- AC:END -->
