---
id: TASK-2919
title: Public beta v2.0.0-beta.392 (2026-11-19 20:00 slot)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T14:36:00Z'
updated_date: '2026-11-19T14:40:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1057000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta from the release commit on origin/dev (candidate beta.392 = 391 SAT TASK-2913 entries 428-430, 392 UI TASK-2914, build.py TASK-2917). Docs-only release commit: RELEASE_NOTES_2.0.0-beta.392.md, CHANGELOG rollover, README. Build esp32-combo, 60 min soak on .63 by 09, tag on the release commit, draft prerelease --latest=false, publish at 20:00, sha256 of the download, #beta-testing announcement (3 parts) + SAT field-run call, #alpha-testing note. Coordinator go 2026-11-19 15:2x; Robert's standing permission for the 2.x beta slots (2026-11-14). Gate for Sunday's stable: 2 clean SAT field runs on this beta (Robert A, DM 1572941358451265618).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit (docs only) on origin/dev with notes, CHANGELOG [2.0.0-beta.392] and README
- [ ] #2 esp32-combo build of the release commit: SUCCESS lines, fresh artifacts; flash zip listing shows sat_log_to_csv.py (TASK-2917 AC1)
- [ ] #3 60 min soak on .63 PASS by 09 (bootcount = flash + 2, no crashlog, heap flat)
- [ ] #4 Tag v2.0.0-beta.392 on the release commit; GitHub prerelease published with --latest=false; latest stays the 1.x stable; sha256 of the downloaded zip = SHA256SUMS
- [ ] #5 #beta-testing announcement (all changes since beta.382) + field-run call, #alpha-testing note; all read back
<!-- AC:END -->
