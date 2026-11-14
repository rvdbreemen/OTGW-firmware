---
id: TASK-1923
title: Public beta v2.0.0-beta.237 (08:00 slot 2026-11-14)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T07:06:00Z'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 985000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Publish the soaked beta.237+c66c9b9 as public prerelease v2.0.0-beta.237 via /beta-release-2x: docs-only release commit (notes bundling beta.225-237, CHANGELOG rollover, README), build, bench on .63, tag, draft, publish, #alpha-testing zip, #beta-testing announcement.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes, CHANGELOG rollover and README committed (docs-only)
- [ ] #2 esp32-combo build of the release commit OK with fresh artifacts
- [ ] #3 Bench OTA on .63: fwversion, settings diff none, no new crash, bootcount stable 5 min
- [ ] #4 Tag v2.0.0-beta.237 on the release commit, prerelease published with --latest=false, assets SHA match
- [ ] #5 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->
