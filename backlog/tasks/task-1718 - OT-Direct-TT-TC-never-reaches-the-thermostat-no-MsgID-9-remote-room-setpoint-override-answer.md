---
id: TASK-1718
title: >-
  OT-Direct TT=/TC= never reaches the thermostat: no MsgID 9 (remote room
  setpoint override) answer
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-08T08:13:00Z'
updated_date: '2026-11-08T08:17:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Reproduce first: host replay through the real OT-Direct gateway path (pattern of tests/test_otd_gateway_own_status.py, TASK-1701): TT=20, then a thermostat READ_DATA MsgID 9 -> old code forwards it to the boiler (no 20.0 answer) = red.
Fix (OTDirect.ino, thermostat-request path next to the SR table ~2093, mirrors gateway.asm:2932-2944 MessageID9 and 3269-3299): while otRemoteOverride.mode != NONE, a thermostat READ_DATA for MsgID 9 is answered READ_ACK with otRemoteOverride.f88Value, and a READ_DATA for MsgID 100 with the flag bits (TT 0x0002 / TC 0x0001); after TT=0 (clearRemoteOverride) the next MsgID 9 read is answered once with 0 (PIC OverrideClr), then MsgID 9 goes to the boiler again. Keep the existing MsgID 16 write to the boiler (TrSet, unchanged behaviour); the MsgID 100 write to the boiler is pointless but harmless -- leave it, note it.
Check OpenTherm v4.2 MsgID 9 (TrOverride, R) and 100 (RemoteOverrideFunction, R) semantics before coding; Celcia MsgID 4 special case out of scope (note).
Verification: host test red->green; regression: test_otd_ch_override, test_otd_gateway_own_status, OT-Direct suites; evaluate --quick; bench .61 has no thermostat (monitor/loopback can't prove it) -> AC3 field (tjfs).
<!-- SECTION:PLAN:END -->
