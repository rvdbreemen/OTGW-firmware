---
id: TASK-658
title: >-
  Backlog admin: flip 6 completed-but-In-Progress tasks to Done + triage 6
  long-stale In Progress tasks
status: To Do
assignee: []
created_date: '2026-05-22 05:52'
labels:
  - backlog-admin
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Five tasks have all ACs checked + Final Summary written + commits shipped in beta.16 but remain In Progress (TASK-633, 637, 652, 631, 607). TASK-626 (ADR-075 proxy-A routing) is In Progress despite commit 1ff0cdbb shipped in beta.16 and credited in CHANGELOG. Six older In Progress tasks (TASK-397, 431, 549, 556, 571, 385/387) have not moved in 14+ days during this 50-commit active window — either work landed elsewhere (close them) or they're blocked (add blocking-AC note).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 TASK-633, 637, 652, 631, 607, 626 each verified against shipped commits and flipped to Done via backlog task edit <id> -s Done
- [ ] #2 Each of those tasks' Final Summary updated if necessary to reference the actual landing commit SHA + PR number
- [ ] #3 TASK-397, 431, 549, 556, 571, 385, 387 individually triaged: either closed (and replaced with the active task that subsumes them) or annotated with an explicit 'AC #N: blocked on <reason>' note
- [ ] #4 backlog task list -s 'In Progress' --plain reviewed after cleanup; remaining In Progress entries are all actively being worked or have a documented block
<!-- AC:END -->
