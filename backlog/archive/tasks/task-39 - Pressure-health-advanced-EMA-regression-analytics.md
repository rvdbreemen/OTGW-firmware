---
id: TASK-39
title: 'Pressure health: advanced EMA + regression analytics'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:05'
updated_date: '2026-04-06 10:41'
labels:
  - sat
  - feature
  - safety
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py:200-389
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement advanced pressure health monitoring with EMA smoothing (alpha=0.05), linear regression drop-rate detection, 600s settle delay after boiler start, and 120s problem confirmation window. This goes beyond basic threshold monitoring to detect slow leaks and pressure decay patterns.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 EMA smoothing with alpha=0.05 applied to pressure readings
- [x] #2 Linear regression calculates pressure drop rate over sliding window
- [x] #3 600s settle delay after boiler start before analysis begins
- [x] #4 120s confirmation window before declaring pressure problem
- [x] #5 Pressure health binary sensor exposed via MQTT
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read existing pressure monitoring code in SATcontrol.ino
2. Add linear regression for drop rate (sliding window of recent samples)
3. Add 600s settle delay after boiler start
4. Add 120s confirmation window
5. Update EMA alpha from current value to 0.05
6. Expose as MQTT binary sensor
7. Commit and report
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented all five features:
- EMA alpha=0.05 was already correct, kept as-is
- Replaced simple delta drop rate with linear regression over 12-sample ring buffer (~6 min at 30s intervals)
- Added 600s settle delay after flame-on using satCycleGetFlameOnStartMs(); resets ring buffer during settle
- 120s confirmation window was already present, enhanced to properly toggle bPressureHealthy
- Added sat/pressure_health MQTT binary sensor (ON=healthy, OFF=problem) with retained flag

Memory cost: ~64 bytes for ring buffer (12 floats) + 48 bytes (12 uint32_t timestamps) = ~112 bytes static
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Advanced pressure health analytics with linear regression.

Changes:
- 12-sample ring buffer with linear regression for drop rate (was 2-point delta)
- 600s settle delay after flame start (pressure fluctuates during boiler startup)
- 120s confirmation window before alarm
- EMA alpha=0.05 (verified correct)
- MQTT binary sensor sat/pressure_health (ON/OFF)
- ~112 bytes static RAM cost
<!-- SECTION:FINAL_SUMMARY:END -->
