---
id: TASK-2988
title: >-
  build.py: rename fails with WinError 183 on a rebuild of the same githash, and
  the consolidation step deletes every build/ subdirectory
status: To Do
assignee: []
created_date: '2026-11-21T01:53:00Z'
labels:
  - build
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1114000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-21 during the TASK-2949.5 heap A/B (two builds of 41827f966 that differ only in a -D flag). (1) rename_build_artifacts() (build.py ~1138-1157) uses Path.rename(); on Windows that raises FileExistsError [WinError 183] when build/OTGW-firmware-esp32-combo-<semver>.ino.bin already exists. It exists because create_build_directory() (build.py ~743-756) shutil.rmtree(build/) fails as soon as one file is locked (here build/logs/build-*.log, WinError 32) and only warns, so the previous build's artifacts stay. Result: exit 1 after a successful compile, no versioned artifacts. (2) The consolidation step after the build (build.py ~1113-1119, comment 'Remove empty subdirectories in build/') calls shutil.rmtree() on EVERY subdirectory of build/, not only empty ones: it deleted build/heapab-nodelay0/ with the saved artifacts of the first build (data loss). Evidence: scratchpad build2.log of otgw-dev 02:37-02:45, heapab-nodelay0 empty afterwards.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test reproduces (1): a build/ that already holds the versioned names makes rename_build_artifacts() raise on the old code and pass on the fix (the new artifact replaces the old one, or the old one is moved aside with a clear message)
- [ ] #2 A host test reproduces (2): a non-empty subdirectory of build/ (e.g. build/keep/x.bin) is deleted by the old consolidation code and kept by the fix; empty subdirectories are still removed
- [ ] #3 When the initial clean of build/ fails, the build reports which files were left and does not mix them with the new artifacts (no stale versioned file survives as if it were new)
- [ ] #4 evaluate.py --quick no new FAIL; one esp32-combo build via build.bat shows the normal SUCCESS lines
<!-- AC:END -->
