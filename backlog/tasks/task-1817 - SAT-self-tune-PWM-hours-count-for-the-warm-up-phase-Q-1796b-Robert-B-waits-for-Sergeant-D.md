---
id: TASK-1817
title: >-
  SAT self-tune: PWM hours count for the warm-up phase (Q-1796b, Robert B; waits
  for Sergeant D)
status: To Do
assignee: []
created_date: '2026-11-11T20:20:00Z'
updated_date: '2026-11-11T23:06:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 910000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert decided Q-1796b = B (DM msg 1570171160111620147, relayed by the coordinator 2026-11-11): PWM hours count for the warm-up phase of the self-tune; the both-phases-agree rule stays, the floating minimum stays 60 min (entry 315, TASK-1816). Today PWM-controlled ticks/hours are excluded from the tuner's clean hours (see the clean-hour rule, ADR-205 decision 2, and the satCurveTune block in SATcontrol.ino / SATcurveTune.h). Sergeant D's answer to the same question is still open: implement only after his answer or explicit silence is recorded in docs/sat/sergeant-d-corrections.md (sat-ground-truth rule; changes SAT control behaviour). Check whether ADR-205 decision 2 / ADR-228 need a superseding ADR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer (or recorded silence) on Q-1796b is in docs/sat/sergeant-d-corrections.md before the code commit
- [ ] #2 Host test: an hour with PWM ticks counts for the warm-up phase on the fix and is excluded on the old code; floating unchanged
- [ ] #3 ADR impact settled (ADR-205 d2 / ADR-228) via adr-kit, recorded in the notes
- [ ] #4 Build, evaluate --quick, OTA bench on .61 with PWM active (satboilercapacity low) shows the PWM hour counted in SATTUNEH
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Sergeant D answered Q-1796b B as well, entry 316 (20:04:33 UTC, msg 1570167372474617888); with Robert's DM B this revises entry 174. Needs an ADR-228 update (ADR-228 is Accepted: supersede or amend via adr-kit). Unblocked once entry 316 is on origin/dev.

ADR-230 (Proposed) carries this rule as decision 2. Built only after Sergeant D answers Q-1817a (A: every PWM tick counts for warm-up, advice; B: PWM hours stay in the normal phase split) and Q-1817b (hybrid: gas-boiler rule, advice, or heat-pump rule), asked via comms 92.
<!-- SECTION:NOTES:END -->
