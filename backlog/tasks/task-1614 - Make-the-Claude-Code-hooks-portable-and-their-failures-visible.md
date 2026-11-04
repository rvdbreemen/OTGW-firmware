---
id: TASK-1614
title: Make the Claude Code hooks portable and their failures visible
status: To Do
assignee: []
created_date: '2026-11-04 21:50'
labels:
  - process
  - agentic
dependencies: []
priority: high
ordinal: 754000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F3. The PostToolUse evaluate hook in .claude/settings.json runs: cd 'D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware' && python evaluate.py --quick 2>/dev/null || true. In a cloud or macOS session, or another worktree, it silently checks nothing (or the wrong tree). The git hooks only run when core.hooksPath is set per clone, which a fresh cloud session does not do.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1611.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The evaluate hook uses $CLAUDE_PROJECT_DIR (same pattern as the other hooks), not a hardcoded path
- [ ] #2 A FAIL from evaluate.py is visible to the agent (non-zero exit or printed summary), no blanket || true; shown with a deliberate PROGMEM violation, then reverted
- [ ] #3 The SessionStart hook sets git config core.hooksPath .githooks when it is unset
- [ ] #4 Verified on Windows (local tree) and on Linux (cloud session or WSL)
<!-- AC:END -->
