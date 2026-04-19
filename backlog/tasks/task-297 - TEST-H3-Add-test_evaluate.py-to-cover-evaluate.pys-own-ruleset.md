---
id: TASK-297
title: '[TEST-H3] Add test_evaluate.py to cover evaluate.py''s own ruleset'
status: To Do
assignee: []
created_date: '2026-04-18 19:17'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
+258 lines of new rules in evaluate.py with zero tests. A regex bug in strcmp check (multi-line call) would silently remove the guard. This is the only CI gate the project has.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 tests/test_evaluate.py feeds synthetic .ino snippets to each check
- [ ] #2 Every rule has at least one positive and one negative fixture
- [ ] #3 python evaluate.py --quick still passes on the clean repo
<!-- AC:END -->
