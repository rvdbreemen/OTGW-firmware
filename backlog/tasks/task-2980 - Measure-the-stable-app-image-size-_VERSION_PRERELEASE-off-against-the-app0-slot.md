---
id: TASK-2980
title: >-
  Measure the stable app image size (_VERSION_PRERELEASE off) against the app0
  slot
status: To Do
assignee: []
created_date: '2026-11-20T17:04:00Z'
updated_date: '2026-11-20T17:13:00Z'
labels:
  - build
  - flash-size
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1112000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1955 and the flash-consumers measurement of beta.404 (architect, 2026-11-20). The 93.9 % app-slot figure (2,153,776 of 2,293,760 B) is a prerelease build. A stable build comments out _VERSION_PRERELEASE, which drops the prerelease-only code (guards in OLED.ino, handleDebug.ino, restAPI.ino, OTGW-firmware.ino). Measure the stable image once so the budget for the 2.0.0 stable is known, not guessed (guess 1-5 KB smaller). Build only through build.bat / ./build.sh in a throwaway worktree; nothing is committed or published.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Stable-configuration esp32-combo image built from the current dev HEAD with _VERSION_PRERELEASE commented out; its .ino.bin size and the TASK-1993 build-log line (percent of app0) recorded with the githash
- [ ] #2 Difference against the prerelease build of the same commit stated in bytes; TASK-1955 notes updated with the stable headroom
<!-- AC:END -->
