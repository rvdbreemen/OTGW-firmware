---
id: TASK-223
title: 'SAT: Implement stalled-ignition adaptive threshold'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:29'
updated_date: '2026-04-09 06:24'
labels:
  - audit-fix
  - sat
  - device-status
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - src/OTGW-firmware/SATcontrol.ino
  - backlog/docs/sat-python-cpp-mapping.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's DeviceStatusEvaluator.is_ignition_stalled() uses the duration of the last completed cycle to set the stall detection threshold: if the last cycle ran for X seconds, the boiler has proven it can complete a cycle in X seconds, so waiting longer than ~2x that before flagging stall makes sense.

The C++ firmware uses a fixed 600s (10 minute) threshold (BS_STALLED_IGNITION_MIN_MS). For boilers with short cycles (e.g. PWM mode with 3 min on-time) this is far too generous: a real ignition failure would take 10 minutes to detect. For boilers with long cycles (heat pumps, underfloor) 600s may be too short.

Complexity: Low-medium. Requires storing the last completed cycle duration and using it to compute the adaptive threshold, floored at a minimum (e.g. 120s) and capped at a maximum (e.g. 900s).

Risk: Low. The existing fixed 600s is a safe fallback if no previous cycle duration is available (cold start).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Last completed CH cycle duration is tracked in state.sat
- [x] #2 Stalled ignition threshold is computed as max(last_cycle_duration * 1.5, 120s), capped at 900s
- [x] #3 Falls back to fixed 600s when no previous cycle duration is available
- [x] #4 Threshold is recalculated on each new cycle completion
- [x] #5 No regression on STALLED_IGNITION detection in normal operation
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace the static _bs_lastCycleDurationMs tracking in satUpdateBoilerStatus() with state.sat.fLastCycleDuration (already set by SATcycles.ino in seconds).
2. Rewrite the stall threshold calculation per AC: stalledThreshold = max(last_cycle_duration_ms * 1.5, 120000), capped at 900000ms. Fall back to 600000ms (BS_STALLED_IGNITION_MIN_MS) when fLastCycleDuration == 0.
3. Remove the static _bs_lastCycleDurationMs variable (no longer needed) and the manual assignment in the flame-off tracking block.
4. Add named constants for the adaptive threshold minimum (120s) and maximum (900s).
5. Verify no regression: STALLED_IGNITION still fires at the right time in normal operation.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced the fixed-plus-ad-hoc stalled ignition threshold in satUpdateBoilerStatus() with a proper adaptive formula matching Python parity.

Changes:
- Removed static _bs_lastCycleDurationMs variable; the boiler status evaluator now reads state.sat.fLastCycleDuration (seconds, set by SATcycles.ino) directly.
- Added BS_STALLED_IGNITION_FLOOR_MS (120s) and BS_STALLED_IGNITION_CAP_MS (900s) constants.
- Stall threshold = max(last_cycle_duration * 1.5, 120s), capped at 900s; falls back to 600s (BS_STALLED_IGNITION_MIN_MS) on cold start when no prior cycle exists.
- Threshold is recalculated on each call to satUpdateBoilerStatus() using the latest state.sat.fLastCycleDuration, which is updated on every cycle completion.

No change to STALLED_IGNITION detection logic or any other boiler status path.
<!-- SECTION:FINAL_SUMMARY:END -->
