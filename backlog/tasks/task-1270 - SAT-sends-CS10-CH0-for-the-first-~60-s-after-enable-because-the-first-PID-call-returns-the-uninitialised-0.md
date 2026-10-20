---
id: TASK-1270
title: >-
  SAT sends CS=10/CH=0 for the first ~60 s after enable because the first PID
  call returns the uninitialised 0
status: To Do
assignee: []
created_date: '2026-10-20 18:16'
updated_date: '2026-10-20 19:30'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 400000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SATpid.ino ~264-285: the first call sets _pid_lastUpdateMs=now and returns the stale fPidOutput=0 (elapsed<10 s); the second call 30 s later returns 0 again (elapsed<60). Telnet 11:38:49 and 11:39:19: pid=10.0 final=10.0, CS=10, CH=0; real PID only at 11:39:49. Part of Sergeant D's 'starting sequence is not correct'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 First control tick after enable sends a computed setpoint (host test or telnet transcript, red on old code)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 cycle: skipped for now; changes SAT behaviour (control/classifier), so it needs Sergeant D's OK per the step 4 rule.
<!-- SECTION:NOTES:END -->
