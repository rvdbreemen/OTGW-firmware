---
id: TASK-223
title: 'SAT: Implement stalled-ignition adaptive threshold'
status: To Do
assignee: []
created_date: '2026-04-09 05:29'
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
- [ ] #1 Last completed CH cycle duration is tracked in state.sat
- [ ] #2 Stalled ignition threshold is computed as max(last_cycle_duration * 1.5, 120s), capped at 900s
- [ ] #3 Falls back to fixed 600s when no previous cycle duration is available
- [ ] #4 Threshold is recalculated on each new cycle completion
- [ ] #5 No regression on STALLED_IGNITION detection in normal operation
<!-- AC:END -->
