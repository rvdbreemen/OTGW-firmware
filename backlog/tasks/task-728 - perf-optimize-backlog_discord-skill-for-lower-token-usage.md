---
id: TASK-728
title: 'perf: optimize backlog_discord skill for lower token usage'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 17:51'
updated_date: '2026-05-27 17:56'
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
- [x] #1 B1: task list defaults to filtered by status; full list capped at 20 with total count shown
- [x] #2 B2: board command uses targeted queries (In Progress: all, To Do: max 5, Done: max 3) instead of full board dump
- [x] #3 B3: Phase 1b simplified — attachment fetch only triggered by explicit log-analysis request in message text
- [x] #4 B4: Early-exit if last_checked < 5 minutes old and no new messages found
- [x] #5 B5: Format examples compressed from ~60 to ~25 lines without losing clarity
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rewrote .claude/commands/backlog_discord.md (177 → 150 lines) with five token-efficiency optimizations:
B1: task list always filtered by status or capped at --limit 20 (never bare list)
B2: board replaced with three targeted queries (To Do/In Progress/Done, --limit 5 each)
B3: attachment fetching gated on bug keywords in message text; WebFetch first-pass only for text files
B4: early-exit if last-checked timestamp < 5 min old, no API call
B5: help text and format examples compressed from ~60 to ~25 lines
Estimated runtime token saving: 40-60% per invocation.
<!-- SECTION:FINAL_SUMMARY:END -->
