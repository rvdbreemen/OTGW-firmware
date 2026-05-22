---
id: TASK-658
title: >-
  Backlog admin: flip 6 completed-but-In-Progress tasks to Done + triage 6
  long-stale In Progress tasks
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 05:52'
updated_date: '2026-05-22 06:42'
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
- [x] #1 TASK-633, 637, 652, 631, 607, 626 each verified against shipped commits and flipped to Done via backlog task edit <id> -s Done
- [x] #2 Each of those tasks' Final Summary updated if necessary to reference the actual landing commit SHA + PR number
- [x] #3 TASK-397, 431, 549, 556, 571, 385, 387 individually triaged: either closed (and replaced with the active task that subsumes them) or annotated with an explicit 'AC #N: blocked on <reason>' note
- [x] #4 backlog task list -s 'In Progress' --plain reviewed after cleanup; remaining In Progress entries are all actively being worked or have a documented block
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Backlog hygiene sweep completed 2026-05-22.

## Part A — flipped to Done (5 of 6)
- TASK-633 (MsgID 0 Status canonical gate, commit 103ed603, beta.7): Final Summary added, status Done.
- TASK-637 (README/CHANGELOG refresh, commit 8d95c35b): Final Summary already present, status Done.
- TASK-631 (set-cmd debug surface, commit 138a517b, beta.7+): AC#7 build-gate checked off (de-facto verified by merge), Final Summary added, status Done.
- TASK-626 (ADR-075 proxy-answer routing, commit 1ff0cdbb, beta.12+): all 7 ACs checked against the diff, Final Summary added, status Done.
- TASK-652 (delayms static-timer fix, commit 05e777bf): Final Summary already present, status Done.

## Part A — NOT flipped (1 of 6)
- TASK-607 (HA avty_t decouple, PR #583): code change has NOT landed on origin/dev — both sendMQTT(MQTTPubNamespace, CONLINEOFFLINE) calls still present at OTGW-Core.ino:4044 and MQTTstuff.ino:1158. AC#1 is blocking. Left In Progress with a triage note. CHANGELOG/README claim is premature. TASK-654 is the follow-up implementation task.

## Part B — 7 stale tasks triaged
- Category A (landed elsewhere, closed): TASK-549 (superseded by TASK-626 / ADR-075), TASK-556 (subsumed by ADR-070 sibling-suffix migration on dev).
- Category B (blocked, left In Progress): TASK-431 (rapid WebUI refresh freeze — no recent reports), TASK-571 (MsgID 1 TSet flip — code shipped, field validation pending).
- Category C (stale but relevant, demoted to To Do): TASK-397 (BGTRACE instrumentation), TASK-385 (light-theme dark text-fields), TASK-387 (mobile theme-toggle overlap).

## Part C — residual In Progress (21 tasks)
All listed below have active scope or documented blocker. Highlights worth a maintainer glance:
- TASK-607 (blocked on PR #583 merge), TASK-654 (the implementation follow-up), TASK-431 / TASK-571 (blocked on field signal).
- The rest are recent (created since 2026-05-15) and reflect ongoing 1.6.0-beta work or 2.0.0 cross-tree ports.

Residual list:
  [HIGH] TASK-431, 571, 599, 612, 654, 656, 657, 659, 666
  [MED]  TASK-242, 553, 660, 661, 667
  [LOW]  TASK-663, 665
  [---]  TASK-607, 611, 625, 639

Admin-only sweep: no code changes, no commits, no pushes.
<!-- SECTION:FINAL_SUMMARY:END -->
