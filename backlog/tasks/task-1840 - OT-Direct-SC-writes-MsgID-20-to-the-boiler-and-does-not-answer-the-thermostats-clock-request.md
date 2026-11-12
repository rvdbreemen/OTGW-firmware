---
id: TASK-1840
title: >-
  OT-Direct: SC= writes MsgID 20 to the boiler and does not answer the
  thermostat's clock request
status: To Do
assignee: []
created_date: '2026-11-11T23:51:00Z'
updated_date: '2026-11-12T00:10:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 921000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N8), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: SC= only stores the time (G:4331-4388); used to answer the thermostat's MsgID 20 READ/WRITE for one expiry window (MessageID20 G:3048-3056); after that a boiler Unknown/Data-Invalid becomes Data-Invalid for READ, WRITE_ACK for WRITE (G:2694-2706). tests/thermostat-2.stc: A50147532, AC0147532, later A5014892A, A60140000. Nothing written to the boiler.
- OT-Direct: enqueueWriteCommand(20,...) WRITEs the boiler (O:3181-3191); its "override" does not exist (O:2433-2437); the thermostat gets the boiler's answer (O:1548-1582).
- Consequence: with "send time" on the firmware sends SC= every minute (networkStuff.ino:1447): boiler clock written every minute, thermostat never sees the time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SC= puts no frame on the bus; host test red on current code
- [ ] #2 while SC valid, thermostat READ/WRITE of MsgID 20 answered with the SC value; after expiry Data-Invalid/WRITE_ACK as the PIC, also once blacklisted
- [ ] #3 bench capture with a thermostat that reads MsgID 20, or the slave simulator
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): expiry window length (see N4).

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
