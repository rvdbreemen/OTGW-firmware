---
id: TASK-1845
title: 'OT-Direct: the DHW push (HW=P) differs from the PIC'
status: To Do
assignee: []
created_date: '2026-11-11T23:57:00Z'
updated_date: '2026-11-12T00:11:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 924000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N11), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: HW=P leaves stored HW state alone (G:4410-4412); any other HW= aborts the push (G:4398-4399); DHW enable forced only when the boiler does not take the push (MsgID 99 unsupported/Unknown/acked without push bit, G:3229-3238, :3273-3279); once started the push ends when the boiler's DHW mode bit clears, before that 8 status answers with flame off end it (G:2783-2797). tests/dhw-5.stc: with boiler support and HW=0 status stays R00000000.
- OT-Direct: HW=P sets otDHWOverride=1 (O:3202-3203); push end sets it to auto (O:1733, :1745) -> a previous HW=0 is gone; DHW enable always forced during a push (O:910-915); 30 s wall-clock timeout also cuts a started push (O:1728-1735); only HW=0 aborts (O:3218-3225); thermostat's own MsgID 99 not handled.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 HW=0, HW=P, push ends: PR=W answers W=0 during and after, bit 1 = 0 afterwards; host test red on current code
- [ ] #2 a started push is not cut at 30 s
- [ ] #3 DHW enable forced only when the boiler refuses or lacks the push
- [ ] #4 bench capture of one push
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): on the PIC MH=/MW=/M2= also set dhwpushpending (PushOperModes G:3281-3288), which looks like it can start a spoofed push after a plain mode command; read from code only, check before copying. Depends on N2.

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
