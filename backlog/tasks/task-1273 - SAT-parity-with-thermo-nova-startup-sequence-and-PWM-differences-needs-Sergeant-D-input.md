---
id: TASK-1273
title: >-
  SAT parity with thermo-nova: startup sequence and PWM differences (needs
  Sergeant D input)
status: To Do
assignee: []
created_date: '2026-10-20 18:17'
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
- [ ] #1 Sergeant D answered the open design questions (recorded in notes)
- [ ] #2 Each agreed difference has its own task or is fixed here with a simulator or host test
<!-- AC:END -->
