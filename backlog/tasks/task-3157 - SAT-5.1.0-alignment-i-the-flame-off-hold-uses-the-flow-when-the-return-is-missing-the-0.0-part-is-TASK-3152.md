---
id: TASK-3157
title: >-
  SAT 5.1.0 alignment: i, the flame-off hold uses the flow when the return is
  missing (the 0.0 part is TASK-3152)
status: To Do
assignee: []
created_date: '2026-11-28T17:44:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1202000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: ADOPT 5.1.0 partly. Adopt register entry 532 (a return of exactly 0.0 counts as missing, so the flow-based hold applies): that part is TASK-3152, link and do not duplicate. KEEP our wait of 5 minutes after boot with the previous setpoint until the return is known (entry 263): until the first return answer flow + 18 is a guess, the previous setpoint is not (poc-dev, safety argument about our code path). Source 5.1.0: heating_control.py:605-613. Our code: SATcontrol.ino satApplyPWM guard and satContinuousSetpoint. Conflicting or related entries: 263, 135, 149, 151, 532. Risk low: only boilers without a return. Part of the SAT 5.1.0 alignment (Robert, coordinator chat 2026-11-28 16:11, register entry 542: align to 5.1.0, ours only where better; the team decides each point). Decision table: OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md (architect, revised 16:3x after poc-dev), record ADR-307 (Proposed). 5.1.0 = Alexwijn/SAT tag 5.1.0, commit 9c2fad9, citations custom_components/sat/<file>:<line>; delta docs/sat/alex-sat-5.1.0-delta.md. Rank (ADR-292): our implementation and Sergeant D's recorded decisions first; a conflicting entry is named and Sergeant D is informed afterwards, not asked. The first beta (build 313c4ba) is unchanged by this task. Step 1 order: a then h (a pair), then d1, k, i.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 TASK-3152 carries the 0.0 part; this task states in its notes what else (if anything) differs from 5.1.0 after the 5 minute wait is kept
- [ ] #2 Host test red on the code before the change, green after (test name in the notes)
- [ ] #3 Bench by otgw-bench-tester in an approved SAT simulation window, result in the notes; BlueFlame informs Sergeant D afterwards
<!-- AC:END -->
