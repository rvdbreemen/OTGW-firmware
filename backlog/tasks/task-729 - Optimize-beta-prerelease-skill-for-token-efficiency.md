---
id: TASK-729
title: Optimize beta-prerelease skill for token efficiency
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 18:01'
updated_date: '2026-05-27 18:04'
labels:
  - optimization
  - skill
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Analyze and optimize .claude/skills/beta-prerelease/SKILL.md to reduce token usage per invocation. The file is 279 lines with the main costs in Phase 3 (reads README+CHANGELOG+RELEASE_NOTES in full) and the Known Traps section (~100 lines loaded every invocation).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 P1: Build output via tee/tail pattern; full log only on failure
- [x] #2 P2: Phase 3 narrative check uses targeted grep/sed instead of full file reads
- [x] #3 P3: Write-to-disk after each document edit in Phase 3; keep only filenames in context
- [x] #4 P4: Known Traps compressed from ~100 to ~25 lines (detail already in workflow YAML)
- [x] #5 P5: Phase 8 uses gh run watch instead of 'open the browser'
<!-- AC:END -->
