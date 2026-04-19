---
id: TASK-316
title: >-
  [PERF-L2] Gate satControlLoop helpers behind bEnabled check to save cycles
  when SAT off
status: To Do
assignee: []
created_date: '2026-04-18 19:24'
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
- [ ] #1 Bulk of helpers run only when bEnabled is true
- [ ] #2 Only the MQTT-loss fallback detection stays above the gate (entry path)
- [ ] #3 No behavior change when SAT is enabled
<!-- AC:END -->
