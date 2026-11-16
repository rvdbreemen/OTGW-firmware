---
id: TASK-2003
title: Public beta v2.0.0-beta.350 (2026-11-16 20:00 slot)
status: To Do
assignee: []
created_date: '2026-11-16T13:19:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1025000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta for the 20:00 slot via /beta-release-2x (Robert's standing permission 2026-11-14). Candidate beta.350 (9011c3c48, soak on .63 PASS 13:14-14:15), or beta.351 with TASK-1942 if its soak passes before 19:00 (coordinator). Release commit docs-only: notes for beta.340-350 with per item host/bench evidence, CHANGELOG rollover (lines to add for TASK-1804 A6, 1887, 1888, 1983), README. Composed by comms session 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit pushed at T-75 with notes listing every change since beta.339, on a soaked candidate
- [ ] #2 Release build SUCCESS, sha256 recorded, 1 h soak on .63 without restart
- [ ] #3 Tag on the release commit, draft then publish with --prerelease --latest=false, releases/latest stays v1.7.9, download sha256 equals the build
- [ ] #4 #beta-testing post with every change since beta.339 (numbered parts < 2000), read back; #alpha-testing note
<!-- AC:END -->
