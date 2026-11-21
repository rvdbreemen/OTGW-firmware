---
id: TASK-3020
title: >-
  Host tests: make the C++ test harnesses build and pass with g++/musl (Alpine)
  so the NAS full suite runs them
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T18:10:00Z'
updated_date: '2026-11-21T19:58:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1135000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-21 (via coordinator, 'port now', overrides the 25 Nov deferral for this item only). 09's NAS baseline at 2f26146ce (image otgw-bench-py312-gxx:2026-11-21): 62 failing files, list + log in /volume1/docker/otgw-bench/runs/suite-2f26146ce/run_tests-gxx.log. Typical causes: a harness-defined static strlcpy clashing with musl's extern one, isnan without std::, Windows file-lock semantics in test_build_lock. Rules: test harness files only (tests/**, test/host/**), no src/ change (stop and ask the coordinator if one seems needed), MSVC keeps working (single files on the laptop), NAS runs through otgw-bench-tester.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every C++ host test file that fails on g++/musl at 2f26146ce builds and passes there, or is listed with the reason it cannot (e.g. needs a src change)
- [ ] #2 The same files still pass with MSVC on the laptop (spot check per batch, single files)
- [ ] #3 A NAS full-suite run via otgw-bench-tester shows the before/after count of failing files
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 batch 1 (lane-5e-gxx, tests only, no src/): baseline 09 at 2f26146ce: 62 failing files (OTGW-field-data/bench-nas/2026-11-21_suite-gxx-2f26146ce). Causes and fixes: (a) 18 files: harness 'static strlcpy/strlcat' vs musl's extern declaration -> '#define strlcpy otgw_test_strlcpy' (strlcat likewise) after the includes, before the harness definition (sonnet subagent); (b) 17 files: isnan/isfinite/isinf without std:: -> '#include <cmath>' + 'using std::isnan; using std::isfinite; using std::isinf;' in the harness preamble (subagent; shared preambles in test_sat_pid_cadence, test_sat_cycle_classifier, test_sat_curve_selftune cover the rest); (c) 4 files: MSVC-only builds (find_vcvars) -> new host_cxx_build() in test/host/test_ot_reserved_range.py (cl on Windows, g++/clang++ elsewhere), used by test_ha_discovery_json.build_and_run (ble_discovery_1462, discovery_drip_per_entity, sat_health_discovery) and test_diag_decode (its 'except Exception' did not catch SystemExit, so it failed instead of skipping); (d) 3 files: sat_loop_build.cl_compile MSVC-only -> gxx_compile() fallback; test_sat_pwm_first_start / test_sat_thermal_learning_gate mkdir(exist_ok=True) (the FileExistsError hid the skipped first build); (e) test_build_lock: a fixed 0.8 s wait was too short on the NAS -> wait until the first child holds the lock (passes on Windows and WSL Linux); (f) test_capture_tool: the fake device's reset closed a socket a thread was blocked in recv() on: on Linux nothing went out -> shutdown(SHUT_RDWR) first (7 failures reproduced under WSL, now OK; Windows OK); (g) test_progmem_assets_deflated: compared deflate bytes, which depend on the zlib build -> compares the unpacked content of every array plus the header layout. Environment (not tests): 5 files need git, 5 need numpy: image :2026-11-21b (09, Robert OK). MSVC: every touched file run as a single file on the laptop: OK. Not verified: g++/musl (needs the NAS run). Not in scope yet: the 33 test/host harnesses that run_tests.py leaves out on Linux (MSVC-only, never run on the NAS).

2026-11-21 NAS run of batch 1 (09, 0a71e7147, image :2026-11-21b; OTGW-field-data/bench-nas/2026-11-21_suite-gxx-0a71e7147/ incl. single/<file>.log): 323 files, 310 passed, 12 failed (was 62). Batch 1b fixes: tests/sat_loop_stubs/Arduino.h used _stricmp/_strnicmp unconditionally -> POSIX strcasecmp via <strings.h> on non-MSVC (loop_integration, pwm_first_start, thermal_learning_gate); the shared STUBS of test_sat_curve_selftune kept an unguarded static strlcpy (curve_selftune, tuner_convergence); test_sat_mqtt_contract_1485 and test_sat_telemetry_wire (also the cyc.cpp of test_sat_lowload_contract) strlcpy renamed, <strings.h> for the non-MSVC strcasecmp; test_ha_climate_sat_follow std:: usings. MSVC single files OK: ha_climate_sat_follow, mqtt_contract_1485, telemetry_wire, curve_selftune (skipped=1), tuner_convergence, thermal_learning_gate, loop_integration, pwm_first_start, lowload_contract. Environment (09): test_otd_loop_task_commands and test_sat_pwm_live_flow_duty_2993 need git history (origin/dev, 939f9406b), test_otd_ps1_outside_temp needs the other-projects submodule: 09 runs from a clone instead of a git archive.

Correction to the note above: test_sat_lowload_contract was NOT OK on MSVC in this worktree: LL-021 crashed (AttributeError: '_ErrorHolder' has no _testMethodName) because other-projects/SAT-releases-thermo-nova is not checked out here and the delegated parity classes skip in setUpClass. delegated_results() now names such holders, so LL-021 reports 'skipped: thermo-nova upstream not runnable' instead of crashing the run (pre-existing weakness on origin/dev, not caused by the g++ port). With the submodule present (main tree, a full NAS clone) LL-021 is expected to pass; not verified in this worktree.
<!-- SECTION:NOTES:END -->
