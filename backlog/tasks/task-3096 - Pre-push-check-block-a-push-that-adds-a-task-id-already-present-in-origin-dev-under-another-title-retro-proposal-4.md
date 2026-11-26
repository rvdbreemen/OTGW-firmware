---
id: TASK-3096
title: >-
  Pre-push check: block a push that adds a task id already present in origin/dev
  under another title (retro proposal 4)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:04:00Z'
updated_date: '2026-11-26T07:07:00Z'
labels: []
dependencies: []
ordinal: 1162000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert adopted retro proposal 4 (DM 1575390723199475816 A). scripts/check_backlog_task_ids.py also runs at pre-push: compare the task ids added in the pushed range with git ls-tree -r origin/dev backlog/; block when an id is already present under another file name (title); warn and skip when origin/dev is not fetched. Measure whether two simultaneous backlog task create in two worktrees collide.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A scratch branch with a deliberately colliding id passes the current hooks (red) and is blocked by the pre-push check (green)
- [x] #2 Same task file already in origin/dev (same name) and an unfetched origin/dev do not block (warn and skip)
- [x] #3 CLI race measured: two simultaneous task create in two worktrees, result reported
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Done 2026-11-26 (otgw-webui), pushed e69d8141d: .githooks/pre-push + check_backlog_task_ids.py --pre-push; 6 tests with real git (bare origin, stale clone rebasing onto a colliding id): the collision passes without the pre-push hook and is blocked with it; same name, rename in the range and unfetched origin/dev do not block. CLI race measured twice (two worktrees, simultaneous task create): no collision, a machine-wide lock refuses the second call; the next id is taken over ALL local worktrees; a create can fail with 'Project root or active branch refs kept changing' while committing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 tests/test_backlog_prepush_task_id.py 6/6 with real git (bare origin, stale clone that rebases onto a colliding id): the collision passes without the pre-push hook (test_colliding_id_blocked fails with ID_CHECK_PREPUSH=0) and is blocked with it; pushed e69d8141d, the push of that commit itself ran through the new hook. AC2 same name, a rename inside the range and an unfetched origin/dev do not block (warn and skip), covered by tests. AC3 CLI race measured twice with two worktrees and simultaneous task create: no collision, a machine-wide lock refuses the second call, the next id is chosen over all local worktrees. All host-verified.
<!-- SECTION:FINAL_SUMMARY:END -->
