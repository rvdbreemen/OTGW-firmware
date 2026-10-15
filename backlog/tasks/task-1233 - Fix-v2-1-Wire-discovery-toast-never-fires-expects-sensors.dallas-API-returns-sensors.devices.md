---
id: TASK-1233
title: >-
  Fix: v2 1-Wire discovery toast never fires (expects sensors.dallas[], API
  returns sensors.devices{})
status: To Do
assignee: []
created_date: '2026-10-15 05:10'
labels:
  - bug
  - webui
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 371000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the gap audit (docs/research/2026-10-15-classic-vs-v2-gap-audit.md): v2.js:3810 reads sensors.dallas[] while the API returns sensors.devices{}, so the new-sensor toast never appears. Code reading only, not run.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced: with a Dallas sensor attached, the old v2 shows no discovery toast (bench or captured API response fed to the code)
- [ ] #2 After the fix the toast appears for a newly seen sensor and not for known ones
<!-- AC:END -->
