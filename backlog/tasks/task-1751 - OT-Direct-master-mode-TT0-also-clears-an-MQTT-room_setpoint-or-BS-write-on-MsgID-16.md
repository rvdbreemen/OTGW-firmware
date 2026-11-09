---
id: TASK-1751
title: >-
  OT-Direct master mode: TT=0 also clears an MQTT room_setpoint or BS= write on
  MsgID 16
status: To Do
assignee: []
created_date: '2026-11-09T09:29:00Z'
labels:
  - otdirect
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 853000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). In OTD_MODE_MASTER, clearRemoteOverride() still calls clearWriteOverride(16) and (100) (OTDirect.ino ~2389-2391), which resets the MsgID 16 write cache (W_ENTRY(16), schedule ~164) and the shared slot-16 override, so TT=0 also drops an MQTT room_setpoint or BS= value. Older behaviour kept by ADR-223 for master mode; decide whether TT=0 should clear only what TT= set.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 TT=0 in master mode leaves an MQTT room_setpoint / BS= value on MsgID 16 untouched (or Robert decides the current behaviour stays, recorded in the notes); host test
<!-- AC:END -->
