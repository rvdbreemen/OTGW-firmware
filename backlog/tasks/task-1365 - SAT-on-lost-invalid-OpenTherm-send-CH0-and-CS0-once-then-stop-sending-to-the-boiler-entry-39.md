---
id: TASK-1365
title: >-
  SAT: on lost/invalid OpenTherm send CH=0 and CS=0 once, then stop sending to
  the boiler (entry 39)
status: To Do
assignee: []
created_date: '2026-10-24T21:41:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 500000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (entry 39): if the software has no valid OT, SAT stops: CH=0, CS=0 once, then no more messages to the boiler. Compare the firmware OT-offline handling (entry 39 cites it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On OT loss SAT sends CH=0 and CS=0 exactly once and nothing after (host test red/green)
<!-- AC:END -->
