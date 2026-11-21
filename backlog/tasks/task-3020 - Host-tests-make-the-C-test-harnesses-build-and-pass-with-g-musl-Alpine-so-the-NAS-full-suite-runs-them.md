---
id: TASK-3020
title: >-
  Host tests: make the C++ test harnesses build and pass with g++/musl (Alpine)
  so the NAS full suite runs them
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T18:10:00Z'
updated_date: '2026-11-21T22:00:00Z'
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

2026-11-21 batch 3 (lane-5e-gxx): the 30 MSVC-only test/host harnesses build through host_cxx_build() (sonnet subagent; per file one or more compiles converted, same sources/includes/defines, same failure messages and exit codes; /Od and /W4 dropped, /utf-8 and _CRT_SECURE_NO_WARNINGS now on every MSVC build); 8 test/host .cpp got the strlcpy rename, 2 the strcasecmp mapping; test_dhw_water_meter keeps its cached vcvars path on Windows and uses host_cxx_build elsewhere (my own first edit wrote a backspace byte for \b in its CASE regex; caught by a byte scan, fixed). scripts/run_tests.py (coordinator OK): test/host runs on a non-Windows host with --host-gxx or OTGW_HOST_GXX=1 (default stays off there until the NAS run is green; Windows unchanged: 35 test/host files listed). MSVC single files: all 30 + test_dhw_water_meter pass. Finding, not fixed (pre-existing on origin/dev, could turn the tests red): test/host/test_diag_recording_routes.py:69 and test/host/test_reboot_log_reset_raw.py:126 contain a backspace byte where a regex \b was meant, so those checks are weaker than written; separate task worth considering.

2026-11-21 NAS run of batch 1b (09, d802486cd, full shallow clone + submodules, image b; OTGW-field-data/bench-nas/2026-11-21_suite-gxx-d802486/): 323 files, 317 passed, 4 failed, 1 timeout (was 12; git-history and otgw-6.8 tests pass from the clone). Batch 1c: tests/sat_loop_build.py g++ build gets -fpermissive (the assembled unit declares satPublishDeferred/mqttConnectPaceHold/localtime_r extern and defines static stubs; MSVC accepts that, g++ only permissive) for loop_integration, pwm_first_start, thermal_learning_gate; test_sat_curve_selftune compile_cpp builds with -O2 under g++ (test_sat_tuner_convergence passed the 900 s timeout unoptimised on the NAS). Not g++: LL-021 of test_sat_lowload_contract fails on any fresh clone because the other-projects gitlink on dev (2b01ba65) has no SAT-releases-thermo-nova (only SAT-develop); test_sat_classifier_parity skips then and lowload counts the skip as a failure: repo/submodule question for the coordinator.

2026-11-21 coordinator (Robert removed SAT-releases-thermo-nova on purpose, ADR-292/TASK-3013; parity tests get repointed at SAT-develop later): option 2: test_sat_lowload_contract.delegated_results() counts the parity classes' 'SAT-releases-thermo-nova is not checked out' skip as a skip, not a failure, with a comment pointing to ADR-292/TASK-3013. Laptop worktree without the folder: lowload OK (skipped=1), LL-021 reported as pass (its other delegated suite, test_sat_cycle_classifier, runs). Note: LL-021 then rests on the cycle classifier alone until the repoint.

2026-11-21 NAS run batch 3 (09, 0bcc64967, --host-gxx; OTGW-field-data/bench-nas/2026-11-21_suite-gxx-0bcc649/): 358 files, 345 passed, 11 failed, 1 timeout; 5 of those already addressed by 1c/1d. Batch 3b for the 7 new test/host ones: roundf/isnan unqualified in sliced firmware code (core.inc, sat_pid_slice.inc) -> <cmath> + using std::... in test_boiler_unsupported_origin.cpp, test_local_frame_consumers.cpp, test_sat_pid_derivative.cpp; dhw_host_shim.h _stricmp -> strcasecmp off MSVC; test_slave_config_evidence.cpp _itoa -> a portable utoa off MSVC; test_reboot_log_reset_raw S4 needs the pinned PlatformIO libs (absent on the NAS) -> explicit 'skip' with reason off Windows, and S4 left out of the old-code must-fail list there; test_stream_real_cbuf is a PowerShell+MSVC script -> SKIP without PowerShell, the same way it already skips without a firmware build. MSVC single files: boiler_unsupported_origin, local_frame_consumers, sat_pid_derivative, slave_config_evidence, dhw_water_meter, reboot_log_reset_raw: PASS; stream_real_cbuf: unchanged (SKIP, no firmware build in this worktree); under WSL it prints the new SKIP.

2026-11-21 deciding NAS run on c5238004e (09, --host-gxx, OTGW-field-data/bench-nas/2026-11-21_suite-gxx-c5238004e/): 358 files, 351 passed, 4 failed, 1 timeout, 2 skipped. Batch 3c: tests/sat_loop_stubs.h defined localtime_r over the MSVC-only localtime_s on every compiler -> only under _MSC_VER (POSIX has localtime_r) [loop_integration, pwm_first_start, thermal_learning_gate]; test_reboot_log_reset_raw mutant MC4 expects S4, which is a skip without the pinned libs -> MC4 skipped there too; test_sat_tuner_convergence gets a 2400 s per-file timeout in run_tests.py MIN_TIMEOUT (past 900 s on the NAS even with -O2; 33 s on the laptop). MSVC: thermal_learning_gate OK, reboot_log_reset_raw PASS.
<!-- SECTION:NOTES:END -->
