---
id: TASK-928
title: >-
  Defer ADR authoring in implement-next-task loop to one end-of-loop evaluation
  pass
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 21:21'
updated_date: '2026-06-24 22:02'
labels: []
dependencies: []
ordinal: 143000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The autonomous implement-next-task loop currently drafts a Proposed ADR per task mid-drain (ADR phase between Review and Land) and commits it inside that task's commit. Maintainer wants ADR creation deferred: no per-task ADR; instead, after the drain loop ends, run ONE ADR-evaluation pass over all tasks landed in THIS run, dedup architectural decisions across tasks, author the Proposed ADRs (JS assigns numbers from glob-max to avoid collision), run the self-accept governance guard intact, commit them in one docs(adr) commit citing TASK-865, push, announce. Range = startHead..HEAD captured before Audit. Touches: .claude/workflows/implement-next-task.js, .claude/skills/implement-next-task/SKILL.md; verify-then-maybe-edit update-docs/SKILL.md, adr-kit-guide.md, CLAUDE.md, .wolf/anatomy.md, .wolf/cerebrum.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Per-task ADR phase + adrPrompt removed from the drain loop; Land no longer stages ADRs or runs the per-task ADR governance gate
- [x] #2 End-of-loop ADR-evaluation pass added after the for-loop, guarded by completed.length, fires on every break exit; captures startHead before Audit and uses startHead..HEAD filtered to src/**
- [x] #3 Self-accept governance guard (force Proposed, strip Accepted/manual history, revert premature back-references) travels intact to the end-of-loop pass and runs over every newly authored ADR
- [x] #4 ADR numbers assigned deterministically in JS from glob max of docs/adr/ADR-*.md (no LLM number-picking); enumerate step writes no files
- [x] #5 End-of-loop ADR commit cites TASK-865 (passes commit-msg hook pass1+pass2, docs-only no bump); each ADR References section lists the task IDs + commit hashes that made the decision
- [x] #6 meta.phases + meta.description + SKILL.md description/steps updated to reflect end-of-loop ADR evaluation; rewritten JS parses and all break paths verified
- [x] #7 Doc refs that state per-task ADR TIMING (update-docs/SKILL.md, adr-kit-guide.md, CLAUDE.md, anatomy.md, cerebrum.md) reconciled to the new flow
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Extension (2026-06-25, maintainer ask): added process-death recovery. Land stamps task note ADR-PENDING; end-of-loop pass stamps ADR-EVALUATED only on confirmed completion; next run's Audit returns leftover ADR-PENDING tasks as adrOrphans and the pass re-sweeps them; enumerate greps docs/adr for task ids and skips already-documented decisions so retry never double-drafts. Validated the eval brain with 2 report-only Workflow dry-runs: maxAdrNumber 152->assign 153; dedup skip-existing (ADR-056/102/152) + no over-skip on ADR-150 passing mention; positive-detection drafted ADR-153 for the SAT dual-axis split (a9047177). JS syntax-checked.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Deferred ADR authoring out of the implement-next-task per-task drain path into one end-of-loop ADR-Evaluation pass. The loop now captures startHead before Audit, drains tasks (Implement/Review/Land/Announce) with NO ADR in the per-task commit, then after the for-loop (fires on every break, guarded by completed.length) enumerates architectural decisions across startHead..HEAD + landed task bodies, dedups them, the JS assigns ADR numbers from glob-max (no LLM number-picking, no collision), drafts one Proposed ADR per decision via adr-kit:adr-generator, runs the self-accept governance guard intact, commits them in one docs(adr): ...(TASK-865) commit (docs-only, no bump; epic file tracked so commit-msg passes), pushes, and announces the batch to #dev-sat-mqtt. Each ADR References the contributing task ids + commit hashes. Parallel lanes inherit skipAdrEval from skipBump. SKILL.md, meta, anatomy.md, cerebrum.md reconciled; JS syntax-checked (async-wrapper) + adversarially reviewed (8/8 invariants PASS). CLAUDE.md/update-docs/adr-kit-guide confirmed not to state per-task ADR timing -> untouched.
<!-- SECTION:FINAL_SUMMARY:END -->
