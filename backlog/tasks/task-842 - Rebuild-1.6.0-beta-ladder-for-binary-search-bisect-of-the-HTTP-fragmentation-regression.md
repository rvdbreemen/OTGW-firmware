---
id: TASK-842
title: >-
  Rebuild 1.6.0 beta ladder for binary-search bisect of the HTTP-fragmentation
  regression
status: To Do
assignee: []
created_date: '2026-06-07 18:03'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Root cause (HTTP-serving heap fragmentation) is contained by the beta.2 gate, but the introducing commit is unknown (likely a cumulative threshold). Rebuild the 12 rebuildable in-line dev betas of the 1.6.0 cycle (beta.1,4,5,6,7,16,20,21,22,23,24,25) plus anchors 1.5.0 (GOOD) and 1.6.0 (BAD), firmware+filesystem each, labeled, into a root testset folder. George binary-searches: BAD -> go earlier, GOOD -> go later, until the exact GOOD/BAD boundary build is found. Builds done in an isolated temp worktree so the dirty dev tree is untouched. Artifacts are gitignored (not committed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 12 betas + 2 anchors built firmware+filesystem from their in-line dev commits in an isolated worktree; dev working tree untouched
- [ ] #2 Artifacts labeled by beta number (.ino.bin + .littlefs.bin + version) under a single root testset folder, which is gitignored
- [ ] #3 A binary-search worksheet (order + BAD-go-earlier / GOOD-go-later rule + GOOD/BAD criterion) is provided for George
<!-- AC:END -->
