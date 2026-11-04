---
id: TASK-1622
title: Resolve the 'backlog task list returns empty' workaround
status: To Do
assignee: []
created_date: '2026-11-04 21:50'
labels:
  - process
  - agentic
dependencies: []
priority: low
ordinal: 762000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
CLAUDE.md and the implement-next-task skill call 'backlog task list returns empty' a known bug and route around it by reading task files. On 2026-11-04 in a Linux cloud session, backlog.md v1.46.1 (the version in docs/guides/backlog-cli.md) listed all 115 tasks and filtered by status correctly. The bug may be Windows-specific, version-specific or already gone; latest npm version is 1.55.3.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1611.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on Robert's Windows tree with the installed version; record version and output
- [ ] #2 If it no longer reproduces (or an upgrade fixes it): remove the workaround text from CLAUDE.md, AGENTS.md and the implement-next-task skill, and update docs/guides/backlog-cli.md
- [ ] #3 If it still reproduces: an upstream issue or a local root cause is recorded in the notes
<!-- AC:END -->
