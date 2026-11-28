---
id: TASK-3155
title: >-
  SAT 5.1.0 alignment: h, wait up to 10 min for ignition, minimum burn stays 3
  min
status: To Do
assignee: []
created_date: '2026-11-28T17:33:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1200000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: ADOPT 5.1.0. Split the 180 s constant: PWM_HEATER_STARTUP_MS 180 s stays for the minimum burn and the tolerance-band waits (SATcontrol.ino:1614, :1411, :1421), PWM_IGNITION_WAIT_MS 600 s is used for the ignition wait only (SATcontrol.ino:1609). Source 5.1.0: pwm.py:262, const.py:18. When a boiler needs a slow or second ignition our ON timer started without a flame and the real burn was cut short. The no_ignition alarm does not read the constant (its own threshold not checked: check it). Conflicting entries: 496 (keep 180 s, Q-456d A), 80 (Q35 A). Risk low: one constant split in two. Goes as a PAIR with a (a first). Part of the SAT 5.1.0 alignment (Robert, coordinator chat 2026-11-28 16:11, register entry 542: align to 5.1.0, ours only where better; the team decides each point). Decision table: OTGW-field-data/_comms/sat_5_1_0_decision_table_2026-11-28.md (architect, revised 16:3x after poc-dev), record ADR-307 (Proposed). 5.1.0 = Alexwijn/SAT tag 5.1.0, commit 9c2fad9, citations custom_components/sat/<file>:<line>; delta docs/sat/alex-sat-5.1.0-delta.md. Rank (ADR-292): our implementation and Sergeant D's recorded decisions first; a conflicting entry is named and Sergeant D is informed afterwards, not asked. The first beta (build 313c4ba) is unchanged by this task. Step 1 order: a then h (a pair), then d1, k, i.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test red on the code before the change, green after (test name in the notes)
- [ ] #2 The no_ignition alarm threshold is checked and unchanged, result in the notes
- [ ] #3 Bench by otgw-bench-tester in an approved SAT simulation window, result in the notes; BlueFlame informs Sergeant D afterwards
<!-- AC:END -->
