---
id: TASK-89
title: 'SAT Audit A2: Python-to-C++ function mapping'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:49'
updated_date: '2026-04-09 05:28'
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
Map every Python class/function from the SAT integration (thermo-nova branch) to its C++ equivalent in the firmware. Note where mapping is missing or deviates.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every Python class mapped to a C++ file/function (or marked as missing)
- [x] #2 Mapping table published in backlog/docs/sat-function-mapping.md
- [ ] #3 Follow-up fix tasks created with label 'audit-fix' for each missing mapping
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed full Python-to-C++ function mapping across all SAT modules.

Published: backlog/docs/sat-python-cpp-mapping.md

Coverage: 13 mapping sections
- PID (pid.py -> SATpid.ino): 12 functions, 10 FULL, 2 MISSING (state persistence)
- Heating curve (heating_curve.py -> SATcontrol.ino): 5 mappings, mostly FULL
- PWM (pwm.py -> SATcontrol.ino): 10 mappings, 6 FULL, 3 PARTIAL, 1 MISSING (restore)
- Heating control (heating_control.py -> SATcontrol.ino): 10 mappings, 4 FULL, 5 PARTIAL, 1 MISSING
- Cycle tracking (cycles/ -> SATcycles.ino): 12 mappings, 2 FULL, 7 PARTIAL, 3 MISSING
- Device tracking (device/ -> SATcontrol.ino): 10 mappings, 3 FULL, 6 PARTIAL, 1 MISSING
- Manufacturer (manufacturer.py -> SATcontrol.ino): 7 mappings, 6 FULL, 1 PARTIAL
- Solar gain (solar_gain.py -> SATcontrol.ino): 8 mappings, 4 FULL, 4 PARTIAL
- Summer Simmer (summer_simmer.py): all MISSING - no C++ equivalent
- Multi-area (area.py): all MISSING - no C++ equivalent
- Pressure health (binary_sensor.py): all MISSING - no C++ equivalent
- OPV calibration (state.py -> SATcontrol.ino): 7 mappings, 5 FULL, 2 PARTIAL, 1 MISSING
- Weather/BLE: FULL with minor PARTIAL notes

AC #3 (follow-up fix tasks) handled by TASK-90.
<!-- SECTION:FINAL_SUMMARY:END -->
