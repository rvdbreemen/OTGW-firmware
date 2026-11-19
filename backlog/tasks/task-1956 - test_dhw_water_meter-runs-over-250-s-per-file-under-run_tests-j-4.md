---
id: TASK-1956
title: test_dhw_water_meter runs over 250 s per file under run_tests -j 4
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T03:22:00Z'
updated_date: '2026-11-15T13:36:00Z'
labels:
  - test
  - tooling
dependencies: []
priority: low
ordinal: 998000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
In run_tests --changed (beta.282-286 lane, 2026-11-15) test/host/test_dhw_water_meter.py took 247-285 s and hit the per-file timeout under -j 4, also on origin/dev 617789d34, ending in 'COMPILATION FAILED (MRn ...)' for the mutant being built when it was cut off (34 mutants, each a host compile). Not caused by the lane (it touches only banners there). Structural: the mutant count times the MSVC compile time. Options: compile the mutants in parallel, cache the unchanged object, or mark it slow so the chunked foreground runs give it its own slot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Measure the file alone (wall time, per-mutant compile time)
- [x] #2 The file finishes under the run_tests default timeout at -j 4, or is listed as slow with its own run instruction; red/green behaviour of the mutants unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 ba. AC1 measured (the file alone, idle-ish machine): export 0.2 s, one build_and_run 10.6 s of which the vcvars call alone is 7.9 s; about 34 builds (fix, 51 mutant entries over the meter and discovery drivers, discovery fix). So vcvars, not cl, was most of the 247-285 s. Fix (test only): build_and_run() captures the vcvars environment once (a bat that calls vcvars and prints 'set'), resolves cl.exe on that PATH (CreateProcess searches the parent's PATH, not env's) and runs cl directly with the same flags; every mutant is still compiled and run as before. Evidence: alone 139 s, RESULT PASS, 119 verdict checks; the origin version in the same session: 577 s (machine busier then, so that ratio is rough), RESULT PASS, the same 119 verdict lines, identical line for line (diff empty), so every mutant is still killed as before. Under run_tests.py -j 4 next to test_fs_ota_rtc_snapshot, test_refresh_storm and test_sat_pm25_poll: 160.7 s, PASS, total 281.5 s; well under the 600 s default. Follow-up idea, not done: other MSVC harnesses call vcvars per build too; the same cache in test_ot_reserved_range.find_vcvars' callers would speed up the whole host suite.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
test_dhw_water_meter took 247-285 s alone and ran into the per-file timeout under -j 4. AC1: per build 10.6 s, of which vcvars 7.9 s, about 34 builds. AC2: vcvars environment captured once and cl.exe run directly with it; alone 139 s, under -j 4 with three heavy neighbours 160.7 s (default timeout 600 s); the 119 verdict lines (every mutant killed, every keep passing) are identical to the origin version's. Test-only change.
<!-- SECTION:FINAL_SUMMARY:END -->
