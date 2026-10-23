---
id: TASK-1330
title: >-
  Check every SAT decision against Sergeant D's recorded decisions; find
  decisions that need an ADR
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T19:29:00Z'
updated_date: '2026-10-23T19:30:00Z'
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
- [ ] #3 Each recorded decision (entries 1-12) is mapped to existing ADRs; the ones that need a new or superseding ADR are listed with the reason, the others with why not
- [ ] #4 Needed ADRs are drafted as Proposed via adr-kit (not accepted), passing adr-kit lint/quality, each citing the Sergeant D entry
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 21:55 part 1 done: CLAUDE.md + AGENTS.md SAT ground truth block (identical) gain the rule 'Every SAT decision is checked against Sergeant D's recorded decisions before it is taken' (read docs/sat/sergeant-d-corrections.md first, name relied-on/conflicting entries, conflicts to Sergeant D never resolved silently, reviewers treat an unmentioned conflict as a finding); tests/test_sat_ground_truth_block.py asserts it (OK; HEAD lacks it); implement-next-task SKILL.md steps 2, 3 and 6 carry the check. Part 2 (ADR audit) running as workflow wf_1bf9af56-cee.
<!-- SECTION:NOTES:END -->
