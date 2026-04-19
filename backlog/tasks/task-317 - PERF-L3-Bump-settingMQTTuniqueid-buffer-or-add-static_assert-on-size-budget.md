---
id: TASK-317
title: '[PERF-L3] Bump settingMQTTuniqueid buffer or add static_assert on size budget'
status: To Do
assignee: []
created_date: '2026-04-18 19:24'
labels:
  - performance
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
settingMQTTuniqueid[26] currently uses ~14 chars. New streaming composer builds long composite keys; a future change appending hostname into a 32-byte buffer would silently overflow.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Buffer size documented with a short comment explaining the format budget
- [ ] #2 Either bump to [33] at next settings schema bump, or add static_assert(sizeof(...) >= 20)
- [ ] #3 No change in saved settings format
<!-- AC:END -->
