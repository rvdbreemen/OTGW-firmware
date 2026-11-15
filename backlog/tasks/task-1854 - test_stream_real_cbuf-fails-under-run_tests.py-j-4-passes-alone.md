---
id: TASK-1854
title: test_stream_real_cbuf fails under run_tests.py -j 4, passes alone
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T02:00:00Z'
updated_date: '2026-11-14T21:14:00Z'
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
- [x] #1 The -j 4 failure is reproduced (command, run_tests.py output) or shown to be the TASK-1810 classifier issue; if it is, this task closes as a duplicate of TASK-1810 with that evidence
- [x] #2 The root cause is named with file:line
- [x] #3 The fix makes the test pass under scripts/run_tests.py -j 4 (shown), and a real failure of the green half is still reported FAILED
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (comms), wt-comms at 99178381e, PLATFORMIO_CORE_DIR D:\DevData\platformio, between two builds of 95 (coordinator window). Set: python scripts/run_tests.py -j 4 test_stream_real_cbuf test_run_tests_classify test_open_questions_status test_sat_corrections_index. CURRENT runner, 3 runs: test_stream_real_cbuf PASS 16.8 s / 14.5 s / 17.6 s, files 4 passed 4 failed 0 each time. OLD runner (scripts/run_tests.py at 789dc56b5^, i.e. before TASK-1872), same set, -j 4: test_stream_real_cbuf FAILED 18.9 s, 3 passed 1 failed; its output contains the mutant's 'FAILED (4 failure(s))' (line 45 of the capture), which the old classify_py matched with re.search(r'^FAILED', out, re.M) at scripts/run_tests.py:105 (789dc56b5^). So the -j 4 failure is the TASK-1810 classifier issue, fixed by TASK-1872 (789dc56b5). Interpretation of ba's 'passes alone': a direct python run of the test file prints RESULT: PASS and exits 0, which the classifier does not see; the old classifier would also mark it FAILED when run alone through run_tests.py. Real-failure detection under the current classifier was shown in TASK-1810 (rc=1 and a unittest 'FAILED (failures=1)' summary both classify FAILED). Evidence: comms scratchpad t1854/run1-3.txt, old.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as a duplicate of TASK-1810 with evidence. AC1: the -j 4 failure is the TASK-1810 classifier issue: the same 4-file set under -j 4 gives test_stream_real_cbuf FAILED with the pre-TASK-1872 runner (789dc56b5^) and PASS 3/3 with the current one (14.5-17.6 s). AC2: root cause scripts/run_tests.py:105 at 789dc56b5^, classify_py's re.search(r'^FAILED', out, re.M) matched the deliberate mutant's 'FAILED (4 failure(s))' line of a test that exits 0; fixed by TASK-1872 (789dc56b5). AC3: under -j 4 the test passes 3/3 with the current runner (shown); a real failure of the green half is still FAILED (TASK-1810 evidence: rc=1 and a unittest failure summary). No code change in this task. Not verified: a full-folder -j 4 run (memory).
<!-- SECTION:FINAL_SUMMARY:END -->
