---
id: TASK-3153
title: >-
  SAT: delta document alex-sat-5.1.0-delta.md, what Alexwijn SAT 5.1.0 (9c2fad9)
  does differently from the 7beaaf5 spec and what it has that we do not
status: To Do
assignee:
  - '@architect'
created_date: '2026-11-28T12:05:00Z'
labels:
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1198000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order of 2026-11-28 12:47 (register entry 536, corrected by entry 537): compare with the latest SAT 5.x. The reference SAT-develop in the store is now tag 5.1.0 (9c2fad9, other-projects 206431dc, firmware origin/dev 002b88b75). Write docs/sat/alex-sat-5.1.0-delta.md: what 5.1.0 does differently from the 7beaaf5 spec (docs/sat/thermo-nova-spec.md), per spec section, PWM and mode switching first, plus what 5.1.0 has that our firmware does not (for example demand_control, fail_safe, watchdog). Input: OTGW-field-data/_comms/sat_5_1_0_vs_7beaaf5_pwm_rules_2026-11-28.md. Rule (ADR-292, entry 537): our implementation and Sergeant D's recorded decisions rank first; every claim cites SAT-develop/custom_components/sat/<file>:<line> at 9c2fad9; facts and readings are marked; 7beaaf5 citations stay valid with their hash. Related: Q-CMP1a (entry 535), which waits for this and for the SAT beta.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PWM and mode-switching sections of the delta with file:line at 9c2fad9
- [ ] #2 The other spec sections of the delta, one by one
- [ ] #3 Cross-reference from docs/sat/thermo-nova-spec.md to the delta
- [ ] #4 Facts and readings are marked; a second reader opened the cited lines before the delta is used publicly
<!-- AC:END -->
