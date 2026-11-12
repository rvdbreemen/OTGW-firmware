---
id: TASK-1872
title: >-
  run_tests.py: an in-test mutant's 'FAILED (n failures)' line marks
  test_stream_real_cbuf.py as FAILED although rc=0 and RESULT: PASS
status: To Do
assignee: []
created_date: '2026-11-12T19:02:00Z'
labels:
  - tooling
  - tests
dependencies: []
priority: low
ordinal: 946000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1757 beta.218 full run on 105ac46fd (2026-11-12): test/host/test_stream_real_cbuf.py exits rc=0 and prints 'RESULT: PASS' and 'PASS: green with the presize, red without it', but its built-in mutant section (OTGW_NO_PRESIZE=1, must fail) prints a line 'FAILED (4 failure(s))'. scripts/run_tests.py classify_py() line 105 marks any output line matching ^FAILED as a failure, so the runner reports files 1 passed 0 failed 1 on every run. Fix options: (A) anchor the runner on unittest's final summary only (e.g. match ^FAILED \(failures=|errors= after a ^Ran N tests line), or (B) have the mutant section print a different word (e.g. 'MUTANT RED (n failure(s))'). Coordinator: no needs-maintainer-ok.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 run_tests.py -j 1 test_stream_real_cbuf.py reports passed 1 failed 0 on the fixed tree and passed 0 failed 1 on the old tree
- [ ] #2 A unittest file that really fails is still reported FAILED by run_tests.py (shown with a deliberately failing test)
<!-- AC:END -->
