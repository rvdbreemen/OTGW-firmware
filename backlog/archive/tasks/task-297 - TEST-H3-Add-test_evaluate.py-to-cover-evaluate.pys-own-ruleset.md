---
id: TASK-297
title: '[TEST-H3] Add test_evaluate.py to cover evaluate.py''s own ruleset'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:17'
updated_date: '2026-04-19 07:05'
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
- [x] #1 tests/test_evaluate.py feeds synthetic .ino snippets to each check
- [x] #2 Every rule has at least one positive and one negative fixture
- [x] #3 python evaluate.py --quick still passes on the clean repo
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
tests/test_evaluate.py: 18 stdlib unittest tests covering is_hot_path_file (6 cases), scan_string_usages_detailed (7 cases including reference-skip and comment-skip), scan_binary_compare_calls (5 cases including memcmp_P negative), plus a smoke-integration test that runs the full --quick pipeline. All green. python evaluate.py --quick still 96.8% (baseline maintained).
<!-- SECTION:FINAL_SUMMARY:END -->
