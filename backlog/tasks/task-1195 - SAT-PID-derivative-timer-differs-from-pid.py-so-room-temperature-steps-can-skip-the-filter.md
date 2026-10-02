---
id: TASK-1195
title: >-
  SAT PID derivative timer differs from pid.py, so room-temperature steps can
  skip the filter
status: To Do
assignee: []
created_date: '2026-10-02 19:48'
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
- [ ] #1 A host harness calls the real satPidUpdate() with a stepped room temperature (0.1 °C every 5 min, 30 s control interval) and reports, on the current code, how many steps update the filtered derivative and with which dt
- [ ] #2 After the fix the same harness shows every step that comes more than 60 s after the previous one reaching the filter, with dt equal to the interval between the two changes
- [ ] #3 Harness cases keep the pid.py behaviour for the deadband freeze, the ±5 cap and alpha = dt/(60+dt)
- [ ] #4 esp32-combo build and evaluate.py green; on the bench a SAT simulation heat-up shows the raw derivative changing on room-temperature steps
<!-- AC:END -->
