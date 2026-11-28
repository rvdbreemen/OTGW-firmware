---
id: TASK-3151
title: SAT mode switching follows thermo-nova (option A)
status: To Do
assignee: []
created_date: '2026-11-28T07:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1192000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parent of the SAT mode-switching simplification. Sergeant D chose option A (plain thermo-nova mode switching), entry 517; Robert (maintainer) set priority and scope 3a to 3d in one beta, entries 518 and 519. Plan: OTGW-field-data/_comms/plan_sat_mode_switching_simplify_for_robert_2026-11-28.md and _comms/plan_a_rule_table_2026-11-28.md. Steps: 3a no fail-safe and no one-entry-per-flame, 3b start in continuous with plain restore, 3c thermo-nova margins and drop SATpwmswitchmargin, 3d cleanup of dead code and rules 4a 5 6 7 8. Every step quotes thermo-nova with file:line (also climate.py, Sergeant D entry 520), names the replaced entries, host tests red on old code and green on the fix, bench and soak before the beta. Replay of the two recorded runs on the finished code as evidence.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Steps 3a, 3b, 3c and 3d are Done with evidence each
- [ ] #2 Replay of the recorded runs (gruijter 30.8 h, indigo_light 25 h) on the finished code reports the number of continuous/PWM switches
- [ ] #3 Beta notes block (HOLD in the next-draft) filled from the shipped build
<!-- AC:END -->
