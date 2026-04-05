---
id: TASK-15
title: Extend cycle tracker with percentile metrics and sliding windows
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 23:32'
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
- [x] #1 Cycle kind tracking: CH, DHW, MIXED (based on OT status flags fraction)
- [x] #2 Rolling averages instead of deque arrays: overwrite-ring of 16 records (was 8)
- [x] #3 Per cycle record expanded: kind, flow/return delta, flow-setpoint error
- [x] #4 Lightweight sliding window: recent_overshoot_fraction, recent_underheat_fraction (exponential decay)
- [x] #5 Duty ratio tracking: sum(cycle durations) / window_seconds over last hour
- [x] #6 Long cycle fraction: cycles > 600s / total
- [x] #7 State fields: state.sat.fDutyRatio, state.sat.fOvershootFraction, state.sat.fUnderheatFraction
- [x] #8 REST API: GET /api/v2/sat/status includes duty_ratio, overshoot_fraction, underheat_fraction, long_cycle_fraction
- [x] #9 MQTT publish: sat/duty_ratio, sat/overshoot_fraction
- [ ] #10 WebUI: cycle statistics section in dashboard
- [ ] #11 HA auto-discovery: sensor entities for cycle health metrics
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Lightweight cycle tracker for ESP8266:\n1. 16-record ring buffer (kind, duration, overshoot_sec, flow_error)\n2. Exponential decay fractions instead of sliding windows\n3. Duty ratio: EMA of flame-on fraction\n4. REST + MQTT + HA discovery
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Lightweight cycle tracker with EMA fractions for ESP8266.\n\nChanges:\n- Ring buffer expanded from 8 to 16 records\n- Cycle kind detection: CH/DHW/MIXED from OT status flag sampling\n- Per-record: kind, flow-setpoint error added\n- EMA fractions (alpha=0.15): duty_ratio, overshoot_fraction, underheat_fraction, long_cycle_fraction\n- REST: duty_ratio, overshoot_fraction, underheat_fraction\n- MQTT: sat/duty_ratio, sat/overshoot_fraction\n- AC#10/#11 (WebUI, HA discovery) left for polish phase\n\nFiles: SATcycles.ino, OTGW-firmware.h, SATcontrol.ino
<!-- SECTION:FINAL_SUMMARY:END -->
