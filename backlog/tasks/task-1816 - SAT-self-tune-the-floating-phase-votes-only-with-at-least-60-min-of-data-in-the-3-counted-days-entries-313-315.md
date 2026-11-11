---
id: TASK-1816
title: >-
  SAT self-tune: the floating phase votes only with at least 60 min of data in
  the 3 counted days (entries 313, 315)
status: To Do
assignee: []
created_date: '2026-11-11T20:09:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 909000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, entry 313 (Q-1796a B, msg 1570167074670780447) and entry 315 (Q-1796a2 B, msg 1570167982762491906): the floating phase may vote only when it has at least 1 hour of data over the 3 counted days; below that the warm-up phase decides alone, as for a phase without data (entry 305). Field case (gruijter 10 Nov, beta.167): 12 floating minutes vetoed 650 warm-up minutes (phases_disagree). Implement as a named constant (60 min, e.g. SAT_CT_FLOAT_MIN_TICKS from the tick period) in the satCurveTune block (SATcurveTune.h / SATcontrol.ino), and narrate it in SATTUNED. ADR-228 decision text on a phase without data is extended by this; check whether that needs a superseding ADR or fits ADR-228 (Accepted) as an implementation of entry 305's rule. Entries 313-315 must be on origin/dev (wt-comms 2f7c8477f, ee4f57eec) before the code commit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Entries 313 and 315 are on origin/dev in docs/sat/sergeant-d-corrections.md before the implementation commit
- [ ] #2 tests/test_sat_curve_selftune.py: a window with 12 floating minutes and a warm-up raise fails on the old code (phases_disagree, hold) and passes on the fix (raise); exactly 60 min floating still votes
- [ ] #3 ADR question settled (fits ADR-228 or a new ADR via adr-kit) and recorded in the task notes
- [ ] #4 Build, evaluate --quick, OTA bench on .61 with the TASK-1525 run-3 scenario shows the new reason in SATTUNED
<!-- AC:END -->
