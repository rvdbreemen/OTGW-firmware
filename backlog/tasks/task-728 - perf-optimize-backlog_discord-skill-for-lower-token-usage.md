---
id: TASK-728
title: 'perf: optimize backlog_discord skill for lower token usage'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 17:51'
updated_date: '2026-05-27 17:51'
labels:
  - tooling
  - performance
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The backlog_discord command skill has token-intensive operations: unfiltered task list/board commands, a rarely-needed attachment workflow, and post-load message filtering. Five optimizations identified.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 B1: task list defaults to filtered by status; full list capped at 20 with total count shown
- [ ] #2 B2: board command uses targeted queries (In Progress: all, To Do: max 5, Done: max 3) instead of full board dump
- [ ] #3 B3: Phase 1b simplified — attachment fetch only triggered by explicit log-analysis request in message text
- [ ] #4 B4: Early-exit if last_checked < 5 minutes old and no new messages found
- [ ] #5 B5: Format examples compressed from ~60 to ~25 lines without losing clarity
<!-- AC:END -->
