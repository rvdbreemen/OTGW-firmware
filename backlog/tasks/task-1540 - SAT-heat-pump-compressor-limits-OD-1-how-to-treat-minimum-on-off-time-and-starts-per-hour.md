---
id: TASK-1540
title: >-
  SAT heat-pump compressor limits (OD-1): how to treat minimum on/off time and
  starts per hour
status: To Do
assignee: []
created_date: '2026-11-01T08:15:00Z'
updated_date: '2026-11-04T21:24:00Z'
labels:
  - sat
  - next-release
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 696000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Open decision OD-1 from docs/sat/sat-commissioning-profiles-2026-11-01.md (TASK-1412.17). Options: A) only record the limits in the commissioning record and block the heat-pump claim when unknown; B) make them SAT settings that SAT enforces (SAT behaviour, plan to Sergeant D first). Robert 2026-11-01: leave this task for now, mark next release.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision taken by Robert (A or B) and recorded in the task notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: decision task, deferred by Robert to the next release.

2026-11-04 Robert (interactive): moved to milestone 2.1.0, out of the 2.0.0 scope.

2026-11-04 Robert (interactive, asked again): leave for 2.1.0, no decision now. SAT keeps the fixed heat-pump 600 s minimum ON / 600 s minimum OFF (entries 197/198, TASK-1542).
<!-- SECTION:NOTES:END -->
