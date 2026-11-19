---
id: TASK-1878
title: >-
  tests/test_sat_curve_selftune.py leaves sat_tune_test.json in the repo root
  after a test run
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T22:07:00Z'
updated_date: '2026-11-15T10:50:00Z'
labels:
  - tests
  - tooling
dependencies: []
priority: low
ordinal: 947000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1872 full run (2026-11-12): an untracked sat_tune_test.json appears in the repo root of every tree after the full tests folder (seen in wt-ba-1757, wt-ba-1872 and the main dev tree). Cause (read from the code, not yet reproduced in isolation): the compiled harness in tests/test_sat_curve_selftune.py uses g_tuneFile() (line 85) = getenv(SATTUNE_FILE) or the relative default sat_tune_test.json; the run at line 294-300 sets SATTUNE_FILE to a temp file, but the run at line 899 starts the exe without env and without cwd, so the file lands in the runner's cwd (scripts/run_tests.py runs every test with cwd=ROOT). Fix: pass SATTUNE_FILE into a TemporaryDirectory for that run too (or run it with cwd in the temp dir). Tests only, no firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After python scripts/run_tests.py -j 1 test_sat_curve_selftune.py in a clean tree, git status shows no sat_tune_test.json, and the same check on the old code shows the file appearing
- [x] #2 test_sat_curve_selftune.py stays green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 (comms, TASK-1883 run): confirmed again. Running python scripts/run_tests.py --no-host tuner_convergence curve_selftune in wt-comms left sat_tune_test.json in the repo root (removed by hand); same cause tests/test_sat_curve_selftune.py:898-899 (test_removing_one_exclusion_is_caught runs the exe without SATTUNE_FILE and without cwd). The untracked sat_tune_test.json in the main dev tree fits this.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause confirmed: ExclusionMutants.test_removing_one_exclusion_is_caught ran the mutant exe without SATTUNE_FILE, so g_tuneFile() fell back to the relative sat_tune_test.json in the runner's cwd (repo root). Fix: that run gets SATTUNE_FILE in the class TemporaryDirectory (tests/test_sat_curve_selftune.py:923-926). AC1: OLD: rm sat_tune_test.json; python scripts/run_tests.py -j 1 test_sat_curve_selftune.py -> PASS 172.1 s (95 tests, skipped=1), git status shows '?? sat_tune_test.json'. FIX: same commands -> PASS 149.2 s, git status shows no sat_tune_test.json. AC2: the test stays green (above). The other tests that import from test_sat_curve_selftune (cold_hysteresis_day, pwm_phase_telemetry, sim_thermal_learning, thermal_sim_persist) use only its compile helper, not the tune-file harness: no same leak. Tests only, no firmware change.
<!-- SECTION:FINAL_SUMMARY:END -->
