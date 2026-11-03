---
id: TASK-1584
title: >-
  SAT cycle classifier: skip the warm-up (min 60 s, 25 % of the burn) before the
  180 s tail, as thermo-nova (entry 244)
status: To Do
assignee: []
created_date: '2026-11-03T06:37:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 729000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 244 (Q-PARITY-3 A, 2026-11-03): the tail the classifier judges starts after an effective warm-up of min(60 s, 25 % of the burn), and is the last 180 s after that (thermo-nova cycles/tracker.py:113-116). The port takes the last 180 samples including the warm-up (SATcycles.ino:1009-1025; the DHW-in-tail check already skips it, :925-927). Found by TASK-1412.14 (Q-PARITY-3).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real SATcycles.ino: a burn under 240 s no longer counts its first quarter in the tail; the warmup_high_flow_short_cycle parity scenario of tests/test_sat_classifier_parity.py agrees with thermo-nova (removed from the DIFFERENCES catalogue); red on the old code
- [ ] #2 RAM delta stated (no new buffer expected)
<!-- AC:END -->
