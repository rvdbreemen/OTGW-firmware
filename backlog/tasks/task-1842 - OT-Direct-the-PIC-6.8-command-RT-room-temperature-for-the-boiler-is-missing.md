---
id: TASK-1842
title: >-
  OT-Direct: the PIC 6.8 command RT= (room temperature for the boiler) is
  missing
status: To Do
assignee: []
created_date: '2026-11-11T23:53:00Z'
updated_date: '2026-11-12T00:11:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 922000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N9), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC 6.8: RT=<temp> stores a fake room temperature, 0 clears (G:3849-3856, :4300-4309, :372-374, :1288). Thermostat MsgID 24 gets that value (MessageID24 G:3061-3068); as alternative (AA=24) it goes out as WRITE (G:2670-2673). tests/heatpump-1.stc RT=25.41 -> R10181969. Reply `RT: 25.41`.
- OT-Direct: no RT branch -> NG (O:3911-3915); MsgID 24 has no override slot (O:434-442); MQTT room temperature path writes MsgID 24 as a gateway frame (O:2269-2273).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 RT=25.41 replies `RT: 25.41`; a thermostat WRITE 24 reaches the boiler with 0x1969; RT=0 stops it; host test red on current code
- [ ] #2 thermostat gets WRITE_ACK with its own data
- [ ] #3 OTD_PIC_COMPAT_VERSION (O:533) goes to 6.8 once this and the MsgID 125 part of N10 have landed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): for a thermostat frame with parity bit 1 the PIC appears to answer Unknown-DataID (setbyte1 G:2680-2686, SendAltResponse G:2467-2477); read from code only. OT-Direct chose not to copy this for 56/57 (O:1482-1488).

Part of the PIC 6.8 parity audit, parent TASK-1851.
<!-- SECTION:NOTES:END -->
