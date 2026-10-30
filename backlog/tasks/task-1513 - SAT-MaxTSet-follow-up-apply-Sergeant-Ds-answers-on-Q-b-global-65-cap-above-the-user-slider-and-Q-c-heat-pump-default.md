---
id: TASK-1513
title: >-
  SAT MaxTSet follow-up: apply Sergeant D's answers on Q-b (global 65 cap above
  the user slider) and Q-c (heat-pump default)
status: To Do
assignee: []
created_date: '2026-10-30T14:35:00Z'
updated_date: '2026-10-30T15:59:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 669000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1373 (alpha.554). Interim today: SH= and CS capped at the global 65 C even when the user chose more; heat-pump 40 cap internal to CS only. Q-b asked 2026-10-30 14:03 UTC (msg 1565727831172845599, docs/sat/open-questions.md row Q-b-1373; thermo-nova MAXIMUM_SETPOINT const.py:36 is unused, user max 10..100 config_flow.py:612-613). Q-c (heat-pump default) still to ask after Q-b. Record each answer in docs/sat/sergeant-d-corrections.md before implementing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Q-b answer recorded and implemented with host test red/green
- [ ] #2 Q-c asked, answered, recorded and implemented
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Q-b answered A (entry 152, msg 1565732208885633145): no global 65 C cap once the user chose a value. AC1 unblocked. Q-c (heat-pump default) still to ask.

AC1 implemented (sonnet lane, integrated on 4903f891b as alpha.557): chosen max flow setpoint is the only cap (MsgID 49 bounds, underfloor 50), no global 65 cap when chosen; not chosen unchanged; heat-pump 40 internal to CS (Q-c open). tests/test_sat_max_tset.py: base Ran 25 FAILED (failures=4: e.g. bounds later gave SH=65 not SH=75, global 60 gave SH=60 not SH=70), fix 25 OK; webui sat-max-tset 8/8; full suite 69/70 (float roundtrip pre-existing); evaluate EXIT 0.
<!-- SECTION:NOTES:END -->
