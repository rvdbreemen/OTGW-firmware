---
id: TASK-906
title: 'feat(build): archive each build''s artifacts to build-archive/<semver>/'
status: To Do
assignee: []
created_date: '2026-06-23 16:33'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build/ is wiped every build (create_build_directory rmtree), so prior artifacts are lost. config.py already defines ARCHIVE_DIR (build-archive) but build.py never used it. Implement archive_build_artifacts(): after a successful build, copy the finalized build/ artifacts (.ino.bin, .littlefs.bin, .elf) to ARCHIVE_DIR/<semver>/ (persistent, gitignored). Default ON, --no-archive opt-out.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each build copies its finalized artifacts to build-archive/<semver>/
- [ ] #2 build-archive/ is gitignored and never wiped
- [ ] #3 --no-archive skips archiving
<!-- AC:END -->
