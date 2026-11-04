---
id: TASK-1233
title: >-
  Fix: v2 1-Wire discovery toast never fires (expects sensors.dallas[], API
  returns sensors.devices{})
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:10'
updated_date: '2026-10-15 14:06'
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
- [x] #1 Reproduced: with a Dallas sensor attached, the old v2 shows no discovery toast (bench or captured API response fed to the code)
- [x] #2 After the fix the toast appears for a newly seen sensor and not for known ones
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Old-vs-fix in tests/webui/dallas-discovery-toast.test.mjs: the real pollDallasDiscovery fed the sensors.devices{} shape from restAPI.ino:436-458 gives 0 cards on the old v2.js, 2 on the fix, and none for a known address. Bench: the unit has no 1-Wire sensor (devices absent), no toast (negative case).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 1-Wire discovery toast reads sensors.devices{} (was sensors.dallas[]), so new sensors are announced again. Evidence: node test old 0 / fix 2 cards.
<!-- SECTION:FINAL_SUMMARY:END -->
