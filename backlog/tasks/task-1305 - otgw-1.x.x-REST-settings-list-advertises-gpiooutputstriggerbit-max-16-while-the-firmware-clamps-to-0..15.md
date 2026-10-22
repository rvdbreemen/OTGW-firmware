---
id: TASK-1305
title: >-
  otgw-1.x.x: REST settings list advertises gpiooutputstriggerbit max 16 while
  the firmware clamps to 0..15
status: To Do
assignee: []
created_date: '2026-10-22T20:28:00Z'
labels:
  - 1.x
  - rest
dependencies: []
priority: low
ordinal: 428000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1295 review (2026-10-22). On otgw-1.x.x restAPI.ino:1678 advertises 0..16 for gpiooutputstriggerbit while settingStuff.ino:685 clamps to 0..15 (webhooktriggerbit already says 15). The dev line fixed its copy in TASK-1295. One-line port in the 1.x worktree; the 1.x UI renders the field as a plain number input with min/max from this range.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 restAPI.ino on otgw-1.x.x advertises 0..15 for gpiooutputstriggerbit
<!-- AC:END -->
