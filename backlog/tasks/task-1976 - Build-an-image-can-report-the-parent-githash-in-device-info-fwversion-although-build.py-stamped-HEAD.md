---
id: TASK-1976
title: >-
  Build: an image can report the parent githash in device/info fwversion
  although build.py stamped HEAD
status: To Do
assignee: []
created_date: '2026-11-15T15:02:00Z'
labels:
  - build
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on the TASK-1931 bench build (2026-11-15): GET /api/v2/device/info fwversion = 2.0.0-beta.318+1233ba9 (the parent commit), while the zip, the .ino.bin name and /version.hash on LittleFS say 56fb6db. The build came from the committed HEAD: reflog commit 15:15:36, amend 15:17:31, BUILD START after; build log (ba scratchpad build-1931.log) line 36 'autoinc-semver.py ... --githash 56fb6db', line 57 'Building version: 2.0.0-beta.318+56fb6db', lines 306 and 870 '[version] Updating version.h (hash=56fb6db)'. The committed version.h held 1233ba9: bin/bump-prerelease.sh stamps the hash of HEAD at bump time, which during a cherry-pick restack is the parent. Other builds today (309+ada14c6, 314) reported the right githash. Open: why the compiled string kept the committed stamp although version.h was rewritten (compile before the restamp? a cached object? which pio step reads it), and whether every build after a restack shows it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: a restack (cherry-pick -n + bump + commit) then build.bat; device/info fwversion vs the zip githash
- [ ] #2 Find the cause in build.py / autoinc-semver.py / the pio pre-scripts with evidence (log order, object timestamps)
- [ ] #3 Fix so fwversion always carries the HEAD the build ran on; a host or build check that fails when they differ
<!-- AC:END -->
