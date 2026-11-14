---
id: TASK-1921
title: >-
  Tooling: every esp32-combo build is cold; find which build.py step removes the
  .pio objects
status: To Do
assignee: []
created_date: '2026-11-14T04:36:00Z'
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
