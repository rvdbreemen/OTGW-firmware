---
id: TASK-2011
title: Public beta v2.0.0-beta.360 (2026-11-17 08:00 slot)
status: To Do
assignee: []
created_date: '2026-11-17T05:15:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1030000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta for the 08:00 slot via /beta-release-2x. Candidate beta.360 = af92cd8cc (TASK-1887 band: restart below target + 0.1 after a 600 s minimum, fix of the 359 ADR-252 regression), alpha post 1572112371298865192, bench OK on .63. Release commit docs-only: RELEASE_NOTES_2.0.0-beta.360.md (beta.352-360), CHANGELOG rollover (lines added for TASK-1887, TASK-1989), README. Notes > 125000 B: the release body is the trimmed form of Phase 4.4. Composed by comms 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit pushed (coordinator go) with notes listing every change since beta.351, on the benched candidate
- [ ] #2 Release build SUCCESS, sha256 recorded, 1 h soak on .63 without restart (full hour before publish)
- [ ] #3 Tag on the release commit, draft then publish with --prerelease --latest=false, releases/latest stays v1.7.9, download sha256 equals the build
- [ ] #4 #beta-testing post with every change since beta.351 (numbered parts < 2000), read back; #alpha-testing note or zip post per the ledger
<!-- AC:END -->
