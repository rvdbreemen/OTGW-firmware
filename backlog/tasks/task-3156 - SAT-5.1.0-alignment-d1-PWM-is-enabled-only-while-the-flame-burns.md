---
id: TASK-3156
title: 'SAT 5.1.0 alignment: d1, PWM is enabled only while the flame burns'
status: To Do
assignee: []
created_date: '2026-11-28T17:37:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1201000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: ADOPT 5.1.0. An overshoot measured with the flame off is a boiler cooling down, not a boiler that cannot modulate low enough; today that can switch to PWM for the wrong reason. Source 5.1.0: heating_control.py:380-382. Our code: satCycleCheckOvershootSwitch (SATcycles.ino); the overshoot count resets on flame-off. No conflicting entry found. Risk low: one condition. Part of the SAT 5.1.0 alignment (Robert, coordinator chat 2026-11-28 16:11, register entry 542: align to 5.1.0, ours only where better; the team decides each point). Decision table: OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md (architect, revised 16:3x after poc-dev), record ADR-307 (Proposed). 5.1.0 = Alexwijn/SAT tag 5.1.0, commit 9c2fad9, citations custom_components/sat/<file>:<line>; delta docs/sat/alex-sat-5.1.0-delta.md. Rank (ADR-292): our implementation and Sergeant D's recorded decisions first; a conflicting entry is named and Sergeant D is informed afterwards, not asked. The first beta (build 313c4ba) is unchanged by this task. Step 1 order: a then h (a pair), then d1, k, i.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test red on the code before the change, green after (test name in the notes)
- [ ] #2 Bench by otgw-bench-tester in an approved SAT simulation window, result in the notes; BlueFlame informs Sergeant D afterwards
<!-- AC:END -->
