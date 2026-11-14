---
id: TASK-1810
title: >-
  run_tests.py marks test/host/test_stream_real_cbuf.py FAILED although it
  passes
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T07:39:00Z'
updated_date: '2026-11-14T15:49:00Z'
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
- [x] #1 run_tests.py reports test_stream_real_cbuf.py PASS when it exits 0 with a build tree present
- [x] #2 A real failure of that test (green half failing) is still reported FAILED: shown with a mutant
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (comms): verified that TASK-1872 (789dc56b5, run_tests.py classifies on exit code and the unittest summary only) fixes this. Real run in wt-comms (own copy of .pio/libdeps/esp32-combo/ESPAsyncWebServer, PLATFORMIO_CORE_DIR D:\DevData\platformio): python test/host/test_stream_real_cbuf.py -> exit 0, 'RESULT: PASS', mutant output contains 'FAILED (4 failure(s))', 13 s. The same captured output through classify_py: old run_tests.py (789dc56b5^) FAILED, current PASS. Mutants: rc=1 (green half fails) -> old FAILED, new FAILED; an added unittest summary 'FAILED (failures=1)' with rc=0 -> old FAILED, new FAILED. End to end: python scripts/run_tests.py test_stream_real_cbuf.py -> PASS 14.0 s, files 1 passed 1 failed 0. Evidence: comms scratchpad t1810/ (direct.txt, run_tests_old.py).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: with a build-tree library present, python scripts/run_tests.py test_stream_real_cbuf.py reports PASS (14.0 s, 1 passed, 0 failed); the same real output (exit 0, RESULT: PASS, mutant line 'FAILED (4 failure(s))') is FAILED under the old classify_py (789dc56b5^) and PASS under the current one. AC2: a real failure is still FAILED: rc=1 (green half failing) and a unittest 'FAILED (failures=1)' summary both classify FAILED with the current code. Fixed by TASK-1872 (789dc56b5); no code change in this task. Not verified: the -j 4 parallel behaviour of TASK-1854 (run here with -j 4 but only one file).
<!-- SECTION:FINAL_SUMMARY:END -->
