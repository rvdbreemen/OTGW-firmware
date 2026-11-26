---
id: TASK-3096
title: >-
  Pre-push check: block a push that adds a task id already present in origin/dev
  under another title (retro proposal 4)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:04:00Z'
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
- [ ] #1 A scratch branch with a deliberately colliding id passes the current hooks (red) and is blocked by the pre-push check (green)
- [ ] #2 Same task file already in origin/dev (same name) and an unfetched origin/dev do not block (warn and skip)
- [ ] #3 CLI race measured: two simultaneous task create in two worktrees, result reported
<!-- AC:END -->
