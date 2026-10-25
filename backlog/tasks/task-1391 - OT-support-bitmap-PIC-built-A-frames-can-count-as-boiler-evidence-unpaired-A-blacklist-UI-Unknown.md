---
id: TASK-1391
title: >-
  OT support bitmap: PIC-built A frames can count as boiler evidence (unpaired
  A, blacklist/UI= Unknown)
status: To Do
assignee: []
created_date: '2026-10-25T13:44:00Z'
labels:
  - opentherm
  - pic
milestone: 2.0.0
dependencies: []
ordinal: 530000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the boiler-capability research 2026-10-25 (docs/research/2026-10-25-boiler-capability-discovery.md, M3; read in code only). B and A pair only with the same ID within 500 ms (OTGW-Core.ino:5326-5328); an unpaired A keeps bAnswerOverride false (:5336) and counts as boiler evidence (:5401): in T(57) R(Y) B(Y) A(57 ack) that A sets boilerAckedRead for 57 (:5402-5406), and a UI= or blacklist A(X Unknown) sets boilerUnsupported for X (:5438-5450). Only B frames are boiler truth. Needs a capture first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench or field capture shows (or refutes) a PIC-built A frame setting the boiler bitmap
- [ ] #2 If confirmed: only B frames set/retract boiler support bits (host test red on HEAD)
<!-- AC:END -->
