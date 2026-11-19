---
id: TASK-2044
title: Public beta v2.0.0-beta.382 (2026-11-19 08:00 slot)
status: To Do
assignee: []
created_date: '2026-11-19T05:50:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1049000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release tracking for the 08:00 public beta slot of 2026-11-19 (/beta-release-2x, standing permission; coordinator GO). Candidate beta.382 (firmware e562f826e, origin/dev 79b9e308e). Covers beta.375-382 since public beta.374: TASK-2025, TASK-1890, TASK-1887, TASK-2018, TASK-1889, TASK-2039, TASK-2040, TASK-2036. Release commit docs-only (notes, CHANGELOG rollover, README); the release build gets its own 1 h soak on .63 (no waiver, coordinator 2026-11-19).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit (notes RELEASE_NOTES_2.0.0-beta.382.md, CHANGELOG [2.0.0-beta.382], README) pushed to dev
- [ ] #2 Release build from the release commit soaked 1 h on .63 by otgw-bench-tester: bootcount stable, MQTT connected, no crashlog, heap no collapse
- [ ] #3 Tag v2.0.0-beta.382 on the release commit; GitHub prerelease with --latest=false and the 6 assets + SHA256SUMS; download sha256 check; latest stays the 1.x stable
- [ ] #4 #beta-testing announcement (all changes since beta.374) and #alpha-testing note posted and read back
<!-- AC:END -->
