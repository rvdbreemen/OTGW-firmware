---
id: TASK-1854
title: test_stream_real_cbuf fails under run_tests.py -j 4, passes alone
status: To Do
assignee: []
created_date: '2026-11-12T02:00:00Z'
labels:
  - tests
  - flaky
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 932000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by ba (relayed by the coordinator, 2026-11-12) on 4cafc97eb: scripts/run_tests.py -j 4 gave 208/210 passed; test/host/test_stream_real_cbuf.py failed under the parallel run and passed when run alone. Suspected: a shared temp file or port, or a timing assumption. Possibly the same cause as TASK-1810: the test exits 0 with RESULT: PASS, then runs its deliberate red mutant (OTGW_NO_PRESIZE=1) whose output has lines starting with FAILED, which run_tests.py's classifier (^FAILED) reports as a failure; that only shows when a .pio build tree is present (else the test SKIPs, TASK-1723). The harness writes its slices and exe to test/host/generated/ (build_and_run_stream_real_cbuf.ps1:26, test_stream_real_cbuf.py:21). Not reproduced by the filer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The -j 4 failure is reproduced (command, run_tests.py output) or shown to be the TASK-1810 classifier issue; if it is, this task closes as a duplicate of TASK-1810 with that evidence
- [ ] #2 The root cause is named with file:line
- [ ] #3 The fix makes the test pass under scripts/run_tests.py -j 4 (shown), and a real failure of the green half is still reported FAILED
<!-- AC:END -->
