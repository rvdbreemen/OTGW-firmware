---
id: TASK-3020
title: >-
  Host tests: make the C++ test harnesses build and pass with g++/musl (Alpine)
  so the NAS full suite runs them
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T18:10:00Z'
updated_date: '2026-11-21T18:31:00Z'
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
<!-- SECTION:NOTES:END -->
