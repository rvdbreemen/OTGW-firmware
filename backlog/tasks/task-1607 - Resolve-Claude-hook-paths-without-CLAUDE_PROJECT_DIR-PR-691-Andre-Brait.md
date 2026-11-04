---
id: TASK-1607
title: 'Resolve Claude hook paths without CLAUDE_PROJECT_DIR (PR #691, Andre Brait)'
status: Done
assignee: []
created_date: '2026-11-04T17:55:00Z'
updated_date: '2026-11-04T19:18:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 747000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Contributor PR #691 by Andre Brait: .claude/settings.json hook commands fall back to git rev-parse --show-toplevel when CLAUDE_PROJECT_DIR is unset (other harnesses such as OMP); backlog-mcp-guard.py matches tool names case-insensitively and also reads a path key (stricter guard). Taken over with the author kept.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Hook commands work with CLAUDE_PROJECT_DIR unset (guard blocks Edit/edit on backlog/tasks/*.md, allows other paths)
- [x] #2 Claude Code behaviour unchanged when CLAUDE_PROJECT_DIR is set
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Host evidence (2026-11-04, lane-prs b9fc4bc1a): guard with CLAUDE_PROJECT_DIR unset: Edit file_path backlog/tasks/*.md -> exit 2, edit path backlog/tasks/*.md -> exit 2, Write src/a.ino -> 0, Bash -> 0. Same lowercase edit+path case on the origin/dev guard -> exit 0 (passes through): red on old, green on new. evaluate --quick 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Hook commands resolve their paths without CLAUDE_PROJECT_DIR (PR #691, Andre Brait); the backlog-file guard blocks Edit/edit on backlog/tasks/*.md and allows other paths, with and without the variable. Evidence in the notes (guard red/green on the old lowercase bypass).
<!-- SECTION:FINAL_SUMMARY:END -->
