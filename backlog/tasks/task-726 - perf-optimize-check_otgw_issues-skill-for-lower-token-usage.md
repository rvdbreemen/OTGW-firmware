---
id: TASK-726
title: 'perf: optimize check_otgw_issues skill for lower token usage'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 17:37'
updated_date: '2026-05-27 17:38'
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
- [ ] #1 O1: Attachment lazy-loading — skip >50KB files, use Grep over Read, WebFetch as first-pass
- [ ] #2 O2: Discord timestamp-filter server-side via after= param, limit 50->25
- [ ] #3 O3: GitHub comments fetched only for confirmed bug reports, capped at 5 comments
- [ ] #4 O4: Tweakers reads only description field for triage; content:encoded only on demand
- [ ] #5 O5: Backlog: one bulk task list instead of N separate search calls
- [ ] #6 O6: Triage output written to .tmp/triage_result.json; developer sees one-liner summary
- [ ] #7 O7: Early-exit if all timestamps <2h old
<!-- AC:END -->
