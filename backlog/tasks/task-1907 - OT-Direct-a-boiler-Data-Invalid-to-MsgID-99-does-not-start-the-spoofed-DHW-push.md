---
id: TASK-1907
title: >-
  OT-Direct: a boiler Data-Invalid to MsgID 99 does not start the spoofed DHW
  push
status: To Do
assignee: []
created_date: '2026-11-13T18:50:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 973000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1851 delta parity check (2026-11-13, read-only, verified in both sources). PIC 6.8: in MessageID99's type jump table the Data-Invalid entry is a nop that falls through into ID99Unknown (gateway.asm:3218-3230), so a Data-Invalid answer is handled like Unknown-DataID: while a manual push is set (manualdhwpush) the gateway sets dhwpushspoof and forces DHW enable. OT-Direct: otdPushOnBoiler99() is only called in the success branch of handleMasterResponse (OTDirect.ino:1865); the boilerDataInvalid branch only bridges the frame to the parser (:1875-1877), and otdPushOnBoiler99 also returns for type 6 (:2685-2687). Effect after HW=P against a boiler that answers MsgID 99 with Data-Invalid: the push stays pending for good, DHW is never enabled, a new HW=P does nothing, and MH/MW/M2 keep sending the push bit. Basis: code reading; no PIC .stc test covers it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real handleMasterResponse and otdPushOnBoiler99 code: HW=P, then a boiler Data-Invalid to the WRITE 99 gives otPushSpoof = true and bit 1 set in the relayed MsgID 0; red on the current code, green on the fix
- [ ] #2 Bench or replay: HW=P against a Data-Invalid MsgID 99 answer enables DHW once, as the PIC does
<!-- AC:END -->
