---
id: TASK-1718
title: >-
  OT-Direct TT=/TC= never reaches the thermostat: no MsgID 9 (remote room
  setpoint override) answer
status: To Do
assignee: []
created_date: '2026-11-08T08:13:00Z'
labels:
  - bug
  - otdirect
  - field-report
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 827000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs 2026-11-08 08:02 #alpha-testing (msg 1568898605186551859): in OT-Direct gateway mode TT=20 'appeared to work but the boiler didn't start'. Cause in code: setRemoteOverride() (OTDirect.ino:2291) only writes MsgID 16 (TrSet) to the boiler plus MsgID 100 flags; nothing answers the thermostat's MsgID 9 read. The PIC does exactly that: gateway.asm:2932-2944 'If a remote override setpoint has been specified, return it to the thermostat' (MessageID9 turns the thermostat's read into an ack carrying setpoint1/2). Without it the thermostat never learns the new setpoint, so it never raises CH demand; MsgID 16 to the boiler does not start heating. Check OT spec v4.2 for MsgID 9 (TrOverride) and the Celcia special case (MsgID 4) before fixing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test (replay): with TT=20 active in gateway mode, the thermostat's MsgID 9 READ is answered with 20.0 (red on old code: forwarded/unanswered)
- [ ] #2 TT=0 / thermostat program change clears it as on the PIC (MsgID 100 flags, gateway.asm:3269-3299)
- [ ] #3 Field or bench: thermostat shows the override and raises CH demand (tjfs)
<!-- AC:END -->
