---
id: TASK-2939
title: >-
  adr-kit: release fdf10bb (lint reads bold Status/frontmatter, knows partial
  supersede), then remove the TASK-2029 waiver
status: To Do
assignee: []
created_date: '2026-11-20T01:23:00Z'
labels:
  - adr
milestone: 2.1.0
dependencies: []
ordinal: 1070000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Part B of Robert's TASK-2029 decision ('A then B', DM 1573144344087498908). rvdbreemen/adr-kit branch fix/supersession-claim-parsing fdf10bb: adr-lint reads the old bold '**Status:**' line and the frontmatter superseded_by/supersedes, and does not count 'partly supersedes' / 'supersedes ADR-X decision N' as a full claim. Until it is released and installed here, the 18 supersession FAILs of adr-lint --strict are a recorded waiver in TASK-2029 (7 legitimate partial supersedes incl. ADR-102 <- ADR-255, 11 lint defects incl. ADR-098 -> 'ADR-097' prose vs frontmatter supersedes ADR-119).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr-kit fdf10bb (or its successor) is merged and released (merge is Robert's call), and that release is installed in this repo's tooling
- [ ] #2 adr-lint --strict on origin/dev shows no supersession FAIL for the 18 waived pairs; any remaining one is classified
- [ ] #3 The TASK-2029 waiver is marked ended in its notes, with the lint output quoted
<!-- AC:END -->
