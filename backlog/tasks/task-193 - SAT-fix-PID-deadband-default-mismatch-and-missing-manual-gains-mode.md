---
id: TASK-193
title: 'SAT fix: PID deadband default mismatch and missing manual gains mode'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:19'
updated_date: '2026-04-09 06:11'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Change fDeadband default in OTGW-firmware.h from 0.25f to 0.1f
2. Change SAT_PID_DEADBAND_DEFAULT in SATpid.ino from 0.25f to 0.1f (this constant is currently unused in runtime but documents the intended default)
3. Implement manual gains mode: when settings.sat.bAutoGains is false (need to add this field), use settings.sat.fKp/fKi/fKd directly instead of formula
4. Note: bAutoGains field does not exist yet; need to add it to SATSection in header, add persistence in settingStuff.ino, and branch in _pidCalculateGains() in SATpid.ino
5. Check if this affects SATcontrol.ino only as instructed
<!-- SECTION:PLAN:END -->
