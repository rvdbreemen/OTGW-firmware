---
id: TASK-1611
title: >-
  OT support bitmap on 1.x: only boiler B frames are evidence (port of
  TASK-1391)
status: To Do
assignee: []
created_date: '2026-11-04T21:27:00Z'
labels:
  - opentherm
  - pic
  - 1.x
dependencies: []
priority: low
ordinal: 751000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x sibling of TASK-1391. otgw-1.x.x OTGW-Core.ino:4489-4495 has the same slave-frame branch ('proxy A with no preceding B still counts, per ADR-075'), so a PIC-built A (T57 R26 B26 A57, or a PIC Unknown-DataID answer) marks boiler support bits the boiler never gave. Cross-line work: master plan first (CLAUDE.md cross-worktree rule). Evidence and primary sources in TASK-1391 notes (gruijter capture 20261030-092718; otgw-6.6 gateway.asm:2490-2496).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on 1.x compiles the real bitmap block: red on otgw-1.x.x HEAD for a PIC A after another B, green on the fix
- [ ] #2 1.x boiler support file format bumped so wrong bits from older builds are not loaded
<!-- AC:END -->
