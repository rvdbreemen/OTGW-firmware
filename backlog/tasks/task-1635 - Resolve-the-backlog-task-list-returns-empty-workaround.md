---
id: TASK-1635
title: Resolve the 'backlog task list returns empty' workaround
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:45:00Z'
updated_date: '2026-11-04T22:57:00Z'
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
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce on Robert's Windows tree with the installed version; record version and output
- [x] #2 If it no longer reproduces (or an upgrade fixes it): remove the workaround text from CLAUDE.md, AGENTS.md and the implement-next-task skill, and update docs/guides/backlog-cli.md
- [x] #3 If it still reproduces: an upstream issue or a local root cause is recorded in the notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Windows reproduction 2026-11-04, backlog.md v1.55.3 (main dev tree): task list --plain printed 115 TASK lines for 115 task files; with the To Do status filter 35; same via node cli.js directly. The bug does not reproduce. Linux cloud session v1.46.1: 115 of 115 as well. Real Windows pitfall instead: backlog.cmd (cmd.exe) cuts the argument list at the first newline inside an argument; recreating TASK-1624..1636 through it kept only the first description paragraph and dropped -l, --priority, --dep and --ac (seen in the task files, redone via node cli.js).

AC3 does not apply: the bug no longer reproduces, so there is no upstream issue or root cause to record.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The backlog task list returns empty bug no longer reproduces. AC1: Windows v1.55.3 lists 115 of 115 task files (35 with the To Do filter), Linux v1.46.1 lists 115 of 115 (notes). AC2: workaround text removed from CLAUDE.md, AGENTS.md and the implement-next-task skill; docs/guides/backlog-cli.md states the check and the installed version. CLAUDE.md, AGENTS.md and the guide now warn about the real Windows pitfall instead: backlog.cmd cuts multi-line arguments at the first newline. AC3: conditional, not applicable because the bug does not reproduce (ticked as satisfied, see notes). Gates: tests/test_adr_governance.py 15 tests OK, evaluate.py --quick 0 FAIL. Docs-only change.
<!-- SECTION:FINAL_SUMMARY:END -->
