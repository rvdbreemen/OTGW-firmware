---
id: TASK-332
title: >-
  Build ADR-080 self-gates: check_adr_gates and check_backlog_hygiene in
  evaluate.py
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 17:37'
updated_date: '2026-04-19 17:42'
labels:
  - architecture
  - meta
  - adr-080
  - review-2026-04-18-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Dogfood ADR-080 by writing the automated checks it demands. Check #1 walks docs/adr/ADR-*.md and flags ADRs with Accepted status that reference neither evaluate.py nor tests/ AND carry no classification label (guideline-level, structural, historical, policy, meta-level). Check #2 walks backlog/tasks/*.md and flags status:Done tasks with unchecked ACs that do not reference a deviation/follow-up in notes or final-summary. Both as INFO-level in the always-run quick pipeline so drift surfaces immediately but does not block CI on day 1. Tests land alongside in tests/test_evaluate.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 evaluate.py has check_adr_gates method that walks docs/adr/*.md and flags Accepted ADRs missing both a gate reference and a classification label, INFO-level
- [x] #2 evaluate.py has check_backlog_hygiene method that walks backlog/tasks/*.md and flags Done tasks with unchecked ACs and no deviation/follow-up note, INFO-level
- [x] #3 Both checks registered in evaluate_all() quick pipeline
- [x] #4 tests/test_evaluate.py covers both checks with at least one passing and one failing fixture each
- [ ] #5 Existing ADRs/tasks audited; gaps either closed inline (header label update) or logged as follow-up work
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC5 (existing ADRs/tasks audited, gaps closed inline OR logged as follow-up) deferred to separate cleanup passes. Running the new checks on the current repo produces a starting audit: 56/73 Accepted ADRs lack both a gate reference and a classification label; 58/334 Done tasks have unchecked ACs without a deviation note. INFO-level so CI is not blocked; humans triage. That is the intended first-state -- the check exists to surface drift rather than force a one-shot cleanup. Follow-up task can track the audit burn-down.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
evaluate.py: two new always-run checks implementing ADR-080 self-gates. check_adr_gates walks docs/adr/ADR-*.md, flags Accepted ADRs that reference neither evaluate.py/tests nor a classification label (guideline-level/structural/historical/policy/meta-level). check_backlog_hygiene walks backlog/tasks/*.md, flags Done tasks with unchecked ACs and no deviation marker (deferred, follow-up, out-of-scope, won't-fix, superseded, hardware/manual test). Both INFO-level so drift surfaces without CI-blocking. tests/test_evaluate.py: 10 new cases covering adr_is_accepted, adr_has_gate_or_label (classification labels + gate markers), task_status, task_unchecked_ac_count, task_has_deviation_note. evaluate.py check_code_structure also now accepts #pragma once alongside the classic #ifndef/#define idiom so modern header guards don't produce WARN noise. First-state audit: 56/73 Accepted ADRs lack markers, 58/334 Done tasks lack deviation notes -- drift visible, next person can triage at their own pace.
<!-- SECTION:FINAL_SUMMARY:END -->
