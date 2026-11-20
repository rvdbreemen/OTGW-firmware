---
id: TASK-2033
title: Public beta v2.0.0-beta.374 (2026-11-18 20:00 slot)
status: To Do
assignee: []
created_date: '2026-11-18T17:34:00Z'
labels:
  - release
dependencies: []
priority: high
ordinal: 1039000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta through /beta-release-2x on the release commit on top of beta.374 (1d124dbc5). Covers beta.361 to beta.374 (beta.366 never landed; beta.375 TASK-2025 not in, its bench OK came after the release commit). Notes docs/releases/RELEASE_NOTES_2.0.0-beta.374.md; CHANGELOG rollover with the TASK-1991/TASK-2007 lines moved out of the released beta.360 section; README links to 374. Timeline (coordinator): release commit ~18:55, build ~19:00-19:10, 1 h soak on .63 19:12-20:12, publish ~20:15 with the full asset check; skip to 08:00 on a soak failure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit pushed; build of that commit made with build.bat (esp32-combo), artifacts checked
- [ ] #2 1 h soak on .63 passes every item of the decision list (bootcount, no crash, MQTT, SAT, min free heap stated, no unresponsive period, no loop stall, settings unchanged)
- [ ] #3 Tag on the release commit, draft prerelease with --latest=false, assets verified (sizes, sha256 of the downloaded zip), published
- [ ] #4 #beta-testing announcement and #alpha-testing note posted and read back
<!-- AC:END -->
