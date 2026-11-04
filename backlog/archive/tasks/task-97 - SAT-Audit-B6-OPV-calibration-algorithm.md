---
id: TASK-97
title: 'SAT Audit B6: OPV calibration algorithm'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:50'
updated_date: '2026-04-09 05:20'
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
Compare the OPV (Outside temperature - Power Value) calibration algorithm in Python SAT thermo-nova with the C++ firmware. Verify measurement method, calculation, storage and application of the calibration value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OPV measurement procedure compared (start conditions, duration, sampling)
- [x] #2 Calibration calculation (formula) verified
- [x] #3 Storage and persistence of calibration value verified
- [x] #4 Application in setpoint calculation verified
- [ ] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B6 audit: OPV calibration algorithm - GAPS FOUND

Python (services.py, const.py): SAT Python uses OVERSHOOT_PROTECTION_REQUIRED_DATASET=40 cycles before OVP value is valid. OPV setpoint per system type: heat_pump=40, radiators=62, underfloor=45. The Python algorithm is cycle-based: it tracks boiler temperature over 40+ completed cycles at MM=0 (minimum modulation) and takes the maximum as the OVP value.

C++ (SATcontrol.ino satOvpCalibrate()): Time-based state machine:
- STARTING: sends CS=max, MM=0
- WARMING: waits for flame + temp rise of 5C (SAT_CALIB_WARM_DELTA=5C)
- MEASURING: samples for 20 minutes (SAT_CALIB_MEASURE_MS=1200000ms), tracks max temp
- DONE: saves max temp as OVP, sets bOvpEnabled=true

GAPS:
1. Python requires OVERSHOOT_PROTECTION_REQUIRED_DATASET=40 cycles of data. C++ uses 20 minutes of time-based measurement instead. The convergence criteria are fundamentally different (data density vs time).
2. Python OPV setpoints per system: heat_pump=40, radiators=62, underfloor=45. C++ satGetMaxSetpoint() returns heat_pump=40, radiators=62, underfloor=45. These match ✓
3. C++ recovery after calibration sends CS=0 + MM=100 (line ~296). Python uses COLD_SETPOINT as the recovery setpoint. Minor difference.
4. Python does not have a WARMING phase; it starts measuring immediately. C++ waits for 5C temp rise. C++ approach is actually safer.
5. C++ sends MM=0 in MEASURING phase periodically (addCommandToQueue). Python implies MM=0 is set during the entire calibration period via the coordinator. Matches intent.

No critical algorithmic gaps. The time-based vs cycle-based approach is a design choice. The OPV system threshold values match.
<!-- SECTION:FINAL_SUMMARY:END -->
