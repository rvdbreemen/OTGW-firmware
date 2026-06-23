---
id: TASK-907
title: >-
  refactor(build): relocate archive to ../OTGW-build-archive as one zip per
  build
status: Done
assignee: []
created_date: '2026-06-23 16:49'
updated_date: '2026-06-23 16:56'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Move per-build archive out of the repo to ../OTGW-build-archive (sibling), and store each build as a single <semver>.zip instead of a folder of loose files. Migrate the existing 37 in-repo build-archive/<semver>/ folders to zips, remove the in-repo dir, drop the now-stale gitignore entry. Both lines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ARCHIVE_DIR points to ../OTGW-build-archive (outside repo)
- [x] #2 each build produces a single <semver>.zip, not a folder
- [x] #3 existing 37 builds migrated; in-repo build-archive removed
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ARCHIVE_DIR moved to PROJECT_DIR.parent/OTGW-build-archive (sibling, outside repo, shared across worktrees). archive_build_artifacts() now writes one <semver>.zip (ZIP_DEFLATED) of .bin/.elf/.zip instead of a folder. Migrated all existing builds (37 1.x + 2 dev = 39 zips, 300MB->122MB), removed both in-repo build-archive/ dirs, dropped stale build-archive/** gitignore. 1.x b97ba643, dev 2f37bedc, both pushed.
<!-- SECTION:FINAL_SUMMARY:END -->
