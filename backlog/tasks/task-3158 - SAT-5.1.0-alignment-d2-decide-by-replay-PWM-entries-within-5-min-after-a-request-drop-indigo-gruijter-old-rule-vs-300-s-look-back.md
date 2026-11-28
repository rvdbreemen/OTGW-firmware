---
id: TASK-3158
title: >-
  SAT 5.1.0 alignment: d2, decide by replay: PWM entries within 5 min after a
  request drop, indigo + gruijter, old rule vs 300 s look-back
status: To Do
assignee: []
created_date: '2026-11-28T17:48:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1203000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Point d2: 5.1.0 compares the overshoot with the highest request of the last 300 s (heating_control.py:388, :407-415), ours compares with the current request (entry 497, Q-456e A, kept without data). The architect and poc-dev disagree and neither has evidence: decision = KEEP OURS UNTIL A MEASUREMENT DECIDES. Measurement (host replay, no firmware change): in the archived captures (indigo_light, gruijter) count the PWM entries that start within 5 min after a request drop of more than the margin, and compare with what the 300 s look-back would have done. If the look-back removes entries that ended through underheat or saturation, adopt it (then a medium risk task: bench a request drop); if not, keep ours. Conflicting entries: 497, 407. Part of the SAT 5.1.0 alignment (Robert, coordinator chat 2026-11-28 16:11, register entry 542: align to 5.1.0, ours only where better; the team decides each point). Decision table: OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md (architect, revised 16:3x after poc-dev), record ADR-307 (Proposed). 5.1.0 = Alexwijn/SAT tag 5.1.0, commit 9c2fad9, citations custom_components/sat/<file>:<line>; delta docs/sat/alex-sat-5.1.0-delta.md. Rank (ADR-292): our implementation and Sergeant D's recorded decisions first; a conflicting entry is named and Sergeant D is informed afterwards, not asked. The first beta (build 313c4ba) is unchanged by this task. Step 1 order: a then h (a pair), then d1, k, i.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Replay count on the indigo and gruijter captures with both rules, in the notes
- [ ] #2 Decision adopt or keep ours with the numbers, and an implementation task if adopt
<!-- AC:END -->
