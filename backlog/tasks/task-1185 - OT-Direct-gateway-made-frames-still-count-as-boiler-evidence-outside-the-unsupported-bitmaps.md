---
id: TASK-1185
title: >-
  OT-Direct: gateway-made frames still count as boiler evidence outside the
  unsupported bitmaps
status: To Do
assignee: []
created_date: '2026-09-30 14:32'
labels:
  - otdirect
  - bug
dependencies: []
priority: low
ordinal: 308000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1086 (found by its adversarial review, same class, outside that task's ACs). TASK-1086 tags every OT-Direct 'A' and every loopback 'B' as a local answer (OTFRAME_SRC_OTDIRECT_LOCAL) and keeps them out of boilerUnsupportedRead/Write. Four other consumers still read those frames as boiler evidence:
1. boilerAckedRead/boilerAckedWrite take gateway-made Acks (the master-mode WRITE_ACK echo, SR= READ_ACK answers, loopback B), which show as 'acknowledged' in the /api/v2/otgw/ot-support Boiler column.
2. The OT log prints '(boiler does not implement)' on synthesized type-7 A lines (processOT, the masterslave==1 && type-7 suffix); a bench tester can read that as a boiler verdict.
3. satNotifyBoilerFrameSeen() still fires on loopback B frames.
4. On the PIC path, frames replayed from /otgw_simulation.log (B and A) still count as boiler evidence.
Maintainer question (TASK-1086 review item 3): verdicts written by older builds persist in /ot-boiler.json and reload at boot; only a genuine boiler B Ack retracts them, so a unit that ran master or loopback mode on an older build keeps false 'unsupported' entries. Decide whether a one-time migration or a reset path is wanted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each of the four consumers either ignores locally made frames (the TASK-1086 bLocalAnswer mark) or is documented as deliberately counting them, with a host proof for every change
- [ ] #2 The maintainer decision on stale /ot-boiler.json verdicts is recorded, and implemented if a migration is chosen
<!-- AC:END -->
