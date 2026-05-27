---
id: TASK-727
title: 'perf: optimize release skill for lower token usage'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 17:44'
updated_date: '2026-05-27 17:45'
labels:
  - tooling
  - performance
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The release skill has high token consumption from Discord (2x100 msgs), verbose build output in context, all 4 documents generated simultaneously, and unfiltered GitHub JSON. Audit identified 5 optimizations.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 R1: Discord limit 100->50 for contributor detection in Phase 4
- [ ] #2 R2: Build output filtered to errors only; full log written to .tmp/build_release.log
- [ ] #3 R3: Documents generated sequentially and written to disk; checkpoint shows summary not full content
- [ ] #4 R4: GitHub issue/PR list uses --jq to extract only author.login and title
- [ ] #5 R5: ADR validation uses bin/adr-context with commit messages instead of manual ADR reading
<!-- AC:END -->
