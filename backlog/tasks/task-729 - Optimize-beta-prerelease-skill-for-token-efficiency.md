---
id: TASK-729
title: Optimize beta-prerelease skill for token efficiency
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 18:01'
updated_date: '2026-05-27 18:07'
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rewrote .claude/skills/beta-prerelease/SKILL.md (279 to 214 lines) with five token-efficiency optimizations:
P1: Build output via tee/tail pattern; full log only read on failure
P2: Phase 3 narrative check uses targeted grep/sed (grep -A 50, sed -n '1,/digest:end/p') instead of full reads of README (492 lines), CHANGELOG (468 lines), and RELEASE_NOTES (128 lines)
P3: Write-to-disk after each Phase 3 document edit; keep only filename in context
P4: Known Traps compressed from ~100 lines to 5 bullets (full detail already in beta-prerelease.yml)
P5: Phase 8 uses 'gh run watch --exit-status' instead of 'open the Actions tab in browser'
Estimated runtime saving: 40-60% when Phase 3 refresh fires; 20-30% when narrative is already fresh (from shorter skill text in context).
<!-- SECTION:FINAL_SUMMARY:END -->
