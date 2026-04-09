---
id: TASK-90
title: 'SAT Audit A3: Identify unported features'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:49'
updated_date: '2026-04-09 05:31'
labels:
  - audit
  - sat
  - phase-1
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Identify all features present in the Python SAT integration (thermo-nova branch) that are not yet implemented in the C++ firmware. Use the A1 inventory as reference.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 List of missing features with priority and impact established
- [x] #2 Per missing feature: description, complexity and risk documented
- [x] #3 Follow-up implementation tasks created for critical missing features
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed identification of unported SAT features and created audit-fix tasks for all critical missing features.

Analysis basis: backlog/docs/sat-feature-completeness-matrix.md (91 features) + backlog/docs/sat-python-cpp-mapping.md

Feature categorization:
- MISSING (14 features): portable algorithm logic not yet in firmware
- PARTIAL (16 features): ported but with meaningful gaps
- N/A (7 features): HA-only, intentionally not ported

Audit-fix tasks created (label: audit-fix):
- TASK-222 (HIGH): PID state persistence across restarts - integral term lost on reboot
- TASK-223 (MED): Stalled ignition adaptive threshold - fixed 600s vs Python's adaptive last-cycle-duration
- TASK-224 (MED): OPV calibration minimum dataset (40 samples) - no quality gate in C++
- TASK-225 (MED): Cycle classifier flow temperature tail percentile (p90 vs max_flow)
- TASK-226 (HIGH): Pressure health monitoring - boiler pressure not monitored at all
- TASK-227 (LOW): Rolling windowed cycle statistics (4h/24h) replacing/complementing EMA
- TASK-228 (LOW): Heating curve recommendation (INCREASE/DECREASE/HOLD)
- TASK-229 (MED): PWM minimum ON time 30s -> 180s to match Python HEATER_STARTUP_TIMEFRAME

Deliberately NOT creating audit-fix tasks for:
- Multi-area PID: complex HA-side feature with no clear ESP8266 equivalent (no multiple thermostat inputs)
- Summer Simmer Index: nice-to-have, requires humidity sensor (not universally available via OT)
- Boiler power/gas consumption estimation: requires additional config data, low heating quality impact

These three remain documented in the completeness matrix as MISSING with notes.
<!-- SECTION:FINAL_SUMMARY:END -->
