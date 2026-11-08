---
id: TASK-1725
title: >-
  New worktrees get their git submodules automatically: post-checkout hook plus
  a guard in run_tests and build
status: To Do
assignee: []
created_date: '2026-11-08T09:54:00Z'
labels:
  - tests
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 833000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08: a fresh git worktree add does not init the submodules (src/libraries/OpenTherm, src/libraries/SimpleTelnet, other-projects), so C++ host harnesses fail on a missing OpenTherm.h (system32-78 report) and other-projects is empty, which breaks the SAT ground-truth and PIC reference reads. .gitmodules also lists Arduino/libraries/aceTime and Arduino/libraries/Time, which are not initialised in the main tree either.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A new worktree via git worktree add has every needed submodule initialised at its recorded commit, without a manual step: .githooks/post-checkout runs git submodule update --init on a worktree's first checkout; hooksPath already .githooks
- [ ] #2 scripts/run_tests.py and build.py stop early with one clear message naming the missing submodule and the command to fix it, instead of compiler errors
- [ ] #3 Decision recorded for Arduino/libraries/aceTime and Time: still needed or removed from .gitmodules, with evidence
- [ ] #4 docs/guides/worktrees.md describes the behaviour; red-green evidence: fresh worktree before the fix has empty submodule dirs, after the fix populated; run_tests.py guard shown failing and passing
<!-- AC:END -->
