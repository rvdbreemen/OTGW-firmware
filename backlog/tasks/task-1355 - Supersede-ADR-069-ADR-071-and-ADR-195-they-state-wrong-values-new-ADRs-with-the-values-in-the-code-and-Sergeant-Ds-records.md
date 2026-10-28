---
id: TASK-1355
title: >-
  Supersede ADR-069, ADR-071 and ADR-195: they state wrong values; new ADRs with
  the values in the code and Sergeant D's records
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T16:44:00Z'
updated_date: '2026-10-28T07:52:00Z'
labels:
  - adr
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 490000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer 2026-10-24: ADRs with wrong values are retired (superseded) and replaced by new ADRs. SAT port audit #69: ADR-069 states deadband 0.25 (code 0.1), integral clamp [0, curve] (code symmetric), interval independence (false, audit #14), reset on coefficient change (no such call). ADR-071 states coefficient default 1.0 (code 2.0 since TASK-1347), range ~0.4-2.0 (code 0.1-5), 'OT-Thing' origin. ADR-195 marks the '<=' cold boundary as settled by entry 12 (entry 12 answered another question; entries 7 and 11 keep it open) and its rule 'CH decided only in satChEnableDecide' does not hold for three early-return paths. Every new ADR via adr-kit, status Proposed, values cited (code file:line, entry N, thermo-nova file:line); open points stay Open Questions (the '<=' boundary goes to Sergeant D via docs/sat/open-questions.md). Acceptance only after Robert's explicit OK; the old ADR is then marked superseded by adr-kit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Three Proposed ADRs exist that supersede ADR-069, ADR-071 and ADR-195, each value cited and checked by a second reader against the code
- [ ] #2 adr lint passes; the superseded links are recorded both ways after acceptance (acceptance needs Robert's explicit OK)
- [x] #3 The '<=' boundary question is queued in docs/sat/open-questions.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 workflow wf_615e59e8-3bf: ADR-197 (supersedes ADR-069), ADR-198 (ADR-071), ADR-199 (ADR-195) drafted by adr-kit generator, status Proposed, supersedes: [] until acceptance (adr supersede writes both sides at once). Second readers opened every citation: no core value refuted; omissions/citation slips found (ADR-197: 11 items) and fixed by the fix agents. ADR index regenerated (199 ADRs). bin/adr-lint: the 3 new ADRs complete; the only FAIL is pre-existing ADR-164 (missing sections). Q20 (cold boundary) queued. Acceptance needs Robert's explicit OK and the Open Questions resolved (several go to Sergeant D).

2026-10-24: Robert chose to wait for Sergeant D before accepting ADR-197/198/199. All their open questions are queued (Q10, Q13, Q20-Q28). ADR-200 accepted, ADR-022 superseded.

2026-10-25 AC1 evidence: commit 057535397 'ADRs 197-199 (Proposed): successors to ADR-069, ADR-071 and ADR-195' plus the second-reader notes of 2026-10-24. AC2 waits on Sergeant D (Q20-Q28) and Robert's explicit OK.

2026-10-28: Q20 (entry 71): values 28.2/21 restated, boundary <= vs < re-asked as Q20b; entry 12's 'at or below' bullet named. Q21 (entry 72): 5.0 stays, confirms ADR-198. Q22 remove auto-tune -> TASK-1413. Q23-Q27 decided (entries 73-77) -> TASK-1414..1418. Q28 rounding scope open (Q28a/b).

2026-10-28 Q20b (entry 96): idle when the requested setpoint is BELOW the cold setpoint (our reading: strict <, equality heats; deviation from thermo-nova heating_control.py:72/:385 and entry 12). Change satColdCutoff (SATchEnable.h:26-29) and compares listed in entry 71.
<!-- SECTION:NOTES:END -->
