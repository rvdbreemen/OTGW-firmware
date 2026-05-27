---
id: TASK-727
title: 'perf: optimize release skill for lower token usage'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 17:44'
updated_date: '2026-05-27 17:48'
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
- [x] #1 R1: Discord limit 100->50 for contributor detection in Phase 4
- [x] #2 R2: Build output filtered to errors only; full log written to .tmp/build_release.log
- [x] #3 R3: Documents generated sequentially and written to disk; checkpoint shows summary not full content
- [x] #4 R4: GitHub issue/PR list uses --jq to extract only author.login and title
- [x] #5 R5: ADR validation uses bin/adr-context with commit messages instead of manual ADR reading
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Optimized release skill for lower token usage per run.

Changes in .claude/skills/release/SKILL.md:

- R1: Discord limit 100->50 for contributor detection (monthly releases fit in 50 msgs)
- R2: Build output filtered to last 10 lines + exit code; full log in .tmp/build_release.log, read only on failure
- R3: Documents written to disk immediately after generation; Checkpoint 1 shows compact summary + filenames, not full document contents
- R4: GitHub issue/PR list uses --jq to extract only {login, title}; no full JSON objects in context
- R5: ADR validation uses bin/adr-context with commit messages as input; top-5 relevant ADRs only

Estimated saving: 35-55% fewer tokens per release run. Main gains from Discord limit and build output filtering.

Committed as 64453418, pushed to origin/dev.
<!-- SECTION:FINAL_SUMMARY:END -->
