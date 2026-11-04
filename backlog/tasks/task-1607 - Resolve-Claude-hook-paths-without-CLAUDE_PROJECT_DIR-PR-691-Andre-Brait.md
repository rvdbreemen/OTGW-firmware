---
id: TASK-1607
title: 'Resolve Claude hook paths without CLAUDE_PROJECT_DIR (PR #691, Andre Brait)'
status: To Do
assignee: []
created_date: '2026-11-04T17:55:00Z'
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
- [ ] #1 Hook commands work with CLAUDE_PROJECT_DIR unset (guard blocks Edit/edit on backlog/tasks/*.md, allows other paths)
- [ ] #2 Claude Code behaviour unchanged when CLAUDE_PROJECT_DIR is set
<!-- AC:END -->
