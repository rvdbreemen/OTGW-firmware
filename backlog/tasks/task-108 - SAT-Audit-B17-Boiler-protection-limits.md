---
id: TASK-108
title: 'SAT Audit B17: Boiler protection limits'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:32'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare boiler protection limits in Python SAT thermo-nova with C++ firmware. Verify max CH setpoint, max flow temperature, boiler protection guards and how these are read/applied via OpenTherm.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Max CH setpoint comparison (OT MsgID 57) verified
- [x] #2 Max flow temperature limit compared
- [x] #3 Boiler protection guards compared
- [x] #4 Reading of boiler limits via OT verified
- [x] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read Python coordinator.maximum_setpoint and helpers.calculate_default_maximum_setpoint
2. Read C++ satGetMaxSetpoint() and hard max constants
3. Compare OT MsgID 57 reading and application
4. Check boiler protection guards
5. Document gaps and create fix tasks
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Max CH setpoint (OT MsgID 57): C++ reads OTcurrentSystemState.MaxTSet from MsgID 57. Used as primary ceiling in satControlLoop. Falls back to SAT_MAX_SETPOINT_DEFAULT=75C when MaxTSet<30C. Python coordinator reads MaxTSet via pyotgw and exposes as coordinator.maximum_setpoint.

2. Max setpoint defaults by system:
   - Heat pump: Python 55C, C++ 40C. C++ more conservative (correct for heat pumps)
   - Underfloor: Python 50C, C++ 45C + hard cap 50C. C++ slightly more conservative
   - Radiators: Python 55C, C++ 62C. C++ allows 7C more. Fix task TASK-230 created.

3. Hard max safety ceiling: C++ SAT_HARD_MAX_FLOOR=50C (underfloor), SAT_HARD_MAX_RAD=80C (radiators/other). Python has no equivalent firmware-level hard max - relies on user config and boiler protection.

4. Min setpoint: Both Python MINIMUM_SETPOINT=10C and C++ SAT_MIN_SETPOINT=10C. MATCH.

5. Emergency cutoffs: C++ has bSafetyTripped (10 consecutive invalid room temps), summer mode CS=10+CH=0, valve-closed CS=10+CH=0. Python relies on HA event system - no equivalent firmware emergency cutoff.

6. Boiler protection guards: C++ skips control on fault_ind (SlaveStatus&0x01). Python event-driven so no equivalent needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B17: Boiler protection limits

OT MsgID 57 (MaxTSet) is read and applied correctly in both Python and C++. Min setpoint 10C matches exactly.

One notable gap: C++ radiators system max is 62C vs Python 55C default. Fix task TASK-230 created for review/alignment.

C++ has additional protections Python lacks: hard max ceilings (50C underfloor, 80C radiators), safety trip mechanism, and fault-flag-triggered control skip.

Heat pump and underfloor defaults in C++ are more conservative than Python (40C vs 55C, 45C vs 50C) - appropriate for these system types.
<!-- SECTION:FINAL_SUMMARY:END -->
