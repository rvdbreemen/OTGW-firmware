---
id: TASK-1921
title: >-
  Tooling: every esp32-combo build is cold; find which build.py step removes the
  .pio objects
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T04:36:00Z'
updated_date: '2026-11-14T11:35:00Z'
labels:
  - tooling
  - build
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 984000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-11-14 on beta.234 and beta.236 (d3). A restart right after a build that was reaped at the LTO link recompiled everything: 364 Compiling lines in the second 236 run (189 NimBLE libf51, 55 FrameworkArduino), against 451 in the first run (which also built the safeboot). Libraries do not include version.h, so the version.h stamp is not the cause (an earlier guess, retracted). After the build .pio/build/esp32-combo held only .sconsign312.dblite and littlefs.bin (05:35); the library object dirs were gone. Candidates, not yet proven: build.py create_build_directory() rmtree(config.BUILD_DIR) at :650 (logged 'Cleaning existing build directory...' twice per build) and clean_build() :2594 (removes .pio). Cost: about 10 to 14 min and the full LTO memory peak per build; that peak is where Claude Code's memory reaper stopped background build shells three times tonight (builds 234, 236 and earlier 227/220). Investigate only with no build running on the machine (machine-wide lock).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 the step that removes the .pio object files is named with file:line and shown in a log of one build
- [ ] #2 decision recorded: keep (with the reason) or change so a second build without source changes recompiles only what changed
- [ ] #3 if changed: a second esp32-combo build with no source change shows far fewer Compiling lines than the first, and the zip is identical in content to a clean build (checked against docs/guides/versioning-policy.md build-stamp restore)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Prove the cause without a build (machine lock): read PlatformIO 6.2.0 in .build-venv. 2. Minimal fix in build.py only: the buildfs step gets its own build dir; artifacts and the one-build lock unchanged. 3. Host test old-vs-fix in tests/test_build.py. 4. Two consecutive esp32-combo builds in my slots: the first cold (the dir is wiped now), the second warm with far fewer Compiling lines; compare the artifacts (littlefs identical, ino.bin differing only in the build stamp).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 cause (proven on PlatformIO's code, no build): platformio/run/helpers.py:26-45 clean_build_dir() compares <build_dir>/project.checksum with compute_project_checksum(config) (platformio/project/helpers.py:93, config.to_json() + PIO version + source path list) and fs.rmtree(build_dir) on a mismatch (helpers.py:41). build.py build_filesystem_pio() sets PLATFORMIO_DATA_DIR=.pio/data-staging for 'pio run -t buildfs'. Measured with the build venv: checksum without the override b16a76e10b81c30f051c3ec4742bda3469e0f4bc, with it a52dde20dc6f340b1399c0a0192a8f41dbea4062; .pio/build/project.checksum after build 240 = a52dde... and .pio/build/esp32-combo holds only littlefs.bin. So the buildfs run wipes the firmware objects, and the next firmware run wipes again: every build is cold. Not the cause: create_build_directory() rmtree(config.BUILD_DIR) at build.py:650 removes build/ (the artifact dir), and clean_build() at :2594 runs only with --clean/--distclean (build.py:3211-3214).

Fix (AC2: change): build_filesystem_pio sets PLATFORMIO_BUILD_DIR=.pio/build-fs (pio_fs_build_root()), verifies and collects littlefs.bin there; clean_build also removes .pio/build-fs. .pio/build keeps one checksum and its objects. No change to the lock, the jobs cap or the artifact names. Tests: tests/test_build.py TestFilesystemStepKeepsFirmwareObjects (3): on the fix 3/3 OK; on origin/dev build.py (OTGW_BUILD_PY) test_buildfs_runs_in_its_own_build_directory FAILS ('the buildfs step must not run in .pio/build'); the checksum test pins the cause on PlatformIO's own compute_project_checksum (passes on both, it tests PlatformIO). Full test_build.py 16/16 under the build venv and the system Python. AC3 needs two builds in my slots.
<!-- SECTION:NOTES:END -->
