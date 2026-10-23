---
id: TASK-1335
title: Cap PlatformIO build parallelism at 8 jobs
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T22:26:00Z'
updated_date: '2026-10-23T22:37:00Z'
labels: []
dependencies: []
ordinal: 458000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PlatformIO/scons defaults to one compiler per logical core (--jobs 16 on the maintainer's machine). With many agent sessions open this pushed the machine to 100% CPU and 89% commit charge (2026-10-24). build.py passes --jobs to every platformio run: min(8, cpu_count), overridable with OTGW_BUILD_JOBS.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every platformio run in build.py (firmware, buildfs, safeboot) gets --jobs N with N = min(8, cpu_count) by default
- [x] #2 OTGW_BUILD_JOBS=<n> overrides N; an invalid value falls back to the default
- [x] #3 A test covers the default, the override and the invalid value
- [x] #4 Evidence: a real build shows scons running with --jobs 8 and at most 8 compilers at once
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py passes --jobs N to every platformio run (firmware, buildfs, safeboot): N = min(8, cpu_count), OTGW_BUILD_JOBS=<n> overrides, an invalid value falls back with a notice.
Evidence: tests/test_build_jobs.py 6/6 OK; on the old build.py 5 ERROR + 1 FAIL. Real build.bat --firmware on this 16-core machine: scons command line showed '--jobs 8', peak 8 concurrent cc1plus (before: 14-16), esp32-combo SUCCESS 00:07:49, firmware.bin fresh. evaluate.py --quick exit 0, 0 FAIL.
Not verified: the buildfs and safeboot runs under the cap (only the firmware run was measured; same helper).
<!-- SECTION:FINAL_SUMMARY:END -->
