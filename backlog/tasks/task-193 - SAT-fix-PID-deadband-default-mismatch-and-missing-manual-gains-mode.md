---
id: TASK-193
title: 'SAT fix: PID deadband default mismatch and missing manual gains mode'
status: To Do
assignee: []
created_date: '2026-04-09 05:19'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python pid.py uses DEADBAND=0.1C (hardcoded in const.py). C++ SATpid.ino defines SAT_PID_DEADBAND_DEFAULT=0.25C. The settings-based deadband should default to 0.1C to match Python. Also, Python PID supports manual gains (automatic_gains=False -> use configured proportional/integral/derivative directly). C++ _pidCalculateGains() always uses automatic formula regardless. Manual gains mode should be implemented.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Default deadband in settings is 0.1C matching Python DEADBAND constant
- [ ] #2 C++ supports manual gains mode: when bAutoGains is false, use settings fKp/fKi/fKd directly
- [ ] #3 Manual gains are passed through to PID output correctly
<!-- AC:END -->
