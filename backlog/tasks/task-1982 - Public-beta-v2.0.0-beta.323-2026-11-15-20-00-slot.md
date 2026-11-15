---
id: TASK-1982
title: Public beta v2.0.0-beta.323 (2026-11-15 20:00 slot)
status: To Do
assignee: []
created_date: '2026-11-15T17:06:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1012000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tracking task for the public beta v2.0.0-beta.323 in the 20:00 slot of 2026-11-15, run by the beta-release-2x skill on Robert's standing permission. Release commit composed by session 92 on origin/dev b28955bc8 (beta.323). Previous public beta: v2.0.0-beta.294 (TASK-1962).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes and CHANGELOG cross-checked: the 26 TASK ids in src/scripts commits v2.0.0-beta.294..b28955bc8 are in the CHANGELOG section, exclusions named with a reason
- [ ] #2 Release commit pushed and the tag v2.0.0-beta.323 on that commit
- [ ] #3 esp32-combo build of the release commit: 'Successfully created ESP32S3 image' and 'Build completed successfully' in its log
- [ ] #4 1 h soak on .63: bootcount = flash + 2, no crashlog, heap stable
- [ ] #5 GitHub release drafted then published with --prerelease --latest=false; asset sizes equal build/; SHA256 of a downloaded asset matches
- [ ] #6 #beta-testing post (all parts) posted and read back
- [ ] #7 #alpha-testing note posted
- [ ] #8 gruijter told that the capture now collects device state, graph history and diag recording (TASK-1964)
<!-- AC:END -->
