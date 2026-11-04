---
id: TASK-1599
title: >-
  Tooling: 'backlog board' grows to 5-9 GB RAM and starves builds and commit
  hooks
status: To Do
assignee: []
created_date: '2026-11-04T05:35:00Z'
labels:
  - tooling
  - backlog
dependencies: []
priority: low
ordinal: 741000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed three times on 2026-11-02/04 on the maintainer's PC: the backlog.md TUI board (backlog.exe board, backlog.md via nvm node 24.19.0) grew to 9.3 GB, 5.5 GB and 9.0 GB within hours; free RAM fell to 0.5-3 GB, and Claude Code then reaped background work (simulation watcher, sessielog coordinator, a 1.x release-prep commit whose adr-judge LLM hook needs memory). Each time the board process was stopped with Robert's OK. Not investigated: backlog.md version, whether the growth tracks the number of task files (~1600 + archive) or file-watch events from parallel worktrees, or an upstream issue.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: board memory over time with the repo idle vs with backlog CLI edits in other worktrees; record version and numbers
- [ ] #2 Check backlog.md upstream (GitHub issues/releases) for a known leak or fix; upgrade or file an issue with the numbers
- [ ] #3 Interim rule recorded (e.g. close the board after use, or a watchdog) if no fix exists
<!-- AC:END -->
