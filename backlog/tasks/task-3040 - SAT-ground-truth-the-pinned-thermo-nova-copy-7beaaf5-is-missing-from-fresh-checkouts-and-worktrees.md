---
id: TASK-3040
title: >-
  SAT ground truth: the pinned thermo-nova copy (7beaaf5) is missing from fresh
  checkouts and worktrees
status: To Do
assignee: []
created_date: '2026-11-25T05:09:00Z'
labels:
  - sat
  - process
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1148000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT ground-truth rules name other-projects/SAT-releases-thermo-nova/ at commit 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb as the read-only local copy. On 2026-11-24 it did not exist in the main checkout (only other-projects/SAT-develop, HEAD 2b01ba65, which is not the pin), and other-projects is a gitignored submodule that fresh worktrees do not carry. A copy with the same file content as the pin sits in the OTGW-other-projects repo. On 2026-11-25 otgw-dev fetched the pin (github.com/Alexwijn/SAT, detached HEAD 7beaaf5) into other-projects/SAT-releases-thermo-nova in the main tree and verified pwm.py:87-97, 106-108, 134, 201, 205 there. Open for Robert: how to make the pinned copy reproducible for every checkout and agent (commit the pinned snapshot into OTGW-other-projects, or a documented one-line fetch recipe in docs/sat/, or a check in evaluate.py that fails when the directory is absent or not at the pin).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision by Robert on how the pinned thermo-nova copy is provided
- [ ] #2 A fresh checkout can obtain other-projects/SAT-releases-thermo-nova at 7beaaf5 with one documented step, and the agent rule says where to look
- [ ] #3 A check (evaluate.py or a test) reports a missing or wrong-commit copy
<!-- AC:END -->
