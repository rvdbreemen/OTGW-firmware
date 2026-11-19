---
id: TASK-2929
title: >-
  Pre-stable: build and bench one production image (no _VERSION_PRERELEASE)
  before the 2.0.0 release, it has never been built
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T20:02:00Z'
updated_date: '2026-11-19T20:18:00Z'
labels:
  - release
  - build
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1063000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Pre-stable review 392..origin/dev (otgw-dev, 2026-11-19). origin has no release/2.x branch and no 2.x commit with _VERSION_PRERELEASE commented out (git log -S on version.h finds only the 1.x production commits), so every 2.x image built so far, also every public beta, is a prerelease build. 36 _VERSION_PRERELEASE sites in src/OTGW-firmware (networkStuff.ino 13, OTGW-firmware.ino 5, OLED.ino 5, Networktypes.h 4, restAPI.ino 3, handleDebug.ino 3, OTGW-firmware.h 2, version.h 1) have an #else or compiled-out path that has never been compiled or run. The first production compile would happen inside /release-2x on release day; a compile error or behaviour difference found there delays the release. Production differs: no beta AP fallback (WIFI_CONNECTING goes straight to restart after 10 failed attempts, networkStuff.ino ~1002-1023), no OLED AP page, no H/B bench keys. Betas never exercise the production WiFi path; with wifinoportal a prerelease keeps retrying forever (final else in the prerelease block) while production restarts after 10. The 393-398 diff itself is clean under this lens: every new prerelease block (heap-hold re-hold, B key, AP-fallback heap guard, help lines) is guarded at definition, prototype and call site, and the production fallback restForceStarved() == false is defined.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A scratch worktree on the release candidate with _VERSION_PRERELEASE commented out builds esp32-combo (build.bat exit 0, Build completed successfully); nothing is committed to dev
- [x] #2 On that .ino.bin the bench strings are absent: grep -c -a for 'REST starved-heap simulation', 'B) bench: REST gate', 'H) bench: starve the heap', 'heaphold:' all 0 (closes TASK-2921 AC1)
- [ ] #3 Bench (otgw-bench-tester, .63): the production image boots, reports a version without -beta, MQTT and REST work; a WiFi outage longer than 10 attempts ends in the clean restart 'WiFi: too many reconnect failures', not in an AP
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 production dry run (otgw-dev, coordinator's slot): wt-2026 detached on origin d6544e15e with only version.h:10 commented out (//#define _VERSION_PRERELEASE beta.398), not committed. build.bat exit 0, 'Build completed successfully!', image 2.0.0+d6544e1 (.ino.bin 2149776 B, 21:14). Compile: 0 warnings in src/OTGW-firmware (the only warning in the log is lto-wrapper 'using serial compilation of 21 LTRANS jobs'), 0 errors: the 36 #else / compiled-out paths compile. On the .ino.bin: 'REST starved-heap simulation' 0, 'B) bench: REST gate' 0, 'H) bench: starve the heap' 0, 'heaphold:' 0, 'BETA: WiFi' 0, '2.0.0-beta' 0 (dry run for TASK-2921 AC1; the final check stays on the real release build). Artifacts parked in RvdB/OTGW-worktree-leftovers/prod-dryrun-d6544e1/ (not for flashing: Robert chose 'B' (DM 1573067183825621095) = the AP fallback goes into stable, so this image has the old no-AP behaviour); tree restored (the build rewrote 57 version banners without -beta, reverted). AC3 changes with Robert's B: the production bench runs on the 400 dry run of the AP-fallback task (WiFi outage -> AP after 2 attempts, heap guard, no restart loop).
<!-- SECTION:NOTES:END -->
