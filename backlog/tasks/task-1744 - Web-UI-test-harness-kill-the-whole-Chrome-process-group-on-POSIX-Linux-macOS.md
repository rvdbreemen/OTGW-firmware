---
id: TASK-1744
title: >-
  Web UI test harness: kill the whole Chrome process group on POSIX
  (Linux/macOS)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T05:08:00Z'
updated_date: '2026-11-15T10:42:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 fix: tests/webui/lib/page-harness.mjs launchChrome() spawns Chrome with detached:true on POSIX (Chrome leads its own process group, marked child.__otgwGroup); stopChromeTree() then sends SIGKILL to -pid before child.kill(), so renderer/GPU/utility children end too. Windows unchanged (taskkill /T). Evidence on Windows only: chrome.exe count 0 before / 0 after otd-override-clear (5 pass, startPage) and graph-room-dropout (1 pass, launchChrome); harness-stall passes. AC1 NOT evidenced: it needs a Linux or macOS host with Chrome; WSL Ubuntu here has no Chromium and no node, and installing them needs sudo (not done). Leave In Progress until a POSIX run (process count before/after).
<!-- SECTION:NOTES:END -->
