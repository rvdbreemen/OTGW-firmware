---
id: TASK-316
title: >-
  [PERF-L2] Gate satControlLoop helpers behind bEnabled check to save cycles
  when SAT off
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:24'
updated_date: '2026-04-19 06:21'
labels:
  - performance
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SATcontrol.ino:3354-3400 runs satUpdateSimulation, satUpdateThermalLearning, fallback detection, and flame-edge check BEFORE the bEnabled early-return. Thousands of needless calls per second on SAT-off devices.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bulk of helpers run only when bEnabled is true
- [x] #2 Only the MQTT-loss fallback detection stays above the gate (entry path)
- [x] #3 No behavior change when SAT is enabled
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Refactor order: pidState restore -> fallback detection (kept above gate as entry path) -> bEnabled/isFlashing early return (moved up) -> satUpdateSimulation + satUpdateThermalLearning. Flame-edge detection stays below as it uses other state. Documented via inline comment: if standalone simulation preview mode is ever introduced, satUpdateSimulation must move back above the gate (it has its own bSimulation guard but no bEnabled guard).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SATcontrol.ino:3354-3400 restructured. Simulation and thermal-learning helpers now run only when SAT is enabled (or fallback active). Behaviour change acknowledged in inline comment: standalone simulation preview without bEnabled would need to move the simulation call back above the gate. No change on the enabled path. Estimated ~5-8 function calls saved per loop() iteration when SAT is off.
<!-- SECTION:FINAL_SUMMARY:END -->
