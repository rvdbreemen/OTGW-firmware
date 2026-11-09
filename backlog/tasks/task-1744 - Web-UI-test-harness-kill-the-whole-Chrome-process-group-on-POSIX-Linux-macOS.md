---
id: TASK-1744
title: >-
  Web UI test harness: kill the whole Chrome process group on POSIX
  (Linux/macOS)
status: To Do
assignee: []
created_date: '2026-11-09T05:08:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 849000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1739 by Robert 2026-11-09 (AC narrowed to Windows). stopChromeTree() in tests/webui/lib/page-harness.mjs only calls child.kill() on POSIX, so renderer/GPU children may outlive a run there. Fix candidate: spawn Chrome detached and kill the process group (process.kill(-pid)). No Linux/macOS host or CI is available here to evidence it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On Linux or macOS, no Chrome process is left after a run of the affected webui tests (process count before/after)
<!-- AC:END -->
