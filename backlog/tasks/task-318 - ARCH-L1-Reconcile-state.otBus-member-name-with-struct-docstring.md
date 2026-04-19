---
id: TASK-318
title: '[ARCH-L1] Reconcile state.otBus member name with struct docstring'
status: To Do
assignee: []
created_date: '2026-04-18 19:24'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-firmware.h:291 struct OTBusState carries comment 'state.otgw' but member is 'otBus' (56 call sites). Breaks sister-section naming convention (otd, sat, pic, mqtt are short).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Pick one: either rename member back to otgw OR update struct docstring to state.otBus
- [ ] #2 All call sites and docs consistent with the chosen name
- [ ] #3 No behavior change
<!-- AC:END -->
