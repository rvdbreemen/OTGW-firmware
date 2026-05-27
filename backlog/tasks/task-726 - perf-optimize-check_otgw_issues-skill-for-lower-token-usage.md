---
id: TASK-726
title: 'perf: optimize check_otgw_issues skill for lower token usage'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 17:37'
updated_date: '2026-05-27 17:42'
labels:
  - tooling
  - performance
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The check_otgw_issues workflow is token-intensive. Audit identified 7 optimizations worth 50-75% reduction. See session audit for full breakdown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 O1: Attachment lazy-loading — skip >50KB files, use Grep over Read, WebFetch as first-pass
- [x] #2 O2: Discord timestamp-filter server-side via after= param, limit 50->25
- [x] #3 O3: GitHub comments fetched only for confirmed bug reports, capped at 5 comments
- [x] #4 O4: Tweakers reads only description field for triage; content:encoded only on demand
- [x] #5 O5: Backlog: one bulk task list instead of N separate search calls
- [x] #6 O6: Triage output written to .tmp/triage_result.json; developer sees one-liner summary
- [x] #7 O7: Early-exit if all timestamps <2h old
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Optimized check_otgw_issues skill for 50-75% lower token usage per run.

Seven optimizations implemented in .claude/commands/check_otgw_issues.md:

- O1: Attachment two-pass lazy-load — metadata triage before any download, WebFetch first-pass for small files, Grep-only (never Read) for large logs
- O2: Discord limit 50->25, discard non-matching messages immediately from context
- O3: GitHub issues classified from body before fetching comments; comments only for bug reports, capped at 5
- O4: Tweakers parses <description> only; <content:encoded> fetched only on bug-keyword match
- O5: Single bulk backlog task list + local matching instead of N separate search calls
- O6: Triage output written to .tmp/triage_result.json; compact one-liner presentation to developer
- O7: Early-exit if all timestamps <2h old — zero API calls in that case

Committed as 0109c4e5, pushed to origin/dev.
<!-- SECTION:FINAL_SUMMARY:END -->
