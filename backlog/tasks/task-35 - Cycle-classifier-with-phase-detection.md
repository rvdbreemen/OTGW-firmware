---
id: TASK-35
title: Cycle classifier with phase detection
status: To Do
assignee: []
created_date: '2026-04-05 20:52'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Classify heating cycles into phases (startup, steady-state, cooldown) and track cycle history with statistics. Extends Task #15 (cycle tracker percentiles) with phase-aware classification.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/cycles/classifier.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Classify cycle phases: startup, steady-state, cooldown
- [ ] #2 Track phase transitions and durations
- [ ] #3 Cycle history with per-phase statistics
- [ ] #4 MQTT publish: current cycle phase and duration
<!-- AC:END -->
