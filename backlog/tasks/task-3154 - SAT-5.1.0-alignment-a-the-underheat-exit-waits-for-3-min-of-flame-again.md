---
id: TASK-3154
title: 'SAT 5.1.0 alignment: a, the underheat exit waits for 3 min of flame again'
status: To Do
assignee: []
created_date: '2026-11-28T17:28:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1199000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: ADOPT 5.1.0. The underheat exit (flow 3.0 C below the request for 180 s, PWM ON phase) also needs the flame to have burned 180 s; at the start of a PWM burn the water is still warming up, so a low flow is no evidence that the boiler is too weak. Source 5.1.0: heating_control.py:446 and :481-486. Our code: satCycleCheckPwmExits in SATcycles.ino (gate removed by ADR-305, entry 456 had it). Conflicting or related entries: 456 (agrees), 517 and 520 (plain March rules). Amends ADR-305 decision 3. Risk low: the earliest exit is about 6 min; saturation (300 s, entry 492) still ends a real shortfall first. Must go BEFORE or together with h (while an ON phase waits for its flame the underheat exit already counts). Part of the SAT 5.1.0 alignment (Robert, coordinator chat 2026-11-28 16:11, register entry 542: align to 5.1.0, ours only where better; the team decides each point). Decision table: OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md (architect, revised 16:3x after poc-dev), record ADR-307 (Proposed). 5.1.0 = Alexwijn/SAT tag 5.1.0, commit 9c2fad9, citations custom_components/sat/<file>:<line>; delta docs/sat/alex-sat-5.1.0-delta.md. Rank (ADR-292): our implementation and Sergeant D's recorded decisions first; a conflicting entry is named and Sergeant D is informed afterwards, not asked. The first beta (build 313c4ba) is unchanged by this task. Step 1 order: a then h (a pair), then d1, k, i.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test red on the code before the change, green after (test name in the notes)
- [ ] #2 Replay count: how often the earliest underheat exit moves from about 3 min to about 6 min on the recorded runs, in the notes
- [ ] #3 Bench by otgw-bench-tester in an approved SAT simulation window, result in the notes; BlueFlame informs Sergeant D afterwards
<!-- AC:END -->
