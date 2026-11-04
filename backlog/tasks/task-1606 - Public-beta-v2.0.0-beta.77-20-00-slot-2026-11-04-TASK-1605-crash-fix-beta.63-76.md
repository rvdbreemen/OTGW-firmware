---
id: TASK-1606
title: >-
  Public beta v2.0.0-beta.77 (20:00 slot, 2026-11-04): TASK-1605 crash fix +
  beta.63-76
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T17:45:00Z'
updated_date: '2026-11-04T17:47:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 747000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release of dev beta.76 + release commit as v2.0.0-beta.77 via /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert's go recorded
- [x] #2 Release commit: bump beta.77, CHANGELOG rollover, release notes RELEASE_NOTES_2.0.0-beta.77.md
- [ ] #3 esp32-combo build green, OTA on .63, 60 min MQTT soak: no reboot, no new crash, settings unchanged
- [ ] #4 Tag v2.0.0-beta.77, GitHub prerelease (--latest=false), assets + SHA256SUMS verified by download
- [ ] #5 #alpha-testing zip post and #beta-testing announcement (shown to Robert first)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 ~18:45 Robert (interactive): 'Go, 20:00 met soak'.
<!-- SECTION:NOTES:END -->
