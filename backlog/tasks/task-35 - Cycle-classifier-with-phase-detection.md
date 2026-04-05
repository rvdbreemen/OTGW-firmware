---
id: TASK-35
title: Cycle classifier with phase detection
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:52'
updated_date: '2026-04-05 23:34'
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
- [x] #1 Classify cycle phases: startup, steady-state, cooldown
- [x] #2 Track phase transitions and durations
- [x] #3 Cycle history with per-phase statistics
- [x] #4 MQTT publish: current cycle phase and duration
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cycle phase detection: startup, steady-state, cooldown, idle.\n\nChanges:\n- Phase detected from flow temp vs setpoint band during active cycle\n- Cooldown set on flame-off transition\n- REST: cycle_phase (string), phase_duration_sec\n- MQTT: sat/cycle_phase\n\nFiles: SATcycles.ino, SATcontrol.ino
<!-- SECTION:FINAL_SUMMARY:END -->
