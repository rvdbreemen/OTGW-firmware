---
id: TASK-1270
title: >-
  SAT sends CS=10/CH=0 for the first ~60 s after enable because the first PID
  call returns the uninitialised 0
status: In Progress
assignee:
  - '@claude'
created_date: 2026-10-20 18:16
updated_date: '2026-10-22T06:54:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Parity with thermo-nova pid.py: there 'output' is a property, always round(heating_curve + P + I + D, 1), so a valid setpoint exists from the first call (pid.py ~127-131); update() only moves the terms. Ours (SATpid.ino ~262-285) returns the stored state.sat.fPidOutput while elapsed < SAT_PID_SAMPLE_TIME_LIMIT or the error is unchanged, and that value is 0 until the first real update ~60 s after enable, so SAT sends CS=10 / CH=0 twice (field log gruijter 11:38:49 and 11:39:19).
Change: when the PID has not produced an output yet (first call after init/enable/restore, or fPidOutput == 0 with a valid curve), return the output computed from the current terms: curve + Kp*error + I (restored integral or 0) + D (0, no slope yet), without advancing the integral/derivative state or the sample timer. The sample-time and unchanged-error gates stay as they are for later calls.
Verification: host test compiling the real satPidUpdate() path (first call returns curve + Kp*error, red on HEAD = 0); bench SAT simulation or replay: telnet 'SAT:' line of the first control tick after enable shows pid = computed value, not 10; CS on the first tick > 10 when heat is needed.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 cycle: skipped for now; changes SAT behaviour (control/classifier), so it needs Sergeant D's OK per the step 4 rule.

2026-10-21 plan posted to Sergeant D (#alpha-testing msg 1562557877732905114), open point: D = 0 and I = restored integral on the first tick. Waiting for his OK; cycle continues.

2026-10-22: TASK-1292 ground-truth spec is on dev: docs/sat/thermo-nova-spec.md (cited, Alexwijn/SAT releases/thermo-nova @ 7beaaf5), docs/sat/firmware-sat-spec.md and docs/sat/sat-comparison.md (443 rows, 434 verified). This task waits for Sergeant D's validation of the thermo-nova spec; the plan must quote the thermo-nova code for its area with file:line citations (CLAUDE.md 'SAT ground truth' block) before it is resumed.
<!-- SECTION:NOTES:END -->
