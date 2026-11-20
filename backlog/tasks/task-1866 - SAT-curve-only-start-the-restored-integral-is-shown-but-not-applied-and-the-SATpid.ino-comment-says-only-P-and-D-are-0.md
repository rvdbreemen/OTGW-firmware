---
id: TASK-1866
title: >-
  SAT curve-only start: the restored integral is shown but not applied, and the
  SATpid.ino comment says only P and D are 0
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T11:54:00Z'
updated_date: '2026-11-16T11:18:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 942000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the second reader of Q-P1 (gruijter capture 2026-11-12, msg https://discord.com/channels/812969634638725140/1514720723980259460/1570395420667023420). SATpid.ino:191-193 on origin/dev (entry 115 exception) says that before the first valid room temperature only P and D are 0 and the restored integral is kept; the code also leaves I out of the output (satPidCurveOnly returns the curve), while state.sat.fPidI still shows the restored I in the web UI. So the UI shows an I that is not applied. The behaviour itself is under Q-P1 (Sergeant D); this task is about making comment and display match what the code does once Q-P1 is answered.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After Q-P1 is answered and recorded as an entry, the SATpid.ino comment at the curve-only exception describes exactly which terms are in the output
- [x] #2 The web UI and REST show the I term that is actually applied during the curve-only window (or mark it as not applied), with a host test on the real code
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 (d3) picked up in the backlog sweep and put back: both ACs wait for Q-P1, which docs/sat/open-questions.md (origin/dev 341291f2b) still lists as 'asked' 2026-11-12 11:53 UTC with no entry in sergeant-d-corrections.md. AC1 needs the recorded answer; AC2 depends on it too (if Sergeant D wants the restored I applied, the display already matches after that change; if not, the UI must mark I as not applied). No code or comment change before the answer (ADR-196).

2026-11-16 (95): moved to milestone 2.1.0 under entry 386 (Robert 08:47:36 UTC, 'Mark anything new to the next release'; scope of the initial SAT release = entries 375-385). Entry 357 (Q-P1 A, 2026-11-15) is outside that scope. Known conflict, deliberately deferred and reported to the coordinator: the firmware is curve-only before the first room temperature after a boot (SATpid.ino:186-213 satPidCurveOnly: P, I, D out of the output, restored integral kept and shown), while entry 357 says curve + P + I + D from the restored state as thermo-nova (pid.py:150-155 loads integral and raw derivative; climate.py:370-375 requested_setpoint = pid.output). Implementing 357 changes the control after a reboot, so it waits for 2.1.0; needs an ack from Robert or Sergeant D that the stable ships curve-only at boot.

2026-11-16 (95), correction on the coordinator's call: the milestone move above is undone. Milestone stays 2.0.0, PENDING Sergeant D's answer: 92 puts it to him as one question, A = fix now for the stable release, B = curve-only at boot accepted for the stable release with entry 357 in 2.1.0. Entry 386 ('the restart issue looks resolved') may mean he considers 357 covered. Nothing is built until he answers.

2026-11-16 (95), per entry 388 option B (curve only until the first room temperature, entry 357 in 2.1.0). AC1: the SATpid.ino comment at the curve-only exception now says none of P, I, D is in the output (the output is the curve alone), the restored integral is kept and still shown in fPidI. AC2: new satPidCurveOnlyActive() (isnan(_pid_lastError)), REST sat/status field pid_curve_only (openapi.yaml + docs/api/README.md), web UI SAT page: the I term reads '... (not applied: curve only until a room temperature)' while it is true (new key tools.kv_i_not_applied in v2.js and i18n nl/de/el; i18n-lang test 6/6). Host test on the real loop: scenario 8 of tests/test_sat_loop_integration.py (room temperature muted 180 s: curveOnly=1 and pid output = heating curve; room back: curveOnly=0), mutant m8 (marker always false) RED. Not covered by a webui test: the '(not applied)' text in the SAT page (checked by reading v2.js only). The el.json string is an untranslated-by-review machine wording, flagged for the next language review. The 60 s boot silence of entry 388 is in TASK-1874 (same commit).

Pushed with TASK-1874 as 55dd8f015 (beta.348); REST pid_curve_only seen on .61 by 09 (false, no SAT running).
<!-- SECTION:NOTES:END -->
