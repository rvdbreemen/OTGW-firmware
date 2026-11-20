---
id: TASK-2972
title: >-
  HA duplicates: which duplicate entities carry different values (HA review H4,
  question only)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:55:00Z'
labels:
  - mqtt
  - ha
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1104000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2970 HA review (docs/reviews/2026-11-20_ux-ha-entities/README.md, H4); umbrella TASK-2970. Several entities look like the same quantity: Room Temperature (MsgID 24) vs SAT Room Temp vs SAT BLE Temp; Water Pressure In CH Circuit vs SAT CH Pressure; Flame (binary) vs SAT Flame Status. Coordinator decision 2026-11-20: first only the question, no renaming in 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 otgw-dev records per pair (room temperature x3, CH pressure x2, flame x2) whether the entities can carry different values, with file:line of their sources
- [ ] #2 The answer is linked back to TASK-2970 and to the 2.1.0 naming task, which uses it for the role-based names
<!-- AC:END -->
