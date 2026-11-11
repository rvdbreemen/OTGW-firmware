---
id: TASK-1816
title: >-
  SAT self-tune: the floating phase votes only with at least 60 min of data in
  the 3 counted days (entries 313, 315)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T20:09:00Z'
updated_date: '2026-11-11T23:30:00Z'
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
- [x] #1 Entries 313 and 315 are on origin/dev in docs/sat/sergeant-d-corrections.md before the implementation commit
- [x] #2 tests/test_sat_curve_selftune.py: a window with 12 floating minutes and a warm-up raise fails on the old code (phases_disagree, hold) and passes on the fix (raise); exactly 60 min floating still votes
- [x] #3 ADR question settled (fits ADR-228 or a new ADR via adr-kit) and recorded in the task notes
- [ ] #4 Build, evaluate --quick, OTA bench on .61 with the TASK-1525 run-3 scenario shows the new reason in SATTUNED
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entries 313/315 (Q-1796a B, Q-1796a2 B): the floating phase votes only with at least 1 hour of data over the counted days of the window; below that the warm-up phase decides alone, as entry 305 does for a phase without data. Warm-up gets no minimum (entry 313 names floating only). Implementation: named constant SAT_CT_FLOAT_MIN_MS 3600000 in SATcurveTune.h; satTuneCloseDay() gets the control tick length (satTuneRollHour passes intervalMs, the same value its minTicks uses) and treats floating as without data when ticks x tick < the minimum; SatTuneDayOut.floatVotes is logged as SATTUNED fvote. Our tuner, no thermo-nova code (entry 298).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented locally on lane-1815: SATcurveTune.h SAT_CT_FLOAT_MIN_MS + SatTuneDayOut.floatVotes; SATcontrol.ino satTuneCloseDay(t, coeff, tickMs, out), floating votes only with phTicks3[FLOATING] x tickMs >= 1 h; SATTUNED gets fvote. Tests (tests/test_sat_curve_selftune.py, 76 OK): test_short_floating_sample_does_not_veto (12 floating minutes vs 72 warm-up hours at a 60 s interval: raise) fails on the pre-1816 code with (2.0, 'hold', 'phases_disagree') != (2.1, 'raise', 'phases_agree'); test_one_hour_of_floating_data_votes (60 ticks = 60 min votes and holds, 59 does not) is the boundary guard; test_record_formats pins fvote. evaluate --quick 86/0, test_sat_narrate_fits OK. docs/sat/sat-log-format.md documents fvote. Entries 313-315 are on origin (19d70dd58), AC1 holds.

AC3 (ADR question): settled with the coordinator: one new ADR via adr-kit amends ADR-228 for entries 313/315 and 316. ADR-230 (number assigned by the coordinator), Proposed, gate tests/test_sat_curve_selftune.py; relate ADR-228/ADR-205 written on both sides by adr relate; strict lint ADVISORY only (its 3 open questions: Q-1817a, Q-1817b, acceptance by Robert); tests/test_adr_governance.py OK.

ADR-230 split on Robert's choice (floating minimum only) and Accepted 2026-11-12 by Robert in session d3 (AskUserQuestion 'Ja, accepteer'; also DM 'A' to the coordinator, msg 1570216668469133343). Clarity gate fixed (PID/PWM expanded), Status paragraph repaired after adr accept cut it; strict lint: ADR-228 and ADR-230 PASS strictly; governance OK.
<!-- SECTION:NOTES:END -->
