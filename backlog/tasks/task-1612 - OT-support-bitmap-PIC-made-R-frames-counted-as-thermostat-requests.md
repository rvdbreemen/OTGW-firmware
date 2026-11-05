---
id: TASK-1612
title: 'OT support bitmap: PIC-made R frames counted as thermostat requests'
status: To Do
assignee: []
created_date: '2026-11-04T21:28:00Z'
labels:
  - opentherm
  - pic
dependencies: []
priority: low
ordinal: 752000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while fixing TASK-1391. The master branch of the bitmap block (OTGW-Core.ino, masterslave == 0) counts every master frame into thermostatSentRead/Write, including R frames the PIC built itself (the R26 in T57 R26 B26 A57, or PIC poll requests). Only T frames are what the thermostat sends. Needs the same evidence path as TASK-1391 (capture + host test on the real block).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture shows (or refutes) PIC R frames setting thermostatSentRead/Write for ids the thermostat never sent
- [ ] #2 If confirmed: only T frames set the thermostat bitmaps (host test red on HEAD, green on fix)
<!-- AC:END -->
