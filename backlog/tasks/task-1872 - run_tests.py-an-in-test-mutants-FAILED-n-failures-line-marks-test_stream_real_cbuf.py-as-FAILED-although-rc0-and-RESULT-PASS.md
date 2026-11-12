---
id: TASK-1872
title: >-
  run_tests.py: an in-test mutant's 'FAILED (n failures)' line marks
  test_stream_real_cbuf.py as FAILED although rc=0 and RESULT: PASS
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T19:02:00Z'
updated_date: '2026-11-12T22:04:00Z'
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
- [x] #1 run_tests.py -j 1 test_stream_real_cbuf.py reports passed 1 failed 0 on the fixed tree and passed 0 failed 1 on the old tree
- [x] #2 A unittest file that really fails is still reported FAILED by run_tests.py (shown with a deliberately failing test)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 fix (ba): classify_py() in scripts/run_tests.py now decides on the exit code plus unittest's own summary line only (^FAILED \((failures|errors|unexpected successes)=), not on any line starting with FAILED. Option A of the description. Checked first which test files print a line starting with FAILED themselves: only C++ harnesses (tests/test_sat_cold_hysteresis.cpp, test/host/test_rest_body_stale.cpp, test_stream_real_cbuf.cpp, test_stream_size_hint.cpp); their Python wrappers exit non-zero on a real failure, so the exit code still catches those.

Evidence: new tests/test_run_tests_classify.py (7 cases, calls the real classify_py() via importlib): on the old classifier 1 of 7 fails (the cbuf-style mutant output with rc 0 was FAILED, expected PASS); on the fix 7/7 OK. run_tests -j 1 test_stream_real_cbuf.py: old tree (105ac46fd, 2026-11-12) passed 0 failed 1, twice; fix tree passed 1 failed 0 (needs .pio/libdeps, copied from the main tree; platformio.ini identical, without it the test SKIPs). A deliberately failing unittest file (tests/test_zz_tmp1872_fail.py, removed after) still reports passed 0 failed 1. Full tests folder on the fix tree (lane/ba-1872 off c97d93659), one file per process, -j 1: 224 files (223 basenames), all PASS, 0 failed, 0 skipped; file 159 test_sat_requested_vs_transmitted failed once in a chunk run under 88 % commit charge while a release build ran, passed directly (10 tests, 89 s) and again through run_tests at 82 %. evaluate.py --quick: see final summary. No firmware change, no bump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/run_tests.py classify_py() now marks a Python test file FAILED only on a non-zero exit code or on unittest's own summary line (FAILED (failures=..)/(errors=..)/(unexpected successes=..)), so a C++ harness line like 'FAILED (4 failure(s))' printed by a passing test's built-in mutant no longer flips it. AC1: test_stream_real_cbuf.py via run_tests -j 1 was passed 0 failed 1 on the old tree (105ac46fd, twice) and is passed 1 failed 0 on the fix (needs .pio/libdeps). AC2: a deliberately failing unittest file still reports failed 1; new tests/test_run_tests_classify.py (7 cases on the real function) is red on the old classifier (1/7) and green on the fix (7/7), covering rc != 0, unittest FAILED summaries with rc 0, OK, 0 tests (SKIP), SKIP marker, timeout. Gates: full tests folder 224 files on the fix tree, all PASS, 0 skipped; evaluate.py --quick 92 checks, 85 PASS, 0 FAIL. Tooling and tests only, no firmware change, no bump. Not checked: Linux/macOS (the harness files are Windows/MSVC only).
<!-- SECTION:FINAL_SUMMARY:END -->
