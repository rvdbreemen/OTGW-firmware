---
id: TASK-1962
title: Public beta v2.0.0-beta.294 (10:00 slot 2026-11-15)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T08:40:00Z'
updated_date: '2026-11-15T08:43:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1003000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Slot release of beta.294+67e3f51 (soaked 1 h on .63, PASS). Docs-only release commit: notes (by 92), CHANGELOG rollover, README. Standing permission (Robert 2026-11-14).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Candidate soaked >= 1 h on .63 (bootcount flash+2, no crashlog, MQTT)
- [ ] #2 Release commit built, bench check on .63 OK
- [ ] #3 Tag + GitHub prerelease (--latest=false), assets sha match
- [ ] #4 #alpha-testing note and #beta-testing announcement posted
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1: 09 soak .63 beta.294+67e3f51 08:37-09:37, 60 samples 0 failed, bootcount 135 (=flash+2) constant, crashlog none, MQTT connected every sample, heap_min_free 33320->32920 (boot->60 min), max_alloc 31732 flat.
<!-- SECTION:NOTES:END -->
