---
id: TASK-1810
title: >-
  run_tests.py marks test/host/test_stream_real_cbuf.py FAILED although it
  passes
status: To Do
assignee: []
created_date: '2026-11-11T07:39:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 903000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-11 (TASK-1759 sweep, wt-1759a with a fresh .pio build tree). The test exits 0 and prints RESULT: PASS, then runs its deliberate red mutant (OTGW_NO_PRESIZE=1), whose output contains lines starting with FAILED; scripts/run_tests.py (classify, the ^FAILED regex around line 105) then reports the file FAILED. Without a build tree the test SKIPs, which is why the full sweep looked green before (TASK-1723).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 run_tests.py reports test_stream_real_cbuf.py PASS when it exits 0 with a build tree present
- [ ] #2 A real failure of that test (green half failing) is still reported FAILED: shown with a mutant
<!-- AC:END -->
