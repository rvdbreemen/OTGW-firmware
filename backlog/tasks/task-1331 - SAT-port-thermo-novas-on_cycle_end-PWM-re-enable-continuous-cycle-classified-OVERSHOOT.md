---
id: TASK-1331
title: >-
  SAT: port thermo-nova's on_cycle_end PWM re-enable (continuous cycle
  classified OVERSHOOT)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T19:43:00Z'
updated_date: '2026-10-25T11:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 454000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1327 review: thermo-nova re-enables PWM in two ways after a disable: sustained overshoot (heating_control.py:304, mirrored by satCycleCheckAutoSwitch) and on_cycle_end, where a continuous cycle classified OVERSHOOT calls pwm.enable() (pwm.py:219-223). The second path is not ported: no firmware code switches mode from the cycle classification (satSetControlMode is only called at SATcycles.ino ~980/1001/1017). SAT control behaviour: needs the thermo-nova trace with citations and Sergeant D's OK; check docs/sat/sergeant-d-corrections.md first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 thermo-nova on_cycle_end path traced with citations and the difference with the firmware recorded
- [ ] #2 Sergeant D's decision recorded in docs/sat/sergeant-d-corrections.md; implemented with a host test (red/green) if he agrees
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 research (read-only opus agent, every thermo-nova location opened at the pinned commit): on_cycle_end trace: heating_control.py:167 subscribes EVENT_SAT_CYCLE_ENDED -> pwm.on_cycle_end; tracker.py:74-81 builds the cycle at flame off, control_mode = PWM if pwm enabled else CONTINUOUS (:129), tail = last 180 s from start+min(60 s,25%) (:113-119); classifier.py:69-90 continuous: tail p90 of (flow - per-sample control setpoint) >= 3.0 (cycles/const.py:10) -> OVERSHOOT; pwm.py:219-223 CONTINUOUS+OVERSHOOT -> enable(); pwm.py:225-227 PWM+UNDERHEAT -> disable() (also unported). No min length, cooldown or cold check in on_cycle_end. Firmware: no cycle-end mode switch (satSetControlMode callers SATcycles.ino:987/1008/1024, SATcontrol.ino:1076/2310/2388-2394/5074); classifier differs (margin 2.0 vs 3.0, SHORT before OVERSHOOT, overshootSec>10 gate, one setpoint per cycle, no DHW-in-tail UNCERTAIN, OVERSHOOT in both modes). Blockers before porting: the tail-sort out-of-bounds bug (new task) and the hook must test SAT_MODE_CONTINUOUS explicitly (OFF exists in the firmware). Entries 8/10/11/12/13: no conflict. Discord question to Sergeant D drafted (4 questions: port which branches, margin 2.0 vs 3.0, gate on PWM auto switch, align classifier first).

2026-10-24 consolidated open questions re-posted to Sergeant D in #alpha-testing (Robert asked to make the wait explicit); waiting for his answer.

2026-10-24 Sergeant D answered (https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840), entry 15: 1a follow thermo-nova (port both on_cycle_end branches, interpretation recorded), 1b margin 2.0, 1d align the classifier first in the same change; 1c (PWM auto switch gate) he asked to expand - explained and asked again. Implementation waits for 1c.

2026-10-24 08:26 UTC Sergeant D on 1c: 'It should follow the thermo-nova solution' -> entry 18: the end-of-cycle switch is NOT gated on bPwmAutoSwitch (option B, interpretation recorded). All decisions for TASK-1331 are in (entries 15 and 18): port both on_cycle_end branches, margin 2.0, align the classifier first, ungated. Implementing.

2026-10-24 implementation (agent) in the working tree, NOT committed. Host test tests/test_sat_cycle_end_switch.py 12/12 OK, HEAD 7 fails; build SUCCESS. Review (opus) BLOCKS: HIGH 1: the classifier compares tail flow with _cycle_setpointAtStart (fFinalSetpoint frozen at flame-on, SATcycles.ino:744/821/701), thermo-nova uses the per-sample control_setpoint (cycles/tracker.py:255, :267-271); now that the class switches the mode this causes (a) firmware-only ping-pong: every mid-burn PWM->continuous exit is reverted to PWM at flame-off, (b) boot/enable with flame on: controlSp=0 -> OVERSHOOT -> PWM, (c) long continuous cycles while the PID ramps the CS, (d) DHW-started burns at CS=10. HEAD's own sat-comparison row 40 required row 26 (per-sample setpoint) first; the diff deferred it silently. MEDIUM 2: sampleCount<3 floor never fires (counted outside the 1 Hz gate) and no warm-up exclusion (tracker.py:98-100, :113-116): a short flicker with one spike can switch to PWM. MEDIUM 3: margin 2.0 equals the flow offset 2.0 (thermo-nova 3.0 vs 2.0): with per-sample CS every clamped sample would be OVERSHOOT -> question for Sergeant D. MEDIUM 4: no PWM IDLE pre-check (classifier.py:28-29). MEDIUM 5: kept continuous SHORT, <120 s UNCERTAIN, PWM SHORT not gated on ON, no kind-UNKNOWN checks: name to Sergeant D. LOW 6-10: wrong citation (guards are not reset by pwm.enable/disable), stale spec caller list, simulation DHW gap, test d not red on HEAD, readers of the class change. Next: port the per-sample setpoint (row 26) first, fix the sample floor and warm-up, and put margin 2.0 vs flow offset 2.0 plus the classifier deviations to Sergeant D. Waiting also for the SAT audit (workflow wf_75e80b42-84f).

2026-10-24: working-tree change parked in git stash 'TASK-1331 blocked by review: per-sample setpoint needed (Q2-Q4)' (SATcycles.ino, SATcontrol.ino, tests/test_sat_cycle_end_switch.py, firmware-sat-spec.md, sat-comparison.md). Resume after Sergeant D answers Q2-Q4 (docs/sat/open-questions.md).

2026-10-24: the parked stash predates TASK-1345 (and later 1341/1344): expect conflicts in SATcontrol.ino/SATcycles.ino on stash pop.

2026-10-24 Sergeant D answered Q2 (entry 36: per-sample control setpoint first, option A) and Q3 (entry 37: margin 2.0, a cycle at exactly 2.0 counts as overshoot; the firmware's strict '>' in the overshoot timer SATcycles.ino:896 must become '>='). Q4 (classifier steps) asked again with detail. Resume after Q4.

2026-10-25 Q4 answered (entry 55: classifier matches classifier.py exactly) -> prerequisite task TASK-1387.
<!-- SECTION:NOTES:END -->
