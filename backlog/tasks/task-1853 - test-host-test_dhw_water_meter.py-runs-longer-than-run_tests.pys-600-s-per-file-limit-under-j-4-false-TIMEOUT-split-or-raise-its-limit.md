---
id: TASK-1853
title: >-
  test/host/test_dhw_water_meter.py runs longer than run_tests.py's 600 s
  per-file limit under -j 4 (false TIMEOUT); split or raise its limit
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T00:42:00Z'
updated_date: '2026-11-16T08:17:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 932000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-12 (otgw-firmware-d3): the full tests folder on integ-188-190 reported TIMEOUT 600.9 s for test/host/test_dhw_water_meter.py; run alone with timeout 590 it was also cut off in the middle of its mutant runs, and the last printed line was a mutant's expected 'CASE X6 FAIL', which looked like a real failure. On a clean export of origin ba41264e6 the same test, given 1500 s, ends RESULT: PASS (exit 0). The test compiles two programs and then a long mutant list (M*/MR*), each a separate compile. Fix one of: give it a per-file limit in scripts/run_tests.py, split the mutant sweep into its own file, or reuse one compile per mutant group. Also make a cut-off run say so (print 'INCOMPLETE' before the mutant list ends) so a truncated log cannot pass for a real X6 failure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 test_dhw_water_meter finishes inside the run_tests.py limit under -j 4 on this machine (or has an explicit larger limit), measured
- [x] #2 A run that is cut off prints a clear incomplete marker instead of ending on a mutant's expected FAIL line
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (comms): change prepared in wt-comms (uncommitted, diff in comms scratchpad t1853/fix.diff): run_tests.py MIN_TIMEOUT test_dhw_water_meter.py 1800 s, PYTHONUNBUFFERED=1 for the child, an INCOMPLETE line after the tail of a TIMEOUT; the test prints '-- mutant i/N <id>: building' before each mutant. tests/test_run_tests_classify.py 7 passed. Evidence pending, scheduled by the coordinator: AC2 old vs new run_tests.py --timeout 90 on this file (during d3's bench window), AC1 one full run tonight after the last build.

2026-11-14 (comms) AC2 red/green, commit charge 0.872, no build running. OLD (runner + test from 7dc396370^): python scripts/run_tests.py --timeout 90 test_dhw_water_meter.py -> TIMEOUT 90.9 s; the 40-line tail ends on '== M10: read-back check removed ... -> failed cases: P7' / '   CASE P7 FAIL' with no marker (the d3 symptom). FIX (runner + test of 0c05372b6, MIN_TIMEOUT entry popped in memory by a wrapper so the 90 s cut still happens; code on disk unchanged): TIMEOUT 90.8 s; the tail ends on '-- mutant 10/17 M19: building (its CASE FAIL lines are expected)' and 'INCOMPLETE: cut off after 91 s before the test printed its verdict'; 8 progress lines in the tail. Side evidence for AC1: the same fix run without the wrapper, --timeout 90, was still running after 300 s, so the 1800 s MIN_TIMEOUT overrides --timeout (stopped by me, no process left). Evidence: comms scratchpad t1853/old.txt, new.txt, run90.py. AC1 (a full run inside the limit, measured) is still open: scheduled tonight by the coordinator.

AC1 evidence (comms 92, this session 2026-11-16 09:13-09:16): python scripts/run_tests.py -j 4 test_dhw_water_meter -> 'PASS 161.5s test/host/test_dhw_water_meter.py', files 1 passed 1 failed 0 timeout 0; inside the default 600 s and the explicit 1800 s MIN_TIMEOUT for this file (scripts/run_tests.py:52-56, TASK-1853 comment). Matches TASK-1956's 160.7 s under -j 4.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
test_dhw_water_meter no longer gives a false TIMEOUT. AC1: fresh run under -j 4 this session: PASS 161.5 s (and TASK-1956: 139 s alone, 160.7 s under -j 4 with heavy neighbours); the file has its own 1800 s limit in scripts/run_tests.py MIN_TIMEOUT (fix 0c05372b6, with the INCOMPLETE marker and mutant progress lines). AC2: old/new red/green recorded in 5f07484b9. Closed by comms 92 from the coordinator's verified close-out list.
<!-- SECTION:FINAL_SUMMARY:END -->
