---
id: TASK-1973
title: >-
  OT-Direct/PIC: readable text for manufacturer OEM fault codes and TSP/FHB
  entries (per-brand tables)
status: To Do
assignee: []
created_date: '2026-11-15T13:12:00Z'
labels:
  - github
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Remainder of TASK-1642 / GitHub #10 (STemplar). The generic part is on the 2.x line: ASF flags as six bit topics, RBP flags as four bit topics, OEMFaultCode as a number (OTGW-Core.ino:3202-3218, :2810-2819). Readable text for the OEM fault/diagnostic codes (MsgID 5 LB, MsgID 115) and the TSP/FHB entries (MsgID 10-13) is manufacturer-specific: the OpenTherm spec defines only the numbers. Needs a design first (feature design in #dev-sat-mqtt): which brands, the cited source per brand table, where the text appears (MQTT topic, web UI, HA attribute), and flash cost (the app slot is at 94 %, TASK-1955).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design agreed in #dev-sat-mqtt: brands, source per table, where the text appears, flash budget
- [ ] #2 First brand table implemented with its source cited and a host test on the real lookup
<!-- AC:END -->
