---
id: TASK-1594
title: >-
  build.py from a worktree loads the main tree's config.py: artifacts land in
  the main tree's build/, merged images and flash zip pick up stale firmware
  while the build reports success
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T19:07:00Z'
updated_date: '2026-11-03T19:08:00Z'
labels:
  - tooling
  - build
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 739000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-03 building beta.58/59 in wt-1552 with the main tree's embedded Python (.build-python). python312._pth names D:\...\OTGW-firmware and makes Python skip the script folder on sys.path, so 'import config' loaded the main tree's config.py; config.BUILD_DIR pointed at the main tree. Rename/collect/archive wrote the fresh .ino.bin/.littlefs.bin/.elf there; create_merged_binary (project_dir/build) fell back to sorted(...)[0] of a stale beta.18 .ino.bin and the flash zip was skipped, yet 'Build completed successfully' was printed. Same trap as noted in TASK-1553.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py imports the config.py next to itself, also when another tree is first on sys.path; host test red on the old build.py, green on the fix
- [ ] #2 A worktree build with the main tree's embedded Python puts every artifact (ino.bin, littlefs.bin, elf, merged, flash zip) in that worktree's build/ with the HEAD githash
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix: build.py inserts its own folder at sys.path[0] before 'import config'. tests/test_build_config_path.py: OLD build.py loads decoy/config.py (AssertionError), FIX OK. test_build 13, safeboot 11, staging 16, jobs, lock OK. Follow-up risk noted, not changed: create_merged_binary falls back to sorted(matches)[0] of any version when the semver pattern finds nothing.
<!-- SECTION:NOTES:END -->
