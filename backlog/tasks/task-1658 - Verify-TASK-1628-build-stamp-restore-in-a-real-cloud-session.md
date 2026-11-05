---
id: TASK-1658
title: Verify TASK-1628 build-stamp restore in a real cloud session
status: To Do
assignee: []
created_date: '2026-11-05T13:09:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: low
ordinal: 782000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Moved out of TASK-1628 (AC3) by Robert's decision on 2026-11-05 (AskUserQuestion). The fix in .claude/hooks/session-start-build-toolchain.sh restores version.h and data/version.hash after the SessionStart build when they were clean before it. So far it has been verified only by emulation in Git Bash. Verify it at the next cloud session on dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In a fresh cloud session on dev, git status straight after the session start is clean (no version.h or data/version.hash change)
- [ ] #2 The Stop hook does not ask to commit version.h in that session
<!-- AC:END -->
