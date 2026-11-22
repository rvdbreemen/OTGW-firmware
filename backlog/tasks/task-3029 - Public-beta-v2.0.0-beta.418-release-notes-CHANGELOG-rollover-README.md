---
id: TASK-3029
title: 'Public beta v2.0.0-beta.418: release notes, CHANGELOG rollover, README'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-22T07:08:00Z'
updated_date: '2026-11-22T07:11:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1140000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-22 08:05 (coordinator session): 'Do Public beta release.' Candidate origin/dev e57a0b958, firmware 2.0.0-beta.418 (6a2412656). Previous public beta v2.0.0-beta.410. Release commit docs-only (normal slot release, no bump). Firmware tasks in range: TASK-3000 (411), TASK-2957 (412), TASK-2959 (413/414), TASK-3014 (415), TASK-3011 + TASK-3015 (416), TASK-3017 (417/418). Left out of the user-facing list (internal only): TASK-3020 (host test harness portability), bench scripts [no-task], the SAT decision register move (0c59f2db0, docs).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes file with a 'What changed since beta.410' section listing every user-visible change with TASK id, no em dash, OTGW32 not named
- [x] #2 CHANGELOG [Unreleased] rolled over to [2.0.0-beta.418] - 2026-11-22 with compare links
- [x] #3 README disclaimer and breaking-changes link point to beta.418
- [x] #4 #beta-testing post drafted (numbered parts < 2000 chars) and handed to the coordinator
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
BlueFlame 2026-11-22: notes file docs/releases/RELEASE_NOTES_2.0.0-beta.418.md (from beta.410's file; new top section + disclaimer bullet for 411-418; em-dash grep 0, TODO grep 0, OTGW32 not named), CHANGELOG Unreleased -> [2.0.0-beta.418] - 2026-11-22 with Added/Changed/Fixed lines for 3011/3015/3017/2957/2959/3014 plus the existing 3000 line, compare links updated; README disclaimer, breaking-changes link and BLE highlight updated.

Release commit a2cbf9da5 pushed (e57a0b958..a2cbf9da5). #beta-testing draft in OTGW-field-data/_comms/beta418_p1.txt (1683 chars) and beta418_p2.txt (935 chars, soak placeholder), handed to the coordinator; not posted by me. Publish/tag/soak are the coordinator's (skill Phase 3-6).
<!-- SECTION:NOTES:END -->
