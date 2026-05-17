---
id: TASK-617
title: Fix false-positive and stale checks in evaluate.py
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-17 10:03'
updated_date: '2026-05-17 10:04'
labels:
  - tooling
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py reports 3 WARN-level checks that are bugs in the evaluator itself, not firmware defects (pragma-once header guard not recognized; stale sendMQTTheapdiag buffer-arithmetic check after refactor; BUILD.md/FLASH_GUIDE.md searched in wrong dir). The related ADR-reference FAIL is owned by TASK-608. Fix these 3 evaluator checks so the run reflects reality.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Header-guard check accepts '#pragma once' as a valid guard (mqtt_discovery_verify.h passes)
- [ ] #2 JSON buffer-arithmetic check returns PASS (not WARN) when sendMQTTheapdiag has no fixed char[N] buffer
- [ ] #3 Documentation check finds BUILD.md and FLASH_GUIDE.md in docs/guides/
- [ ] #4 python evaluate.py: header-guard, buffer-arithmetic, and BUILD/FLASH doc checks all pass; no new failures introduced
<!-- AC:END -->
