---
id: TASK-1355
title: >-
  Supersede ADR-069, ADR-071 and ADR-195: they state wrong values; new ADRs with
  the values in the code and Sergeant D's records
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T16:44:00Z'
updated_date: '2026-10-29T04:54:00Z'
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
- [x] #2 adr lint passes; the superseded links are recorded both ways after acceptance (acceptance needs Robert's explicit OK)
- [x] #3 The '<=' boundary question is queued in docs/sat/open-questions.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 workflow wf_615e59e8-3bf: ADR-197 (supersedes ADR-069), ADR-198 (ADR-071), ADR-199 (ADR-195) drafted by adr-kit generator, status Proposed, supersedes: [] until acceptance (adr supersede writes both sides at once). Second readers opened every citation: no core value refuted; omissions/citation slips found (ADR-197: 11 items) and fixed by the fix agents. ADR index regenerated (199 ADRs). bin/adr-lint: the 3 new ADRs complete; the only FAIL is pre-existing ADR-164 (missing sections). Q20 (cold boundary) queued. Acceptance needs Robert's explicit OK and the Open Questions resolved (several go to Sergeant D).

2026-10-24: Robert chose to wait for Sergeant D before accepting ADR-197/198/199. All their open questions are queued (Q10, Q13, Q20-Q28). ADR-200 accepted, ADR-022 superseded.

2026-10-25 AC1 evidence: commit 057535397 'ADRs 197-199 (Proposed): successors to ADR-069, ADR-071 and ADR-195' plus the second-reader notes of 2026-10-24. AC2 waits on Sergeant D (Q20-Q28) and Robert's explicit OK.

2026-10-28: Q20 (entry 71): values 28.2/21 restated, boundary <= vs < re-asked as Q20b; entry 12's 'at or below' bullet named. Q21 (entry 72): 5.0 stays, confirms ADR-198. Q22 remove auto-tune -> TASK-1413. Q23-Q27 decided (entries 73-77) -> TASK-1414..1418. Q28 rounding scope open (Q28a/b).

2026-10-28 Q20b (entry 96): idle when the requested setpoint is BELOW the cold setpoint (our reading: strict <, equality heats; deviation from thermo-nova heating_control.py:72/:385 and entry 12). Change satColdCutoff (SATchEnable.h:26-29) and compares listed in entry 71.

2026-10-28 entry 98 (msg 1564909937161805966): 'at or below' — corrects entry 96; boundary is <= as thermo-nova and entry 12; no code change.

2026-10-28 maintainer decision (Robert, in session): ADR-198 accepted (adr accept --confirm, changed_by Robert van den Breemen); ADR-071 superseded by ADR-198 (adr supersede; ADR-071 frontmatter date set to its first commit 2026-04-09 so the supersede command could record the earlier transition).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-197 (SAT PID controller) and ADR-199 (SAT cold state and CH-enable rule) accepted 2026-10-29 with Robert's explicit OK in session; ADR-069 superseded by ADR-197 and ADR-195 by ADR-199, links recorded both ways (supersedes/superseded_by in frontmatter, adr supersede). Before acceptance the Open Questions were moved to 'Resolved Questions' (Sergeant D entries 42/111/115, 73, 74 + maintainer 15-min checkpoint, 75, 76, 77, 95; ADR-199 entries 44, 71, 96->98, maintainer answers TASK-1361/1362) and the Decision text was updated to the decided rules with the pending implementation tasks named (TASK-1414..1418, 1423; TASK-1238 and 1331 as follow-up work). Acronyms expanded for the clarity gate. ADR-069 frontmatter date set to its first commit (2026-04-09) so the supersede could record the earlier transition. Evidence: adr accept output 'accepted: ADR-197...' and 'accepted: ADR-199...'; bin/adr-lint PASS strictly on 197, 199, 195, ADR-069 advisory only; adr-index regenerated (201 ADRs). Not fixed here: tests/test_adr_governance.py test_live_index_complete fails because ADR-197..201 are missing from the README index (pre-existing, TASK-1437).
<!-- SECTION:FINAL_SUMMARY:END -->
