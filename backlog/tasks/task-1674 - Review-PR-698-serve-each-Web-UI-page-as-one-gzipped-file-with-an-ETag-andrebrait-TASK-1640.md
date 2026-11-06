---
id: TASK-1674
title: >-
  Review PR #698: serve each Web UI page as one gzipped file with an ETag
  (andrebrait, TASK-1640)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-06T16:58:00Z'
updated_date: '2026-11-06T16:59:00Z'
labels:
  - github
  - pr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 797000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/pull/698 by andrebrait, base dev. Robert 2026-11-06: keep the #697 file queue (beta.109) and review #698 on top of dev. Its ADR is numbered ADR-214 in the PR, which dev already uses (diagnostic topic ids): ask for the next free number. Expected to reduce concurrent file streams and with that the two-tab heap dip (TASK-1673).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reviewer team on the PR head rebased on dev (correctness, ADR-139/147/165/188/192/212, tests); findings posted as a PR review
- [ ] #2 Tests + evaluate + combo build green on the rebased head; bench two-tab load with hd_min_free_heap and hd_min_max_block against dev beta.109
- [ ] #3 ADR renumbered to a free number and lint-clean
- [ ] #4 Merge only on Robert's go
<!-- AC:END -->
