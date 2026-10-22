---
id: TASK-1273
title: >-
  SAT parity with thermo-nova: startup sequence and PWM differences (needs
  Sergeant D input)
status: To Do
assignee: []
created_date: 2026-10-20 18:17
updated_date: '2026-10-22T06:54:00Z'
labels:
  - sat
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 403000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. Line-by-line comparison with other-projects/SAT-releases-thermo-nova (SAT 5.0.0) found, besides the flame-off offset and PID-first-call bugs: continuous clamp lacks the previous-CS term (heating_control.py:486-489 vs satApplyContinuous ~981-987), so CS follows a rising flow; PWM ON expiry timed from the previous cycle's flame-on (~750-756) instead of the ignition sync; no on_time>=180 start gate; effective boiler temp not seeded with flow at flame-on, EMA 30 s vs 180 s; overshoot->PWM auto-switch compares flow with fFinalSetpoint (SATcycles.ino ~949), which the clamp keeps within 2 C of flow, so it practically never fires; no cycle-end OVERSHOOT/UNDERHEAT switch; 30 s tick vs 5 s loop; MM=max in PWM-OFF vs 0; CH threshold CS>10 vs >22; no PM=25 after CS; PM/MI boot commands not sent on runtime enable. Open design questions for Sergeant D: should the PWM CS override be gated by overshoot_protection (Python default off)? keep the 28.2 cold cutoff (ADR-150) and coefficient 1.5 (Python: 22 and 2.0)?
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D answered the open design questions (recorded in notes)
- [ ] #2 Each agreed difference has its own task or is fixed here with a simulator or host test
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 Sergeant D answers (#alpha-testing 18:26 UTC): a) PWM CS override behaviour must be identical to thermo-nova, i.e. gated by overshoot_protection (Python default off). b) Keep cold cutoff 28.2 C (radiators); change the heating-curve coefficient default from 1.5 to 2.0. Note: a default change only affects units without a saved value; existing settings.ini keep 1.5 unless migrated (decide with maintainer).

2026-10-20 18:54 UTC Sergeant D: leave existing settings alone; the coefficient default 2.0 applies only to units without a stored value, no migration of stored 1.5.

From the TASK-1268 round-2 review (opus, by reading, pre-existing): (a) PWM ON-phase expiry is measured from the previous cycle's flame-on (SATcontrol.ino ~813-817), not from the ON-phase start as thermo-nova pwm.py (_last_update): when ignition takes longer than one 30 s tick the phase flips ON/OFF every tick (CS hold <-> 10) and the 180 s ignition timeout is never reached. (b) force PWM with the PID at the system maximum switches PWM->CONTINUOUS->PWM in the same tick (~4753-4761), so PWM state is reset every tick and the ignition timeout never runs. Both belong to the PWM ON timing scope of this task.

2026-10-22: TASK-1292 ground-truth spec is on dev: docs/sat/thermo-nova-spec.md (cited, Alexwijn/SAT releases/thermo-nova @ 7beaaf5), docs/sat/firmware-sat-spec.md and docs/sat/sat-comparison.md (443 rows, 434 verified). This task waits for Sergeant D's validation of the thermo-nova spec; the plan must quote the thermo-nova code for its area with file:line citations (CLAUDE.md 'SAT ground truth' block) before it is resumed.
<!-- SECTION:NOTES:END -->
