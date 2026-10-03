---
id: TASK-1195
title: >-
  SAT PID derivative timer differs from pid.py, so room-temperature steps can
  skip the filter
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-02 19:48'
updated_date: '2026-10-03 07:19'
labels:
  - sat
  - pid
  - bug
dependencies: []
priority: medium
ordinal: 317000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D asked in #alpha-testing (2026-10-02) whether SAT's derivative smoothing filter is also in the firmware. The filter math in SATpid.ino `_pidUpdateDerivative()` matches pid.py `_update_derivative()`: deadband freeze, 60 s minimum interval, ±5 cap before and after filtering, alpha = dt/(60+dt). The timer does not match.

pid.py:
- takes dt from the sensor's own `last_changed` (area.py:128, climate.py:314);
- runs the no-change branch before the 60 s check;
- so dt is the interval between two room-temperature changes.

Firmware:
- takes dt from millis() at each PID update and checks the 60 s first;
- satPidUpdate() runs at least every 60 s while the error is unchanged (control interval 30 s by default);
- each idle update hits the no-change branch and moves the timer to now;
- a step first seen less than 60 s after that returns early;
- satPidUpdate() then stores the new temperature as `_pid_lastRoomTemp`, so that step never reaches the filter;
- steps that do get through use dt of about 60 s instead of the real interval.

Found by reading the code (SATpid.ino:161-211 and 215-275, pid.py:199-232). Not reproduced yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host harness calls the real satPidUpdate() with a stepped room temperature (0.1 °C every 5 min, 30 s control interval) and reports, on the current code, how many steps update the filtered derivative and with which dt
- [ ] #2 After the fix the same harness shows every step that comes more than 60 s after the previous one reaching the filter, with dt within one control interval of the interval between the two changes (the firmware sees a change at its next control tick)
- [ ] #3 Harness cases keep the pid.py behaviour for the deadband freeze, the ±5 cap and alpha = dt/(60+dt)
- [ ] #4 esp32-combo build and evaluate.py green. On the OTGW32 bench, with the external room temperature raised by 0.1 °C every 150 s through POST /api/v2/sat/externaltemp, the mean raw_derivative over steps 9-16 stays below 60% of the true slope on the old build and reaches at least 85% on the fix, as the harness predicts (OLD 25-49%, FIX 92-98%)
- [ ] #5 An error of exactly 0.1 (target 21.0, room 20.9) counts as inside the deadband, because pid.py rounds TemperatureState.error to 3 decimals: the primary integral keeps growing, the primary derivative freezes, and the zone integral keeps growing. Harness B1, B1d and B2 fail on the old code and pass on the fix
- [ ] #6 Through the TASK-894 room EMA (the device path), the mean rawDerivative of a 0.1 °C per 5 min heat-up is within 10% of pid.py on the raw sensor, and a sensor that changes every 30 s gives the true slope. Harness E1 and R1 fail on the old code and pass on the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. SATpid.ino: add a sensor-side change stamp, _pid_roomTempChangedMs plus _pid_roomTempSeen. It is set at the top of satPidUpdate(), before any early return, when the room temperature differs by 0.001 or more from the last value seen. It plays the role of HA's last_changed in pid.py. satPidReset() leaves it alone, the same way pid.py reset() keeps its timers.
2. Init block: _pid_lastDerivativeMs = _pid_roomTempChangedMs, so after a reset dt still spans the real interval.
3. _pidUpdateDerivative(): use the stamp instead of millis(), in the pid.py order: deadband freeze, then no change, then the 60 s check. The freeze, cap and alpha lines stay textually the same, so the mutants MF, MC and MA still apply.
4. AC#2 sharpened: dt within one control interval of the real interval. The firmware sees a change at its next control tick; pid.py has HA's exact time.
5. Proof: test_sat_pid_derivative.py --old-rev HEAD. FIX passes every check. OLD keeps the rules (C1, C2, C3, C5a, C6, C7, H1) and shows the defect. Each mutant fails its own case.
6. Same commit: bump to alpha.403, build.bat esp32-combo, evaluate.py --quick, and update c4-code-sat.md if it describes the timer. AC#4 on the bench: app-only flash of the OTGW32, then a SAT simulation heat-up.
7. P1 (the float compare at error 0.1): separate task, not in this fix.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 REPRODUCED 2026-10-03 on the current code (SATpid.ino at HEAD febf7f4b4, alpha.402).
Harness: test/host/test_sat_pid_derivative.{cpp,py}. It compiles the real SATpid.ino (whole file), satGetEffectiveHeatingSystem() (SATcontrol.ino) and the real SATtypes.h (defaults fDeadband 0.1, iControlInterval 30, bAutoGains 1). A pid.py transcription runs next to it as a reference oracle, not as code under test: it gets the sensor's last_changed (climate.py:314) and is called every 60 s, like control_pid (climate.py:713).
Command: python test/host/test_sat_pid_derivative.py --report worktree
Scenario: 40 steps of +0.1 C every 300 s, step phase swept 0..29 s, loop latency off and on (60 runs, 39 judged steps each).
- Steps that reached the filter: min 9, max 26 of 39.
- dt the filter used: 60.0 .. 150.0 s; the room changed every 300 s.
- Final rawDerivative: firmware -0.000833 .. -0.001650; pid.py -0.000333 in every run, so the firmware is 2.5x to 5x larger.
- D term, both with Kd 11760 (coeff 1.5, curve 40, radiators; pid.py kd = 0.07*8400*kp, the same formula): firmware -9.8 .. -19.4 C, pid.py -3.9 C.
- C4: the first step out of the deadband ran with dt 60 s, pid.py 300 s.
- C6b: the first step after satPidReset() was swallowed.
- C5b: with the target moving every tick, 10..22 of 39 steps reached the filter.
The pid.py rules still hold on the current code: C1 (deadband freeze), C2/C2b (cap), C3 (alpha), C5a (a target change does not move D), C6 (no spike after reset), C7, and H1 (dt recovered from the filter output, exact on 90 s and 110 s).
Side finding P1 (report only, out of scope here): at error 21.0f-20.9f = 0.1000004 the firmware treats the error as OUTSIDE the deadband: the integral reset to 0. pid.py rounds the error to 3 decimals (TemperatureState.error) and keeps it inside.
<!-- SECTION:NOTES:END -->
