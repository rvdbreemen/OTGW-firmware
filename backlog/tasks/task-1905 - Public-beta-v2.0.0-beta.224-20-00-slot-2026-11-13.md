---
id: TASK-1905
title: Public beta v2.0.0-beta.224 (20:00 slot 2026-11-13)
status: To Do
assignee: []
created_date: '2026-11-13T18:09:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 969000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert typed /beta-release-2x. Docs-only release commit on the soaked beta.224 (5ddcc6543; src unchanged up to origin/dev 479e49391). Notes bundle beta.219-beta.224.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes, CHANGELOG rollover and README on dev, claims checked against code
- [ ] #2 Release build from the release commit; 2x Successfully created ESP32S3 image
- [ ] #3 Bench check of the release build on the Classic+S3 Mini bench unit (.63): fwversion, bootcount +2 only, no crashlog, settings unchanged
- [ ] #4 Tag v2.0.0-beta.224 on the release commit, GitHub prerelease --latest=false with assets and SHA256SUMS, download hash matches
- [ ] #5 #alpha-testing zip post + #beta-testing announcement, ledger row
<!-- AC:END -->
