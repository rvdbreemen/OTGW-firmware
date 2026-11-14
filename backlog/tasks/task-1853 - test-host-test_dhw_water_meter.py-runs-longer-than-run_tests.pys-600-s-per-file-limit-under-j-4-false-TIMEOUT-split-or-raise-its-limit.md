---
id: TASK-1853
title: >-
  test/host/test_dhw_water_meter.py runs longer than run_tests.py's 600 s
  per-file limit under -j 4 (false TIMEOUT); split or raise its limit
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T00:42:00Z'
updated_date: '2026-11-14T15:59:00Z'
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
- [ ] #1 test_dhw_water_meter finishes inside the run_tests.py limit under -j 4 on this machine (or has an explicit larger limit), measured
- [ ] #2 A run that is cut off prints a clear incomplete marker instead of ending on a mutant's expected FAIL line
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (comms): change prepared in wt-comms (uncommitted, diff in comms scratchpad t1853/fix.diff): run_tests.py MIN_TIMEOUT test_dhw_water_meter.py 1800 s, PYTHONUNBUFFERED=1 for the child, an INCOMPLETE line after the tail of a TIMEOUT; the test prints '-- mutant i/N <id>: building' before each mutant. tests/test_run_tests_classify.py 7 passed. Evidence pending, scheduled by the coordinator: AC2 old vs new run_tests.py --timeout 90 on this file (during d3's bench window), AC1 one full run tonight after the last build.
<!-- SECTION:NOTES:END -->
