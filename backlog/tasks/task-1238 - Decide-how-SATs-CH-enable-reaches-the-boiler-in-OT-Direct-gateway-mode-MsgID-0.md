---
id: TASK-1238
title: >-
  Decide how SAT's CH-enable reaches the boiler in OT-Direct gateway mode (MsgID
  0)
status: To Do
assignee: []
created_date: '2026-10-15 15:47'
labels:
  - sat
  - otdirect
  - adr
dependencies: []
ordinal: 376000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Moved out of ADR-179's open questions (TASK-1237). In gateway mode the gateway schedules its own MsgID 0 alongside the thermostat's (OTDirect.ino buildStatusRequest; scheduler excludes only monitor mode), and applyOverrides returns early for non-WRITE_DATA frames, so the thermostat's READ_DATA MsgID 0 is relayed unmodified. Each frame carries a different CH-enable bit, so SAT's CH=0 is not reliably honoured. Options: (A) suppress the gateway's own MsgID 0 while a live thermostat is present: SAT's CH=0 then has no carrier, the cold cutoff relies on CS=10 alone, and the MsgID-0-based bus-offline detection loses its probe. (B) apply SAT's CH bit to the relayed READ_DATA frame: new relay semantics that change what the thermostat's boiler sees; needs its own ADR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench capture in OT-Direct gateway mode with a live thermostat shows which CH-enable bit the boiler acts on
- [ ] #2 Maintainer decision A or B (or neither) recorded in an ADR
<!-- AC:END -->
