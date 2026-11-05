---
id: TASK-1391
title: >-
  OT support bitmap: PIC-built A frames can count as boiler evidence (unpaired
  A, blacklist/UI= Unknown)
status: To Do
assignee: []
created_date: '2026-10-25T13:44:00Z'
updated_date: '2026-11-05T05:51:00Z'
labels:
  - opentherm
  - pic
milestone: 2.0.0
dependencies: []
priority: low
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: only a genuine B retracts (OTGW-Core.ino:5564/5576); unpaired PIC A can still set (:5530/5545). Needs a capture.

Bench .61 beta.87+2a9201f 2026-11-05 05:50-06:43 (sonnet agent, evidence scratchpad/bench-b87/, router exports b87-before/after, rules left 0): idle OT-Direct bench has no OT traffic: ot-support {"msgids":[]} at 05:55 and 06:40 (nothing to relearn). Under /api/v2/simulate/start builtin replay 06:40:53-06:42:40 the map filled with 14 ids (0,1,3,5,6,14,17,18,25,26,28,48,56,57; e.g. 0 tsR true, 1 tsW true). Real-traffic relearn not shown.
<!-- SECTION:NOTES:END -->
