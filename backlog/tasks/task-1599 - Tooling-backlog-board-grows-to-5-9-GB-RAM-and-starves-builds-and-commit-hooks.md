---
id: TASK-1599
title: >-
  Tooling: 'backlog board' grows to 5-9 GB RAM and starves builds and commit
  hooks
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T05:35:00Z'
updated_date: '2026-11-04T07:59:00Z'
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
- [x] #1 Reproduce: board memory over time with the repo idle vs with backlog CLI edits in other worktrees; record version and numbers
- [ ] #2 Check backlog.md upstream (GitHub issues/releases) for a known leak or fix; upgrade or file an issue with the numbers
- [x] #3 Interim rule recorded (e.g. close the board after use, or a watchdog) if no fix exists
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 measurement (backlog.md 1.55.3, Windows 11, backlog.exe board started in wt-stock, 1353 task files, WorkingSet sampled every 10 s; raw data scratchpad board_mem.csv / board_storm.csv): (1) idle ~190 MB. (2) 5 task files touched (mtime only) every 5 s for 120 s: spikes 275-555 MB per change, back to 232 MB after (+40 MB kept). (3) all 1353 files touched at once, 3 times 90 s apart (what a rebase / checkout / EOL rewrite does): storm 1 no spike (227 MB), storm 2 peak 1611 MB, storm 3 peak 3456 MB, each falling back to ~235 MB within ~60 s; after the test 232 MB. Reading: no large permanent leak; the board re-reads the whole task corpus on each file event without coalescing, so a burst of events stacks overlapping full reloads and the peak grows with burst size (1.6 then 3.4 GB). With several sessions rebasing, committing and running backlog CLI in the same tree, bursts follow each other before memory is released, which fits the 5-9 GB seen. Related upstream: MrLesk/Backlog.md#1038 (backlog browser retains 60-130 KB per task read; 201 MB to 25 GB in 5 min on Windows with 1.47.1). Not shown: a 9 GB plateau reproduced, the exact allocation site (no heap profile of the compiled binary).

2026-11-04 test 3: same storm test with check_active_branches=false and remote_operations=false (local wt-stock config only, reverted after): peaks 1882, 2084, 2156 MB, each back to ~224 MB. So cross-branch loading is not the main driver (with it on: 0/1611/3456 MB; one run each, noisy). The driver is the full corpus reload itself: ~9.7 MB of task text becomes ~2 GB transient per reload. Code (upstream 3a296c0, src/core/content-store.ts:1086-1186): a 'rename' event (how git and editors write files) can call loadTasksWithLoader() (full corpus) inside reconcileRenamedItem; any change in an archived-task dir calls refreshTasksFromDisk() (full corpus). Upstream comment posted: https://github.com/MrLesk/Backlog.md/issues/1038#issuecomment-6623857412

2026-11-04 local fix shipped: 1250 Done tasks archived with 'backlog task archive' (origin/dev 3dd6efdbf, 1250 pure renames, 107 tasks left in backlog/tasks; 29 duplicate-ID groups left, the CLI blocks ID commands on those). commit-msg hook accepts archived task records (04a071a5c; red/green: archived-only task old exit 1 / new exit 0, unknown id stays blocked). Measured on a copy: board burst peak 2526 -> 684 MB. Interim rule recorded in memory (feedback_backlog_board_short_open): board only briefly open; sessions may stop it under memory pressure and report. Upstream: comment on MrLesk/Backlog.md#1038 posted; coalescing fix being prepared locally (not opened yet). AC2 (upstream fix/upgrade) open.
<!-- SECTION:NOTES:END -->
