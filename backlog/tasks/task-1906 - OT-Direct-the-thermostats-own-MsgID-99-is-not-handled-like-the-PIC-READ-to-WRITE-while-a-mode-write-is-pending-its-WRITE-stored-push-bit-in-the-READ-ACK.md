---
id: TASK-1906
title: >-
  OT-Direct: the thermostat's own MsgID 99 is not handled like the PIC (READ to
  WRITE while a mode write is pending, its WRITE stored, push bit in the
  READ-ACK)
status: To Do
assignee: []
created_date: '2026-11-13T18:48:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 972000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Orphan from the 2026-11-12 PIC 6.8 parity audit (REVIEW.md N11 'thermostat's own MsgID 99 not ...', explicitly left out of TASK-1845 with no AC); found again by the TASK-1851 delta check. PIC 6.8: a thermostat READ 99 is turned into a WRITE carrying the gateway's operating mode while opermodewrite is set (ID99ReadData, gateway.asm:3253-3257); a thermostat WRITE 99 is stored as the operating mode (ID99WriteData, :3267-3271); a boiler READ-ACK 99 gets the push bit set while the gateway spoofs a push (ID99ReadAck, :3259-3265). OT-Direct: by grep, MsgID 99 code covers only the gateway's own writes (OTDirect.ino:2677, :3616-3646) and the boiler reply hook (:1865); no code in the relay path rewrites or stores a thermostat MsgID 99 (grep-based, the relay path not traced line by line).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: with an MW=/MH=/M2= or HW=P write pending, a relayed thermostat READ 99 goes to the boiler as WRITE 99 with the gateway's mode; red on the current code
- [ ] #2 Host test: a thermostat WRITE 99 updates the stored operating mode used by later MW/MH/M2 writes
- [ ] #3 Host test: during a spoofed push the READ-ACK 99 answered to the thermostat has the push bit set
<!-- AC:END -->
