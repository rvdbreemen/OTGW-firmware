---
id: TASK-617
title: Fix false-positive and stale checks in evaluate.py
status: To Do
assignee: []
created_date: '2026-05-17 10:03'
labels:
  - tooling
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py reports 1 FAIL + 3 WARN that are bugs in the evaluator itself, not firmware defects. The ADR-reference FAIL also red-flags the documented origin/dev auto-push gate. Fix the four evaluator checks so the run reflects reality.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 check_adr_references_resolve skips reserved/skipped/unused ADR numbers and cross-worktree (2.0.0) references; the 4 known false positives no longer fail
- [ ] #2 Header-guard check accepts '#pragma once' as a valid guard (mqtt_discovery_verify.h passes)
- [ ] #3 JSON buffer-arithmetic check returns PASS (not WARN) when sendMQTTheapdiag has no fixed char[N] buffer
- [ ] #4 Documentation check finds BUILD.md and FLASH_GUIDE.md in docs/guides/
- [ ] #5 python evaluate.py shows 0 FAIL and these 4 items resolved; python evaluate.py --quick is green
<!-- AC:END -->
