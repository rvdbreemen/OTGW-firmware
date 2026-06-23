---
id: TASK-907
title: >-
  refactor(build): relocate archive to ../OTGW-build-archive as one zip per
  build
status: To Do
assignee: []
created_date: '2026-06-23 16:49'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Move per-build archive out of the repo to ../OTGW-build-archive (sibling), and store each build as a single <semver>.zip instead of a folder of loose files. Migrate the existing 37 in-repo build-archive/<semver>/ folders to zips, remove the in-repo dir, drop the now-stale gitignore entry. Both lines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ARCHIVE_DIR points to ../OTGW-build-archive (outside repo)
- [ ] #2 each build produces a single <semver>.zip, not a folder
- [ ] #3 existing 37 builds migrated; in-repo build-archive removed
<!-- AC:END -->
