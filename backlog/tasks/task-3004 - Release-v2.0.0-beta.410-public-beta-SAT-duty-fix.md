---
id: TASK-3004
title: 'Release v2.0.0-beta.410 (public beta: SAT duty fix)'
status: To Do
assignee: []
created_date: '2026-11-21T12:54:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1126000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta on Robert's order (#alpha-testing 1573682252607397969: Publish beta 410 now). Docs-only release commit on bce9496a5 + notes/CHANGELOG/README; tag, build, 1 h soak on .63, GitHub prerelease --latest=false, Discord posts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit pushed to dev
- [ ] #2 Release build soaked 1 h on .63 (bootcount, no crashlog, MQTT)
- [ ] #3 Tag + GitHub prerelease with verified assets (SHA256)
- [ ] #4 #alpha-testing note and #beta-testing post
<!-- AC:END -->
