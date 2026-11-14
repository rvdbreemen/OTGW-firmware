---
id: TASK-1921
title: >-
  Tooling: every esp32-combo build is cold; find which build.py step removes the
  .pio objects
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T04:36:00Z'
updated_date: '2026-11-14T12:33:00Z'
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
- [x] #1 the step that removes the .pio object files is named with file:line and shown in a log of one build
- [x] #2 decision recorded: keep (with the reason) or change so a second build without source changes recompiles only what changed
- [x] #3 if changed: a second esp32-combo build with no source change shows far fewer Compiling lines than the first, and the zip is identical in content to a clean build (checked against docs/guides/versioning-policy.md build-stamp restore)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Prove the cause without a build (machine lock): read PlatformIO 6.2.0 in .build-venv. 2. Minimal fix in build.py only: the buildfs step gets its own build dir; artifacts and the one-build lock unchanged. 3. Host test old-vs-fix in tests/test_build.py. 4. Two consecutive esp32-combo builds in my slots: the first cold (the dir is wiped now), the second warm with far fewer Compiling lines; compare the artifacts (littlefs identical, ino.bin differing only in the build stamp).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 cause (proven on PlatformIO's code, no build): platformio/run/helpers.py:26-45 clean_build_dir() compares <build_dir>/project.checksum with compute_project_checksum(config) (platformio/project/helpers.py:93, config.to_json() + PIO version + source path list) and fs.rmtree(build_dir) on a mismatch (helpers.py:41). build.py build_filesystem_pio() sets PLATFORMIO_DATA_DIR=.pio/data-staging for 'pio run -t buildfs'. Measured with the build venv: checksum without the override b16a76e10b81c30f051c3ec4742bda3469e0f4bc, with it a52dde20dc6f340b1399c0a0192a8f41dbea4062; .pio/build/project.checksum after build 240 = a52dde... and .pio/build/esp32-combo holds only littlefs.bin. So the buildfs run wipes the firmware objects, and the next firmware run wipes again: every build is cold. Not the cause: create_build_directory() rmtree(config.BUILD_DIR) at build.py:650 removes build/ (the artifact dir), and clean_build() at :2594 runs only with --clean/--distclean (build.py:3211-3214).

Fix (AC2: change): build_filesystem_pio sets PLATFORMIO_BUILD_DIR=.pio/build-fs (pio_fs_build_root()), verifies and collects littlefs.bin there; clean_build also removes .pio/build-fs. .pio/build keeps one checksum and its objects. No change to the lock, the jobs cap or the artifact names. Tests: tests/test_build.py TestFilesystemStepKeepsFirmwareObjects (3): on the fix 3/3 OK; on origin/dev build.py (OTGW_BUILD_PY) test_buildfs_runs_in_its_own_build_directory FAILS ('the buildfs step must not run in .pio/build'); the checksum test pins the cause on PlatformIO's own compute_project_checksum (passes on both, it tests PlatformIO). Full test_build.py 16/16 under the build venv and the system Python. AC3 needs two builds in my slots.

Coordinator review: (1) ADR-237 (Proposed, via adr-kit, renumbered from the auto-allocated 234 because 234-236 are taken on other lanes) records the build-tooling change; related ADR-190, ADR-083; README entry; adr governance OK. (2) Other readers of .pio/build/<env>/littlefs.bin: only flash_otgw.bat:383 as its last fallback (after ./ and build/), now also .pio\build-fs\*; scripts/ota_flash.py and flash_otgw.sh use build/; flash_esp.py reads only bootloader/partitions from .pio/build/esp32; safeboot is its own PIO project (build.py:2252); scripts/device-verify.sh runs pio buildfs itself without an override (unaffected, its default dir).

AC3 measured 2026-11-14 on lane-95-1921 36fb13763 (no src change between the builds; build.bat, Start-Process, 4 jobs; logs in 95's scratchpad b1921/build1.log, build2.log, status.txt). Build 1 (cold: .pio/build/esp32-combo was empty): exit 0, 609 s, 364 Compiling lines, 2 'Successfully created ESP32S3 image' (bootloader + firmware). After it: 364 .o files kept in .pio/build/esp32-combo, .pio/build/project.checksum = b16a76e1... (the firmware run's), .pio/build-fs/project.checksum = fbf867c9... Build 2 (warm): exit 0, 253 s, 1 Compiling line (src/OTGW-firmware.ino.cpp.o, the sketch: version.h is restamped each build), 1 image line (firmware; the bootloader comes from the cache), 364 .o kept, same checksums. Before the fix every build looked like build 1 (d3's 236 rerun: 364 Compiling lines).

AC3 artifacts (art1 vs art2, same githash 36fb137): flash zip has the same 14 entries; only .ino.bin and the two merged images that contain it differ. littlefs.bin: unpacked with mklittlefs -u, the 10 files are identical; the image differs in 204 bytes in blocks 0, 1 and 30 (LittleFS metadata: the staging copy is recreated per build, so file times differ). .ino.bin (same size 2147904): 137 bytes differ in 6 runs: the ELF sha256 at 0xb0 (32 B), the build time string at 0x5656e ('07:54' vs '12:47'), three small runs of Xtensa immediates (0xf2cf4 5 B, 0xf44c0 46 B, 0x1061c7 19 B, values that move with the build time), and the image hash at the end (33 B). Not proven byte by byte that the immediates are only the stamp: a cold and a warm build cannot carry the same _VERSION_TIME. Side effect recorded: the image-created count is 2 on a cold build and 1 on a warm one; .claude/skills/beta-release-2x and release-2x updated (they said 'expect 2'). Stamps restored with git checkout after the builds.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: the step is PlatformIO's own clean_build_dir() (.build-venv/Lib/site-packages/platformio/run/helpers.py:26-45, rmtree at :41), triggered because build.py build_filesystem_pio() set PLATFORMIO_DATA_DIR for pio run -t buildfs: project checksum b16a76e1... without, a52dde20... with the override (compute_project_checksum, platformio/project/helpers.py:93). State after build 240: .pio/build/project.checksum = a52dde..., esp32-combo held only littlefs.bin. build.py:650 (build/) and clean_build :2594 (--clean only) are not the cause. AC2: changed, ADR-237 (Proposed): the buildfs step runs with PLATFORMIO_BUILD_DIR=.pio/build-fs (pio_fs_build_root), verify/collect read littlefs.bin there, --clean removes it, flash_otgw.bat falls back to it. tests/test_build.py TestFilesystemStepKeepsFirmwareObjects: 3/3 on the fix, the core test fails on origin's build.py; test_build 16/16; evaluate --quick 86/0; adr governance OK. AC3: two builds on 36fb13763 without a source change: cold 609 s / 364 Compiling / 2 image lines; warm 253 s / 1 Compiling (the sketch) / 1 image line (bootloader cached); 364 .o kept. Zip: same 14 entries; littlefs files identical (metadata/file times differ); ino.bin differs in 137 bytes (ELF sha, build-time string, time-dependent immediates, image hash). Not proven byte by byte that the immediates are only the stamp. Release skills updated for the 1-vs-2 image-line count.
<!-- SECTION:FINAL_SUMMARY:END -->
