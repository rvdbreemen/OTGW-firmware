---
id: TASK-193
title: 'SAT fix: PID deadband default mismatch and missing manual gains mode'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:19'
updated_date: '2026-04-09 06:17'
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
- [x] #1 Default deadband in settings is 0.1C matching Python DEADBAND constant
- [x] #2 C++ supports manual gains mode: when bAutoGains is false, use settings fKp/fKi/fKd directly
- [x] #3 Manual gains are passed through to PID output correctly
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Change fDeadband default in OTGW-firmware.h from 0.25f to 0.1f
2. Change SAT_PID_DEADBAND_DEFAULT in SATpid.ino from 0.25f to 0.1f (this constant is currently unused in runtime but documents the intended default)
3. Implement manual gains mode: when settings.sat.bAutoGains is false (need to add this field), use settings.sat.fKp/fKi/fKd directly instead of formula
4. Note: bAutoGains field does not exist yet; need to add it to SATSection in header, add persistence in settingStuff.ino, and branch in _pidCalculateGains() in SATpid.ino
5. Check if this affects SATcontrol.ino only as instructed
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Changed PID deadband default from 0.25C to 0.1C in OTGW-firmware.h (settings default) and
SAT_PID_DEADBAND_DEFAULT in SATpid.ino (documents the intended default), matching Python
const.py DEADBAND=0.1.

Implemented manual gains mode: added bAutoGains (bool, default true), fKpManual (5.0),
fKiManual (0.0005), fKdManual (0.0) to SATSection. When bAutoGains is false,
_pidCalculateGains() sets state.sat.fKp/Ki/Kd from the manual settings instead of the
automatic formula -- matching Python pid.py automatic_gains=False behavior.

New fields exposed in satSendStatusJSON() JSON output and MQTT (sat/auto_gains, sat/kp_manual,
sat/ki_manual, sat/kd_manual).
<!-- SECTION:FINAL_SUMMARY:END -->
