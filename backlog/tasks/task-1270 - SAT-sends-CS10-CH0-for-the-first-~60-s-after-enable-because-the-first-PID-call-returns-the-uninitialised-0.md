---
id: TASK-1270
title: >-
  SAT sends CS=10/CH=0 for the first ~60 s after enable because the first PID
  call returns the uninitialised 0
status: To Do
assignee:
  - '@claude'
created_date: 2026-10-20 18:16
updated_date: '2026-10-28T20:08:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1368
references:
  - TASK-1412
documentation:
  - docs/sat/sat-low-load-agent-handoff-2026-10-28.md
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

2026-10-22 23:15 cycle: skipped, still waiting for Sergeant D's OK on the plan (SAT control behaviour).

2026-10-25: changes boiler control, so queued for Sergeant D as Q39 in docs/sat/open-questions.md (A curve+P+restored I with D=0 as thermo-nova pid.py:126-131; B curve only; C keep CS=10/CH=0). Blocked on Q39.

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q39 A decided (entry 83): first tick sends curve + P + restored I; D term on the first tick asked as Q39b (D=0 provisional per the option text).

2026-10-28 Q39b (entry 95): D = 0 on the first tick.

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed (own host-test AC#1; decisions entries 83, 95). TASK-1368 kept: both change the SATpid.ino update gates.

2026-10-28: the current plan keeps the sample-time and unchanged-error gates that TASK-1368 removes (entry 75 :3425; TASK-1368 plan). Plan superseded until TASK-1368's update/output split lands; then rewrite it (start-up state + D = 0, Q39b).
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
author: @codex
created: 2026-10-28T20:08:00Z

---

Robert requested this builder handoff from Codex. Read docs/sat/sat-low-load-execution-backlog-2026-10-28.md (current schedule) and docs/sat/sat-low-load-agent-handoff-2026-10-28.md before the next task. Keep current 1368 and 1409 work; this update changes only metadata/docs. Main PID chain: 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415. 1381 branches after 1270 and still waits on Q53; 1415 waits on Robert for write cadence. Q55 is answered by entry 111. 1409 is independent, followed by telemetry verification with 1425. Include 1422/1424 and 1413 in their integration gates. Do not resolve the ID6/MaxTSet contract or any open question by assumption. Per-fix bench old-vs-new proof is required before tester alphas; formal 1412.18 -> .26 validation remains required. Do not stage or overwrite another worker's source/task changes; coordinate shared SATpid.ino edits one at a time.

---
<!-- COMMENTS:END -->
