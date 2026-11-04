---
id: TASK-1330
title: >-
  Check every SAT decision against Sergeant D's recorded decisions; find
  decisions that need an ADR
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T19:29:00Z'
updated_date: '2026-10-23T20:11:00Z'
labels:
  - sat
  - docs
  - adr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 453000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer (session 2026-10-23): (1) make sure future decisions are checked against George's (Sergeant D's) recorded decisions in docs/sat/sergeant-d-corrections.md; (2) check whether ADRs still need to be recorded based on his insights and decisions (entries 1-12 so far).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CLAUDE.md + AGENTS.md SAT ground truth block: every SAT plan, review, ADR and Discord answer is checked against docs/sat/sergeant-d-corrections.md; the plan names the entries it relies on or conflicts with; a conflict is put to Sergeant D, never decided silently; guarded by tests/test_sat_ground_truth_block.py
- [x] #2 implement-next-task SKILL.md steps 2/3/6 include the check
- [x] #3 Each recorded decision (entries 1-12) is mapped to existing ADRs; the ones that need a new or superseding ADR are listed with the reason, the others with why not
- [x] #4 Needed ADRs are drafted as Proposed via adr-kit (not accepted), passing adr-kit lint/quality, each citing the Sergeant D entry
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 21:55 part 1 done: CLAUDE.md + AGENTS.md SAT ground truth block (identical) gain the rule 'Every SAT decision is checked against Sergeant D's recorded decisions before it is taken' (read docs/sat/sergeant-d-corrections.md first, name relied-on/conflicting entries, conflicts to Sergeant D never resolved silently, reviewers treat an unmentioned conflict as a finding); tests/test_sat_ground_truth_block.py asserts it (OK; HEAD lacks it); implement-next-task SKILL.md steps 2, 3 and 6 carry the check. Part 2 (ADR audit) running as workflow wf_1bf9af56-cee.

2026-10-23 22:20 part 2 (ADR audit, workflow wf_1bf9af56-cee, 17 agents): entries mapped to ADRs. Needs ADR: (a) ADR-195 'SAT cold state and CH-enable rule (proposed successor to ADR-150)' for entries 5, 7, 9, 11 (part), 12; (b) ADR-196 'SAT behaviour ground truth: Sergeant D's recorded decisions, then pinned thermo-nova, then specs, then firmware' (entry 4 + the recording/check rule). No ADR: 1 (PWM BT fix within ADR-085's port), 2 (follow-thermostat bug fix), 3 (covered by ADR-071), 4 (folded into ADR-196), 6 (no behaviour change; hold covered by ADR-150/195), 8 (explanation), 10 (UI within ADR-085/192), 11 solar part (bug fix toward thermo-nova). Refutation pass: ADR-196 ok; ADR-195 failed (supersedes consistency lint FAIL, stale firmware claim, missing ADR-179 gateway-mode consequence) -> fixed: supersedes: [] until 'adr supersede ADR-150 --by ADR-195' runs at acceptance, firmware claims re-based on c3aa767a0, ADR-179/TASK-1238 consequence added. adr-lint: both ADVISORY (open questions); adr-quality ADR-195 1.00 (A), ADR-196 0.96 (A); test_adr_governance OK. Both Proposed; acceptance is Robert's. Note: ADR-195's Force PWM exception becomes obsolete with entry 13 / TASK-1332 and is updated in that commit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Part 1: every SAT decision is checked against Sergeant D's recorded decisions (CLAUDE.md/AGENTS.md block, skill steps 2/3/6, test), commit 11b3756da. Part 2: of his 12 recorded decisions, two need an ADR and are drafted as Proposed via adr-kit: ADR-195 (SAT cold state and CH-enable rule, proposed successor to ADR-150) and ADR-196 (SAT behaviour ground truth order); the other ten are covered, bug fixes toward thermo-nova or UI, reasons in the notes. Both passed an independent refutation after fixes; lint advisory only, quality A. Acceptance is the maintainer's decision; ADR-195 needs 'adr supersede ADR-150 --by ADR-195' together with its acceptance.
<!-- SECTION:FINAL_SUMMARY:END -->
