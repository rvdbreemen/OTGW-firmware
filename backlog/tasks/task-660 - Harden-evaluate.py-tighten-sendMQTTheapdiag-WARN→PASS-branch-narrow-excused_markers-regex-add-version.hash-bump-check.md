---
id: TASK-660
title: >-
  Harden evaluate.py: tighten sendMQTTheapdiag WARN→PASS branch + narrow
  excused_markers regex + add version.hash bump-check
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 05:52'
updated_date: '2026-05-22 06:24'
labels:
  - ci
  - process
dependencies: []
references:
  - evaluate.py
  - .githooks/pre-commit
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py changes in commit 946708b7 converted the sendMQTTheapdiag arithmetic check from WARN→PASS unconditionally when no char[N] buffer is found. Future regression that re-introduces snprintf without a sized buffer would now PASS silently. The new excused_markers regex matches any line containing '2.0.0', widening the escape hatch beyond cross-worktree references. Separately: .githooks/pre-commit bump-check enforces _VERSION_PRERELEASE was bumped but not that data/version.hash was staged — a committer who forgets to git-add version.hash ships a stale frontend cache-bust hash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 evaluate.py sendMQTTheapdiag check: when no char[N] buffer is found, WARN if snprintf(_P) is also present in the body (regression smell); PASS only when both buffer AND snprintf are absent
- [ ] #2 evaluate.py excused_markers regex tightened from bare '2.0.0' to '2\.0\.0\s+(worktree|line|branch|tree)' OR more specific contextual match — confirmed no false-negatives on existing ADR-cited code paths
- [ ] #3 .githooks/pre-commit extended: when src/OTGW-firmware/version.h is staged with _VERSION_PRERELEASE change, fail if src/OTGW-firmware/data/version.hash is NOT in the staged diff
- [ ] #4 Self-test: regression commit (snprintf added back to sendMQTTheapdiag without buffer) triggers WARN, and commit with version.h staged but version.hash forgotten is blocked
<!-- AC:END -->
