---
id: TASK-3159
title: 'SAT 5.1.0 alignment: b and f, revisit with a capture count after step 1 and 2'
status: To Do
assignee: []
created_date: '2026-11-28T17:51:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1204000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Points b and f: KEEP OURS FOR NOW, revisit with data (low priority). b: when heating demand stops 5.1.0 pauses PWM instead of ending it (heating_control.py:302, pwm.py:132-143); ours ends PWM (entries 12, 462, ADR-305). High risk (three code paths, a second change of a rule Robert accepted on 28 Nov, a restart in a pause would resume PWM). Count in the archived captures how often a heating restart after a pause overshoots into PWM within 10 min. f: after a restart PWM resumes in its off phase (pwm.py:167-190) needs a wall-clock stamp in flash (about 3 to 4 writes per hour, poc-dev estimate) and a valid time at boot; the gain is one burn per restart and restarts are rare (entry 462: the gateway rarely restarts). Revisit together with b (the same PWM state model). Conflicting entries: 12, 190, 462, 43, 121, 140, 363. The summer pause (entry 190) stays ours in any case. Part of the SAT 5.1.0 alignment (Robert, coordinator chat 2026-11-28 16:11, register entry 542: align to 5.1.0, ours only where better; the team decides each point). Decision table: OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md (architect, revised 16:3x after poc-dev), record ADR-307 (Proposed). 5.1.0 = Alexwijn/SAT tag 5.1.0, commit 9c2fad9, citations custom_components/sat/<file>:<line>; delta docs/sat/alex-sat-5.1.0-delta.md. Rank (ADR-292): our implementation and Sergeant D's recorded decisions first; a conflicting entry is named and Sergeant D is informed afterwards, not asked. The first beta (build 313c4ba) is unchanged by this task. Step 1 order: a then h (a pair), then d1, k, i.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture count for b (overshoot restarts after a pause) in the notes
- [ ] #2 Decision for b and f with the numbers
<!-- AC:END -->
