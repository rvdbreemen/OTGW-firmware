---
id: TASK-906
title: 'feat(build): archive each build''s artifacts to build-archive/<semver>/'
status: Done
assignee: []
created_date: '2026-06-23 16:33'
updated_date: '2026-06-23 16:44'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build/ is wiped every build (create_build_directory rmtree), so prior artifacts are lost. config.py already defines ARCHIVE_DIR (build-archive) but build.py never used it. Implement archive_build_artifacts(): after a successful build, copy the finalized build/ artifacts (.ino.bin, .littlefs.bin, .elf) to ARCHIVE_DIR/<semver>/ (persistent, gitignored). Default ON, --no-archive opt-out.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each build copies its finalized artifacts to build-archive/<semver>/
- [x] #2 build-archive/ is gitignored and never wiped
- [x] #3 --no-archive skips archiving
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Archive feature already implemented on otgw-1.x.x (commit a6368892: archive_build_artifacts copies each build's .bin/.elf to build-archive/<semver>/; 37 builds present incl beta.34 with .elf). Reason user could not see it: build.bat was broken (Python detection), so build.py never ran from build.bat. This turn: build.bat fixed (f6a226f1), added --no-archive opt-out (24ce6343). build-archive/** already gitignored. Ported the full feature to dev/2.0.0 (5c1961a2).
<!-- SECTION:FINAL_SUMMARY:END -->
